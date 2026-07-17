# Pack the mod as a mod-manager-ready 7z (data-relative layout).
# Run after build.bat. The ESP is taken from the repo copy (plugin/), which is
# the ESL-flagged, FormID-compacted build — see the git history for how it was made.

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$version = "0.2.1"

$stage = Join-Path $root "build\package"
$dist = Join-Path $root "dist"
$archive = Join-Path $dist "IsekaiHero-v$version.7z"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
New-Item -ItemType Directory -Force $dist | Out-Null

# --- plugin (ESL-flagged .esp) ---
Copy-Item (Join-Path $root "plugin\IsekaiHero.esp") $stage

# --- SKSE plugin + symbols (symbols stay in during the testing phase: without
# them, Crash Logger prints raw addresses for our frames) ---
$plugins = Join-Path $stage "SKSE\Plugins"
New-Item -ItemType Directory -Force $plugins | Out-Null
Copy-Item (Join-Path $root "build\IsekaiHeroSKSE.dll") $plugins
Copy-Item (Join-Path $root "build\IsekaiHeroSKSE.pdb") $plugins

# --- panel icons ---
$icons = Join-Path $plugins "IsekaiHero\icons"
New-Item -ItemType Directory -Force $icons | Out-Null
Copy-Item (Join-Path $root "icons\*.png") $icons

# --- sounds (only the four the mod actually plays) ---
$sound = Join-Path $stage "Sound\fx\isekai"
New-Item -ItemType Directory -Force $sound | Out-Null
foreach ($wav in "Cinematic_6_1.wav", "Cinematic_7_2.wav", "Modern_2_2.wav", "Modern_5_2.wav") {
    Copy-Item (Join-Path $root "sounds\$wav") $sound
}

# --- pack ---
if (Test-Path $archive) { Remove-Item $archive -Force }
& "C:\Program Files\7-Zip\7z.exe" a -t7z -mx=7 $archive (Join-Path $stage "*") | Select-Object -Last 2

Write-Host ""
Write-Host "PACKAGE_OK -> $archive"
