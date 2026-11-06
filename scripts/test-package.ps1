param(
    [string]$Directory = "$PSScriptRoot/../dist",
    [string]$Compiler = "ISCC.exe",
    [switch]$AllUsers
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$cmake = Get-Content (Join-Path $projectRoot "CMakeLists.txt") -Raw
if ($cmake -notmatch 'project\(Nexa VERSION (?<version>[0-9]+\.[0-9]+\.[0-9]+)') {
    throw "Missing release version"
}
$version = $Matches["version"]
$name = "Nexa-$version-windows-x64"
$directory = (Resolve-Path -LiteralPath $Directory).Path
$stage = Join-Path $directory $name
$installer = Join-Path $directory "$name-setup.exe"

foreach ($file in @((Join-Path $stage "Nexa.exe"), $installer)) {
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($file)
    if ($info.ProductName.Trim() -ne "Nexa" -or $info.ProductVersion.Trim() -ne $version) {
        throw "Incorrect package metadata: $file"
    }
}

$info = [Diagnostics.FileVersionInfo]::GetVersionInfo($installer)
if ($info.LegalCopyright.Trim().Replace('©', '(C)') -ne "Copyright (C) 2026 teesa (whyn0tq)") {
    throw "Incorrect installer copyright"
}

$archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $directory "$name.zip"))
try {
    foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File) {
        $relative = [IO.Path]::GetRelativePath($stage, $file.FullName).Replace("\", "/")
        $entry = $archive.GetEntry("$name/$relative")
        if (-not $entry -or $entry.Length -ne $file.Length) {
            throw "Missing or damaged portable file: $relative"
        }
        $stream = $entry.Open()
        try {
            if ((Get-FileHash -InputStream $stream).Hash -ne (Get-FileHash -LiteralPath $file.FullName).Hash) {
                throw "Portable file mismatch: $relative"
            }
        } finally {
            $stream.Dispose()
        }
    }
} finally {
    $archive.Dispose()
}

$testId = [Guid]::NewGuid().ToString('N')
$testRoot = Join-Path $projectRoot "build/package-test-$testId"
$registryRoot = "HKCU:"
$programs = [Environment]::GetFolderPath('Programs')
$desktopDir = [Environment]::GetFolderPath('DesktopDirectory')
$mode = '/CURRENTUSER'
if ($AllUsers) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Run all-user package tests from an administrator PowerShell"
    }
    $registryRoot = "HKLM:"
    $programs = [Environment]::GetFolderPath('CommonPrograms')
    $desktopDir = [Environment]::GetFolderPath('CommonDesktopDirectory')
    $mode = '/ALLUSERS'
}
$registry = "$registryRoot\Software\Microsoft\Windows\CurrentVersion\Uninstall\Nexa-test-${testId}_is1"
$installDir = Join-Path $testRoot "Installed Nexa"
$shortcutName = "Nexa Test $testId"
$shortcut = Join-Path $programs "$shortcutName\$shortcutName.lnk"
if (Test-Path -LiteralPath $shortcut) { throw "A Nexa Start menu shortcut already exists" }
$desktop = Join-Path $desktopDir "$shortcutName.lnk"
if (Test-Path -LiteralPath $desktop) { throw "A Nexa desktop shortcut already exists" }
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$settingsKey = "Software\Nexa-test-$([Guid]::NewGuid().ToString('N'))"
$dataDirs = @('LocalData', 'RoamingData', 'EditData', 'DropData') | ForEach-Object {
    Join-Path $testRoot $_
}
$compilerPath = (Get-Command $Compiler -ErrorAction Stop).Source
& $compilerPath "/DAppVersion=$version" "/DSourceDir=$stage" "/DOutputDir=$testRoot" `
    "/DPackageId=Nexa-test-$testId" "/DShortcutName=$shortcutName" `
    "/DSettingsKey=$settingsKey" "/DLocalData=$($dataDirs[0])" "/DRoamingData=$($dataDirs[1])" `
    "/DEditData=$($dataDirs[2])" "/DDropData=$($dataDirs[3])" `
    (Join-Path $projectRoot "installer/setup.iss") *> (Join-Path $testRoot "compile.log")
