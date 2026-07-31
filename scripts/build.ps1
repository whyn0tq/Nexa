param(
    [string]$OutputDirectory = "dist"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot "build"
$distDir = if ([System.IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory
} else {
    Join-Path $projectRoot $OutputDirectory
}

cmake -S $projectRoot -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build $buildDir --parallel
cmake --install $buildDir --prefix $distDir
windeployqt --release --no-translations (Join-Path $distDir "Nexa.exe")

$qtBin = Split-Path (Get-Command qmake).Source
$ucrtRoot = Split-Path $qtBin
$msysRoot = Split-Path $ucrtRoot
$ldd = Join-Path $msysRoot "usr\bin\ldd.exe"

if (Test-Path $ldd) {
    $processed = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    $queue = [System.Collections.Generic.Queue[string]]::new()
    Get-ChildItem -Path $distDir -Recurse -File |
        Where-Object { $_.Extension -in ".exe", ".dll" } |
        ForEach-Object { $queue.Enqueue($_.FullName) }

    while ($queue.Count -gt 0) {
        $binary = $queue.Dequeue()
        if (-not $processed.Add($binary)) {
            continue
        }

        & $ldd $binary 2>$null | ForEach-Object {
            if ($_ -match "[\\/]ucrt64[\\/]bin[\\/](?<dll>[^ ]+\.dll)") {
                $dllName = $Matches["dll"]
                $source = Join-Path $qtBin $dllName
                $destination = Join-Path $distDir $dllName
                if ((Test-Path $source) -and -not (Test-Path $destination)) {
                    Copy-Item -LiteralPath $source -Destination $destination
                    $queue.Enqueue($destination)
                }
            }
        }
    }
}

Write-Host "Nexa is ready: $distDir\Nexa.exe"
