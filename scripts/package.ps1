param(
    [string]$DistDirectory = "dist-release",
    [string]$OutputDirectory = "dist",
    [switch]$Zip
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$resolve = {
    param([string]$path)
    if ([System.IO.Path]::IsPathRooted($path)) { $path } else { Join-Path $projectRoot $path }
}
$distDir = & $resolve $DistDirectory
$outputDir = & $resolve $OutputDirectory

$cmake = Get-Content (Join-Path $projectRoot "CMakeLists.txt") -Raw
if ($cmake -notmatch "project\(Nexa VERSION (?<version>[0-9.]+)") {
    throw "No version in CMakeLists.txt"
}
$version = $Matches["version"]
$name = "Nexa-$version-windows-x64"
$stage = Join-Path $outputDir $name

if (Test-Path $stage) {
    Remove-Item -Recurse -Force $stage
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -Path (Join-Path $distDir "*") -Destination $stage -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot "LICENSE") -Destination (Join-Path $stage "LICENSE.txt")
foreach ($notice in "THIRD_PARTY_NOTICES.md", "THIRD_PARTY_NOTICES.ru.md", "THIRD_PARTY_NOTICES.de.md") {
    Copy-Item -LiteralPath (Join-Path $projectRoot $notice) -Destination $stage
}

$qtBin = Split-Path (Get-Command qmake).Source
$msysRoot = Split-Path (Split-Path $qtBin)
$database = Join-Path $msysRoot "var\lib\pacman\local"

$owners = @{}
$packages = @{}
foreach ($package in Get-ChildItem -Path $database -Directory) {
    $files = Get-Content (Join-Path $package.FullName "files") |
        Where-Object { $_ -and $_ -ne "%FILES%" -and $_ -notmatch "^%" }
    $packages[$package.Name] = $files
    foreach ($file in $files) {
        if ($file -match "\.dll$") {
            $owners[$file.ToLowerInvariant()] = $package.Name
        }
    }
}

$used = [System.Collections.Generic.SortedSet[string]]::new()
Get-ChildItem -Path $stage -Recurse -File -Filter "*.dll" | ForEach-Object {
    $relative = $_.FullName.Substring($stage.Length + 1).Replace("\", "/").ToLowerInvariant()
    foreach ($candidate in "ucrt64/bin/$relative", "ucrt64/share/qt6/plugins/$relative") {
        if ($owners.ContainsKey($candidate)) {
            $used.Add($owners[$candidate]) | Out-Null
            break
        }
    }
}

$allLicenses = Get-ChildItem -Path (Join-Path $msysRoot "ucrt64\share\licenses") -Recurse -File
$licenses = Join-Path $stage "licenses"
foreach ($package in $used) {
    $short = $package -replace "^mingw-w64-ucrt-x86_64-", "" -replace "-[^-]+-[0-9]+$", ""
    $target = Join-Path $licenses $short
    New-Item -ItemType Directory -Force -Path $target | Out-Null

    $files = $packages[$package] | Where-Object { $_ -match "^ucrt64/share/licenses/.+[^/]$" }
    if (-not $files) {
        $files = $packages[$package] | Where-Object {
            $_ -match "(^|/)(COPYING|LICEN[CS]E)(\.[A-Za-z0-9]+)?$"
        }
    }
    foreach ($file in $files) {
        Copy-Item -LiteralPath (Join-Path $msysRoot $file) -Destination $target
    }

    if (-not (Get-ChildItem -Path $target -File)) {
        $desc = Get-Content (Join-Path $database "$package\desc")
        $at = [array]::IndexOf($desc, "%LICENSE%")
        $spdx = if ($at -ge 0) { $desc[$at + 1] -replace "^spdx:", "" } else { "" }
        $text = $allLicenses | Where-Object { $_.BaseName -eq $spdx } | Select-Object -First 1
        if (-not $text) {
            throw "No license text for $package"
        }
        Copy-Item -LiteralPath $text.FullName -Destination (Join-Path $target "COPYING")
    }
}

if ($Zip) {
    $archive = Join-Path $outputDir "$name.zip"
    if (Test-Path $archive) {
        Remove-Item -Force $archive
    }
    & "$env:SystemRoot\System32\tar.exe" -a -c -f $archive -C $outputDir $name
    if ($LASTEXITCODE -ne 0) {
        throw "tar failed"
    }
    Write-Host "Nexa package: $archive"
} else {
    Write-Host "Nexa package: $stage"
}
