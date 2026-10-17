# One-shot Scooby-Doo: Re-Masked build: portable tools -> pinned dependencies -> translate -> configure -> compile.
# Run from anywhere:  powershell -ExecutionPolicy Bypass -File <project>\tools\build-scooby.ps1
# Everything it downloads is the pinned set WiiCompiled build scripts (wiicompiled\buildtools) fetch (llvm-mingw, CMake,
# Ninja, SDL3, Dawn and the other Dependencies/ trees); nothing else is installed. Logs: <project>\build-logs\.
[CmdletBinding()]
param(
    [int]$Jobs = 0,
    [switch]$SkipTools,
    [switch]$SkipTranslate,
    [switch]$ConfigureOnly,
    # Levels whose logic modules get translated (comma list of .ghr names, e.g. MAINMENU,W1L1); 'all' = every level,
    # which a playable build needs. A short list only speeds up test builds.
    [string]$LogicLevels = 'all'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$root = Split-Path -Parent $PSScriptRoot            # <project>\tools -> <project>
$wc = Join-Path $root 'wiicompiled'
$toolchain = Join-Path $root 'toolchain'
$logs = Join-Path $root 'build-logs'
$project = 'projects/scooby/recomp.yml'
if ($Jobs -le 0) { $Jobs = [Environment]::ProcessorCount }
[IO.Directory]::CreateDirectory($logs) | Out-Null
. (Join-Path $wc 'buildtools\NativeBuildFlags.ps1')

function Set-Status([string]$Text) {
    $line = "{0:HH:mm:ss} {1}" -f (Get-Date), $Text
    Write-Host $line
    Add-Content -LiteralPath (Join-Path $logs 'status.txt') -Value $line
}

# Runs a native tool, keeps its output in build-logs\<name>.log and stops on a non-zero exit code.
function Invoke-Step([string]$Name, [string]$Exe, [string[]]$Arguments, [string]$WorkDir) {
    $log = Join-Path $logs "$Name.log"
    Set-Status "${Name}: started"
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'            # Windows PowerShell 5.1 turns native stderr into errors
    Push-Location $WorkDir
    try { & $Exe @Arguments 2>&1 | ForEach-Object { "$_" } | Out-File -LiteralPath $log -Encoding utf8 }
    finally { Pop-Location; $ErrorActionPreference = $previous }
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        Set-Status "${Name}: FAILED (exit $code), see $log"
        Get-Content -LiteralPath $log -Tail 40
        throw "$Name failed with exit code $code."
    }
    Set-Status "${Name}: ok"
}

Remove-Item -LiteralPath (Join-Path $logs 'status.txt') -ErrorAction SilentlyContinue
Set-Status "build started, project $root, $Jobs jobs"

# The translation input is your own disc: extract it to Disc\DATA (the launcher's --extract does it), main.dol comes from there.
$discDol = Join-Path $root 'Disc\DATA\sys\main.dol'
if (-not (Test-Path -LiteralPath (Join-Path $root 'game\main.dol')) -and (Test-Path -LiteralPath $discDol)) {
    New-Item -ItemType Directory -Force (Join-Path $root 'game') | Out-Null
    Copy-Item -LiteralPath $discDol -Destination (Join-Path $root 'game\main.dol')
}
foreach ($required in @((Join-Path $root 'game\main.dol'), (Join-Path $root 'Disc\DATA\files'), (Join-Path $wc 'projects\scooby\recomp.yml'))) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing $required" }
}

# 1. Toolchain and dependencies
if (-not $SkipTools) {
    if (-not (Test-Path -LiteralPath (Join-Path $toolchain 'llvm-mingw\bin\x86_64-w64-mingw32-clang++.exe'))) {
        Set-Status 'tools: downloading llvm-mingw, CMake and Ninja'
        & (Join-Path $wc 'buildtools\Prepare-PortableTools.ps1') -Destination $toolchain *>&1 |
            ForEach-Object { "$_" } | Out-File -LiteralPath (Join-Path $logs 'tools.log') -Encoding utf8
    }
    if (-not (Test-Path -LiteralPath (Join-Path $wc 'Dependencies\dawn_prebuilt\bin\webgpu_dawn.dll'))) {
        Set-Status 'dependencies: downloading the pinned source trees and Dawn'
        & (Join-Path $wc 'buildtools\Prepare-Dependencies.ps1') -Destination (Join-Path $wc 'Dependencies') *>&1 |
            ForEach-Object { "$_" } | Out-File -LiteralPath (Join-Path $logs 'dependencies.log') -Encoding utf8
    }
    foreach ($tool in @('llvm-mingw\bin\x86_64-w64-mingw32-clang++.exe', 'CMake\bin\cmake.exe', 'Ninja\ninja.exe')) {
        if (-not (Test-Path -LiteralPath (Join-Path $toolchain $tool))) { throw "Toolchain file missing after setup: $tool (see build-logs\tools.log)" }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $wc 'Dependencies\dawn_prebuilt\bin\webgpu_dawn.dll'))) {
        throw 'Dependencies are incomplete (see build-logs\dependencies.log)'
    }
    Set-Status 'tools and dependencies ready'
}

