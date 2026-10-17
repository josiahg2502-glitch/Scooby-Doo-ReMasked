# Assembles dist\ScoobyDooReMasked: the launcher exe and the game (Play\, runtime files only). No disc data.
#   powershell -ExecutionPolicy Bypass -File tools\package-scooby.ps1 [-Build] [-Zip]
# -Build first rebuilds the game (build-scooby.ps1 -SkipTools -LogicLevels all) and the launcher; -Zip also writes the .zip.
[CmdletBinding()] param([switch]$Build, [switch]$Zip)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$launcher = "$root\launcher\publish\Scooby-Doo Re-Masked Launcher.exe"
if ($Build) { & "$PSScriptRoot\build-scooby.ps1" -SkipTools -LogicLevels all; & "$PSScriptRoot\build-launcher.ps1" }
elseif (-not (Test-Path -LiteralPath $launcher)) { & "$PSScriptRoot\build-launcher.ps1" }
$dist = "$root\dist\ScoobyDooReMasked"
for ($try = 1; Test-Path -LiteralPath $dist; $try++) {  # Explorer/antivirus can briefly hold files: retry
    try { Remove-Item -LiteralPath $dist -Recurse -Force -ErrorAction Stop }
    catch { if ($try -ge 5) { throw }; Start-Sleep -Seconds 3 }
}
New-Item -ItemType Directory -Force "$dist\Play\UserData\mods", "$dist\Disc" | Out-Null
Copy-Item -LiteralPath $launcher -Destination $dist
# Runtime files only: no saves, logs, Config.toml (the game writes defaults), caches or dev captures.
foreach ($f in 'Scooby-Doo Re-Masked.exe', '*.dll', 'dsp_coef.bin', 'initial_pipeline_cache.db', 'portable.txt') {
    if (Test-Path "$root\Play\$f") { Copy-Item "$root\Play\$f" "$dist\Play" }
}
Copy-Item -LiteralPath "$PSScriptRoot\package-README.txt" -Destination "$dist\README.txt"
$size = [math]::Round((Get-ChildItem -LiteralPath $dist -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)
Write-Host "DONE: $dist ($size MB)"
if ($Zip) { Compress-Archive -LiteralPath $dist -DestinationPath "$root\dist\ScoobyDooReMasked.zip" -Force; Write-Host "DONE: $root\dist\ScoobyDooReMasked.zip" }
