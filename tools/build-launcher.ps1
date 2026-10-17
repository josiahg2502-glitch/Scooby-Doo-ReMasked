# Builds the Scooby-Doo: Re-Masked launcher (launcher\) with the portable .NET 8 SDK in toolchain\dotnet.
# Output: launcher\publish\Scooby-Doo Re-Masked Launcher.exe (self-contained single file, no .NET install needed).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dotnet = Join-Path $root 'toolchain\dotnet\dotnet.exe'
if (-not (Test-Path -LiteralPath $dotnet)) { throw "Missing $dotnet (copy toolchain\dotnet from the SPM project or run dotnet-install)." }
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'; $env:DOTNET_NOLOGO = '1'; $env:DOTNET_ROOT = Split-Path $dotnet
& $dotnet publish (Join-Path $root 'launcher\ScoobyReMasked.Launcher.csproj') -c Release -r win-x64 --self-contained true `
    -p:PublishSingleFile=true -o (Join-Path $root 'launcher\publish')
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed ($LASTEXITCODE)" }
Write-Host "DONE: $root\launcher\publish\Scooby-Doo Re-Masked Launcher.exe"
