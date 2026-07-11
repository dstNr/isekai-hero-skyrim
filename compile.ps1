# compile.ps1
# Papyrus compile check for the Isekai Hero mod.
# Compiles every .psc in Scripts/Source against the Skyrim SE script sources
# and reports any errors. Run this BEFORE opening the Creation Kit so syntax
# and API errors are caught early.
#
# Usage:
#   ./compile.ps1                 # auto-detect Skyrim SE + compiler
#   ./compile.ps1 -SkyrimPath "E:\SteamLibrary\steamapps\common\Skyrim Special Edition"
#   ./compile.ps1 -Only IsekaiPowerScript   # compile a single script
#
# Requires: Skyrim Special Edition Creation Kit installed (provides
# PapyrusCompiler.exe, TESV_Papyrus_Flags.flg and the vanilla .psc sources).
# For IsekaiMCMScript you also need the SkyUI SDK sources (SKI_ConfigBase.psc).

[CmdletBinding()]
param(
    [string]$SkyrimPath,
    [string]$Only,
    [string[]]$ExtraImports = @(),
    # After a successful build, copy the .pex into the game's Data\Scripts so the
    # Creation Kit's "Add Script" list finds them. Use -NoDeploy to skip.
    [switch]$NoDeploy
)

$ErrorActionPreference = "Stop"
$projectRoot = $PSScriptRoot
$projectSource = Join-Path $projectRoot "Scripts\Source"
# Drop third-party modder-resource sources (SKSE / SkyUI SDK / UIExtensions)
# here to compile against them without polluting the game's Data folder.
$projectDeps = Join-Path $projectRoot "Scripts\SourceDeps"

function Find-SkyrimPath {
    if ($SkyrimPath) { return $SkyrimPath }

    # Known location on this machine first
    $known = "E:\SteamLibrary\steamapps\common\Skyrim Special Edition"
    if (Test-Path $known) { return $known }

    # Parse Steam libraryfolders.vdf and probe each library
    $vdf = "C:\Program Files (x86)\Steam\steamapps\libraryfolders.vdf"
    if (Test-Path $vdf) {
        $libs = Select-String -Path $vdf -Pattern '"path"\s+"([^"]+)"' |
            ForEach-Object { $_.Matches[0].Groups[1].Value -replace '\\\\', '\' }
        foreach ($lib in $libs) {
            $candidate = Join-Path $lib "steamapps\common\Skyrim Special Edition"
            if (Test-Path $candidate) { return $candidate }
        }
    }
    return $null
}

$skyrim = Find-SkyrimPath
if (-not $skyrim) {
    Write-Host "ERROR: Could not locate the Skyrim Special Edition folder." -ForegroundColor Red
    Write-Host "Pass it explicitly: ./compile.ps1 -SkyrimPath '<path to Skyrim Special Edition>'"
    exit 1
}
Write-Host "Skyrim SE:  $skyrim" -ForegroundColor Cyan

$compiler = Join-Path $skyrim "Papyrus Compiler\PapyrusCompiler.exe"
if (-not (Test-Path $compiler)) {
    Write-Host "ERROR: PapyrusCompiler.exe not found at:" -ForegroundColor Red
    Write-Host "  $compiler"
    Write-Host "The Skyrim Special Edition Creation Kit is not installed."
    Write-Host "Install it via Steam (free) - it adds the compiler and vanilla script sources."
    exit 1
}

# Skyrim SE uses Data\Source\Scripts; some setups use the older Data\Scripts\Source.
$vanillaSource = $null
foreach ($rel in @("Data\Source\Scripts", "Data\Scripts\Source")) {
    $p = Join-Path $skyrim $rel
    if (Test-Path (Join-Path $p "Game.psc")) { $vanillaSource = $p; break }
}
if (-not $vanillaSource) {
    Write-Host "ERROR: Vanilla script sources (Game.psc) not found under Data." -ForegroundColor Red
    Write-Host "Extract Scripts.zip from the Creation Kit into Data\Source\Scripts."
    exit 1
}
Write-Host "Sources:    $vanillaSource" -ForegroundColor Cyan

$flags = Join-Path $vanillaSource "TESV_Papyrus_Flags.flg"
if (-not (Test-Path $flags)) {
    Write-Host "ERROR: TESV_Papyrus_Flags.flg not found in $vanillaSource" -ForegroundColor Red
    exit 1
}

# Import path: project sources + project dependency sources + any extra dirs +
# vanilla sources. The first match wins, so project copies override vanilla.
$importList = @($projectSource)
if (Test-Path $projectDeps) {
    $importList += $projectDeps
    Write-Host "Deps:       $projectDeps" -ForegroundColor Cyan
}
$importList += $ExtraImports | Where-Object { $_ -and (Test-Path $_) }
$importList += $vanillaSource
$importPaths = $importList -join ";"

$outputDir = Join-Path $projectRoot "Scripts"
if (-not (Test-Path $outputDir)) { New-Item -ItemType Directory -Path $outputDir | Out-Null }

# Collect scripts to compile
$scripts = Get-ChildItem -Path $projectSource -Filter "*.psc" -File
if ($Only) {
    $scripts = $scripts | Where-Object { $_.BaseName -eq $Only }
    if (-not $scripts) { Write-Host "No script named '$Only' in $projectSource" -ForegroundColor Red; exit 1 }
}

Write-Host ""
Write-Host "Compiling $($scripts.Count) script(s)..." -ForegroundColor Cyan
Write-Host ("=" * 60)

$failed = @()
foreach ($s in $scripts) {
    Write-Host ""
    Write-Host ">> $($s.Name)" -ForegroundColor Yellow
    & $compiler $s.BaseName `
        -import="$importPaths" `
        -output="$outputDir" `
        -flags="TESV_Papyrus_Flags.flg" 2>&1 | ForEach-Object { Write-Host "   $_" }
    if ($LASTEXITCODE -ne 0) { $failed += $s.BaseName }
}

Write-Host ""
Write-Host ("=" * 60)
if ($failed.Count -ne 0) {
    Write-Host "FAILED: $($failed -join ', ')" -ForegroundColor Red
    exit 1
}

Write-Host "ALL SCRIPTS COMPILED SUCCESSFULLY" -ForegroundColor Green

# Deploy compiled .pex into the game so the Creation Kit sees them.
if (-not $NoDeploy) {
    $gameScripts = Join-Path $skyrim "Data\Scripts"
    if (-not (Test-Path $gameScripts)) { New-Item -ItemType Directory -Path $gameScripts -Force | Out-Null }
    $copied = 0
    foreach ($s in $scripts) {
        $pex = Join-Path $outputDir ($s.BaseName + ".pex")
        if (Test-Path $pex) { Copy-Item $pex -Destination $gameScripts -Force; $copied++ }
    }
    Write-Host "Deployed $copied .pex -> $gameScripts" -ForegroundColor Cyan
}
exit 0
