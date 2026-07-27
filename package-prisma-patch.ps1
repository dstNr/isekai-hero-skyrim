# Pack the OPTIONAL PrismaUI skill-tree patch as its own mod-manager-ready 7z.
# This ships ONLY the web view (Data\PrismaUI\views\IsekaiHero\...). The base mod's
# DLL already contains the runtime auto-detect: with this patch AND the PrismaUI
# framework installed, the skill tree renders in the web view; without either, it
# falls back to the built-in ImGui tree. No separate DLL.
#
# Requirements the patch adds for the PLAYER (documented on the patch page):
#   - Prisma UI - Next-Gen Web UI Framework (SKSE)
#   - Media Keys Fix (SKSE)  [PrismaUI's own requirement]

# -Version overrides the archive name only, same as package.ps1 — for test builds
# that must not overwrite a released archive.
param([string]$Version = "0.6.0")

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$version = $Version

$stage = Join-Path $root "build\prisma-patch"
$dist = Join-Path $root "dist"
$archive = Join-Path $dist "IsekaiHero-PrismaUI-Patch-v$version.7z"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$viewDir = Join-Path $stage "PrismaUI\views\IsekaiHero"
New-Item -ItemType Directory -Force $viewDir | Out-Null
New-Item -ItemType Directory -Force $dist | Out-Null

# --- the web view ---
Copy-Item (Join-Path $root "prisma-patch\PrismaUI\views\IsekaiHero\index.html") $viewDir

# --- node icons the view references (icons/<name>.png) ---
$icons = Join-Path $viewDir "icons"
New-Item -ItemType Directory -Force $icons | Out-Null
Copy-Item (Join-Path $root "icons\*.png") $icons

# --- pack ---
if (Test-Path $archive) { Remove-Item $archive -Force }
& "C:\Program Files\7-Zip\7z.exe" a -t7z -mx=7 $archive (Join-Path $stage "*") | Select-Object -Last 2

Write-Host ""
Write-Host "PATCH_OK -> $archive"