if ($LASTEXITCODE -ne 0) { throw "Test installer compilation failed: $testRoot\compile.log" }
$installer = Join-Path $testRoot "$name-setup.exe"
$uninstaller = Join-Path $installDir "unins000.exe"
$oldPath = $env:PATH
$oldPlatform = $env:QT_QPA_PLATFORM

function Invoke-Setup([string]$File, [string[]]$Arguments) {
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(60000)) {
        $process.Kill()
        throw "Timed out: $File"
    }
    if ($process.ExitCode -ne 0) { throw "Exit code $($process.ExitCode): $File" }
}

try {
    foreach ($language in "russian", "english", "german", "french") {
        $tasks = if ($language -eq "german") { '/TASKS=desktopicon' } else { '/TASKS=' }
        Invoke-Setup $installer @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-',
            $mode, "/LANG=$language", $tasks, "/DIR=`"$installDir`"",
            "/LOG=`"$testRoot\$language.log`"")
        $entry = Get-ItemProperty -LiteralPath $registry
        if ($entry.DisplayVersion -ne $version -or -not (Test-Path -LiteralPath $shortcut)) {
            throw "Installation registration or shortcut failed: $language"
        }
        if ($language -eq "german" -and -not (Test-Path -LiteralPath $desktop)) {
            throw "Desktop shortcut was not created"
        }
    }
    foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File) {
        $relative = [IO.Path]::GetRelativePath($stage, $file.FullName)
        $installed = Join-Path $installDir $relative
        if (-not (Test-Path -LiteralPath $installed) -or
            (Get-FileHash -LiteralPath $installed).Hash -ne (Get-FileHash -LiteralPath $file.FullName).Hash) {
            throw "Installed file mismatch: $relative"
        }
    }
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:QT_QPA_PLATFORM = "windows"
    foreach ($path in $stage, $installDir) {
        $snapshot = Join-Path $testRoot "$([IO.Path]::GetFileName($path)).png"
        Invoke-Setup (Join-Path $path "Nexa.exe") @('--settings', "`"$($dataDirs[0])`"",
            '--snapshot', "`"$snapshot`"")
        if (-not (Test-Path -LiteralPath $snapshot)) { throw "Application startup failed: $path" }
    }
    foreach ($path in @($installDir) + $dataDirs) {
        $nested = Join-Path $path "nested"
        New-Item -ItemType Directory -Path $nested -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $nested "data.txt") -Value "remove"
    }
    $settingsPath = "HKCU:\$settingsKey\Nexa"
    New-Item -Path $settingsPath -Force | Out-Null
    New-ItemProperty -LiteralPath $settingsPath -Name "hosts/v1" -Value "test servers" | Out-Null
    New-ItemProperty -LiteralPath $settingsPath -Name "language" -Value "ru" | Out-Null
    $outside = Join-Path $testRoot "outside.txt"
    Set-Content -LiteralPath $outside -Value "preserve"
    Invoke-Setup $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART',
        "/LOG=`"$testRoot\uninstall.log`"")
    for ($attempt = 0; $attempt -lt 50 -and (Test-Path -LiteralPath $installDir); ++$attempt) {
        Start-Sleep -Milliseconds 100
    }
    if ((Test-Path -LiteralPath $registry) -or (Test-Path -LiteralPath $shortcut) -or
        (Test-Path -LiteralPath $desktop) -or
        (Test-Path -LiteralPath "HKCU:\$settingsKey") -or
        (Test-Path -LiteralPath $installDir) -or
        @($dataDirs | Where-Object { Test-Path -LiteralPath $_ }).Count -gt 0 -or
        -not (Test-Path -LiteralPath $outside)) {
        throw "Uninstall left application data or removed an unrelated file"
    }
    Write-Host "Package tests passed: portable startup, installation, upgrade, four languages, hashes, shortcuts, complete uninstall."
    Write-Host "Test artifacts: $testRoot"
} finally {
    $env:PATH = $oldPath
    $env:QT_QPA_PLATFORM = $oldPlatform
    if (Test-Path -LiteralPath $registry) {
        if (Test-Path -LiteralPath $uninstaller) {
            Invoke-Setup $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART')
        }
    }
}
