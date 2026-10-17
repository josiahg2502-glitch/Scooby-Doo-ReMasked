# Assembles dist\ScoobyDooReMasked: one exe on top (the launcher, named "Scooby-Doo Re-Masked.exe"), README.txt, and
# data\ with the game's runtime files. No disc data.
#   powershell -ExecutionPolicy Bypass -File tools\package-scooby.ps1 [-Build] [-Zip]
# -Build first rebuilds the game (build-scooby.ps1 -SkipTools -LogicLevels all) and the launcher.
# -Zip also writes dist\ScoobyDooReMasked.zip from a clean copy (never the player's disc, saves or settings).
# Updating dist in place only replaces program files: data\Disc and data\UserData (saves, Config.toml, mods) are kept,
# so a disc set up there by the launcher survives a repackage. An old Play\ layout is moved into data\ first.
[CmdletBinding()] param([switch]$Build, [switch]$Zip)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$launcher = "$root\launcher\publish\Scooby-Doo Re-Masked Launcher.exe"
if ($Build) { & "$PSScriptRoot\build-scooby.ps1" -SkipTools -LogicLevels all; & "$PSScriptRoot\build-launcher.ps1" }
elseif (-not (Test-Path -LiteralPath $launcher)) { & "$PSScriptRoot\build-launcher.ps1" }

# Program files only: what a new download replaces.
function Stage([string]$to) {
    New-Item -ItemType Directory -Force "$to\data\UserData\mods" | Out-Null
    Copy-Item -LiteralPath $launcher -Destination "$to\Scooby-Doo Re-Masked.exe" -Force
    Copy-Item -LiteralPath "$PSScriptRoot\package-README.txt" -Destination "$to\README.txt" -Force
    foreach ($f in 'Scooby-Doo Re-Masked.exe', '*.dll', 'dsp_coef.bin', 'initial_pipeline_cache.db', 'portable.txt') {
        if (Test-Path "$root\Play\$f") { Copy-Item "$root\Play\$f" "$to\data" -Force }
    }
}

$dist = "$root\dist\ScoobyDooReMasked"
New-Item -ItemType Directory -Force $dist | Out-Null
# Old layout (Play\ + Disc\ + "...Launcher.exe"): keep the player's UserData and disc, drop the rest.
if (Test-Path "$dist\Play") {
    New-Item -ItemType Directory -Force "$dist\data" | Out-Null
    if ((Test-Path "$dist\Play\UserData") -and -not (Test-Path "$dist\data\UserData")) { Move-Item "$dist\Play\UserData" "$dist\data\UserData" }
    if ((Test-Path "$dist\Disc\DATA") -and -not (Test-Path "$dist\data\Disc\DATA")) {
        New-Item -ItemType Directory -Force "$dist\data\Disc" | Out-Null; Move-Item "$dist\Disc\DATA" "$dist\data\Disc\DATA"
    }
    # The launcher wrote dvd_root relative to Play\UserData; data\Disc sits one level closer.
    $cfg = "$dist\data\UserData\Config.toml"
    if (Test-Path $cfg) { (Get-Content $cfg) -replace "^dvd_root = '../../Disc/DATA'", "dvd_root = '../Disc/DATA'" | Set-Content $cfg }
    Remove-Item -Recurse -Force "$dist\Play", "$dist\Disc" -ErrorAction SilentlyContinue
    Remove-Item -Force "$dist\Scooby-Doo Re-Masked Launcher.exe" -ErrorAction SilentlyContinue
}
Stage $dist
$size = [math]::Round((Get-ChildItem -LiteralPath $dist -Recurse -File | ? FullName -notmatch '\\data\\(Disc|UserData)\\' | Measure-Object Length -Sum).Sum / 1MB)
Write-Host "DONE: $dist ($size MB of program files; data\Disc and data\UserData left as they were)"
if ($Zip) {
    $clean = Join-Path $env:TEMP "ScoobyDooReMasked-zip\ScoobyDooReMasked"
    Remove-Item -Recurse -Force (Split-Path $clean) -ErrorAction SilentlyContinue
    Stage $clean
    Compress-Archive -LiteralPath $clean -DestinationPath "$root\dist\ScoobyDooReMasked.zip" -Force
    Remove-Item -Recurse -Force (Split-Path $clean)
    Write-Host "DONE: $root\dist\ScoobyDooReMasked.zip"
}