# 2. Translate (.NET 8 runtime; the translator is already built)
if (-not $SkipTranslate) {
    $dotnet = Get-Item (& (Join-Path $PSScriptRoot 'get-dotnet.ps1') | Select-Object -Last 1)  # portable .NET 8 SDK
    $env:DOTNET_ROLL_FORWARD = 'Major'
    $env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
    $translator = Join-Path $wc 'translator\src\Translator.Cli\bin\Release\net8.0\Translator.Cli.dll'
    if (-not (Test-Path -LiteralPath $translator)) {
        Invoke-Step 'translator-build' $dotnet.FullName @('build', (Join-Path $wc 'translator\src\Translator.Cli'), '-c', 'Release') $wc
    }
    # Logic modules: pre-relocated into game\main_logic.dol (text section 2 at 0x81800000), their functions
    # appended to the function map (see projects\scooby\tools\build_logic_overlay.py).
    $sp = Join-Path $wc 'projects\scooby'
    $overlayArgs = @('-I', (Join-Path $sp 'tools\build_logic_overlay.py'), (Join-Path $root 'Disc\DATA\files'),
        (Join-Path $root 'game\main.dol'), (Join-Path $root 'game\main_logic.dol'), (Join-Path $sp 'MAP_logic.txt'),
        (Join-Path $wc 'runtime\src\hle\scooby_logic_table.inc'))
    if ($LogicLevels -ne 'all') { $overlayArgs += @('--levels', $LogicLevels) }
    $python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) { throw 'Python 3 was not found on PATH (needed for the logic-module overlay step).' }
    Invoke-Step 'logic-overlay' $python.Source $overlayArgs $wc
    # main_logic.dol changes with the overlay; keep the manifest's sha256 pin in step with it.
    $yml = Join-Path $sp 'recomp.yml'
    $sha = (Get-FileHash (Join-Path $root 'game\main_logic.dol') -Algorithm SHA256).Hash.ToLower()
    [IO.File]::WriteAllText($yml, ([IO.File]::ReadAllText($yml) -replace '(?m)(path: \.\./game/main_logic\.dol[^\n]*\n\s*sha256: )[0-9a-f]{64}', "`${1}$sha"))
    # The exe ships without game data: hle\scooby_dol_loader.cpp rebuilds this image from the player's disc and
    # checks it against this hash.
    [IO.File]::WriteAllText((Join-Path $wc 'runtime\src\hle\scooby_logic_dol_sha256.inc'),
        "// AUTO-GENERATED by tools/build-scooby.ps1: SHA-256 of the game/main_logic.dol the translation was built from.`nconstexpr char kLogicDolSha256[] = `"$sha`";`n")
    Get-Content (Join-Path $sp 'MAP.txt'), (Join-Path $sp 'MAP_logic.txt') | Set-Content (Join-Path $sp 'MAP_all.txt') -Encoding ascii
    Invoke-Step 'translate' $dotnet.FullName @($translator, 'translate-recursive', '0x80011C1C', '--project', $project,
        '--outdir', 'generated/functions', '--output-metadata', 'generated/base_translation_output.json',
        '--production-source-bundle', 'generated/base_translation_sources.bin',
        '--no-function-files', '--prune-stale', '--threads', "$Jobs") $wc
    # No base mod manifest: emit-base-manifest needs a REL and this game is main.dol only (the runtime never reads it).
    Invoke-Step 'data-init' $dotnet.FullName @($translator, 'generate-data-init', '--project', $project) $wc
    Invoke-Step 'shards' $dotnet.FullName @($translator, 'emit-build-shards', '--project', $project,
        '--base-metadata', 'generated/base_translation_output.json', '--base-functions-dir', 'generated/functions',
        '--native-source-dir', 'runtime/src', '--out', 'generated/build_shards') $wc
}

# 3. Configure and compile with the portable toolchain on a scrubbed PATH
$env:PATH = Get-MkwToolchainPath $toolchain
$bin = Join-Path $toolchain 'llvm-mingw\bin'
$build = Join-Path $wc 'native-build'
$configure = Get-MkwNativeConfigureArguments `
    -SourceDirectory (Join-Path $wc 'runtime') -BuildDirectory $build `
    -Ninja (Join-Path $toolchain 'Ninja\ninja.exe') `
    -CCompiler (Join-Path $bin 'x86_64-w64-mingw32-clang.exe') `
    -CxxCompiler (Join-Path $bin 'x86_64-w64-mingw32-clang++.exe') `
    -ResourceCompiler (Join-Path $bin 'x86_64-w64-mingw32-windres.exe') `
    -DependenciesDirectory (Join-Path $wc 'Dependencies') `
    -AdditionalArguments @("-DMKW_TRANSLATED_COMPILE_JOBS=$Jobs")
$cmake = Join-Path $toolchain 'CMake\bin\cmake.exe'
Invoke-Step 'configure' $cmake $configure $wc
if ($ConfigureOnly) { Set-Status 'configure only: done'; exit 0 }
Invoke-Step 'compile' $cmake @('--build', $build, '--target', 'WiiCompiled', '--parallel', "$Jobs") $wc
Set-Status "DONE: $build\WiiCompiled.exe"
Get-Process 'Scooby-Doo Re-Masked' -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep -Seconds 1
Copy-Item -LiteralPath (Join-Path $build 'WiiCompiled.exe') -Destination (Join-Path $root 'Play\Scooby-Doo Re-Masked.exe') -Force
Set-Status 'copied to Play\Scooby-Doo Re-Masked.exe'

