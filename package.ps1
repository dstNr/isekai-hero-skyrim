# Pack the mod as a mod-manager-ready 7z (data-relative layout).
# Run after build.bat. The ESP is taken from the repo copy (plugin/), which is
# the ESL-flagged, FormID-compacted build — see the git history for how it was made.

# -Version overrides the archive name only (nothing inside the mod carries it).
# Use it for test builds — "0.6.1-soundfix" — so a released archive is never
# overwritten by a work-in-progress one of the same name.
param([string]$Version = "0.8.0")

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$version = $Version

$stage = Join-Path $root "build\package"
$dist = Join-Path $root "dist"
$archive = Join-Path $dist "IsekaiHero-v$version.7z"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
New-Item -ItemType Directory -Force $dist | Out-Null

# --- plugin (ESL-flagged .esp) ---
Copy-Item (Join-Path $root "plugin\IsekaiHero.esp") $stage

# --- SKSE plugin ---
# The .pdb is intentionally NOT shipped: it embeds the full source build paths
# (C:\Users\<real name>\...) in hundreds of records, which would make the author's
# real name public. Crash Logger then prints raw offsets for our frames instead of
# symbols — an acceptable trade for not leaking the name. The .pdb is still built
# and kept locally (build.bat deploys it to the game folder) for the author's own
# crash analysis.
$plugins = Join-Path $stage "SKSE\Plugins"
New-Item -ItemType Directory -Force $plugins | Out-Null
$stagedDll = Join-Path $plugins "IsekaiHeroSKSE.dll"
Copy-Item (Join-Path $root "build\IsekaiHeroSKSE.dll") $stagedDll

# --- privacy: scrub the developer's real name from the DLL ---
# The compiler bakes absolute source paths (__FILE__ / std::source_location, used by
# the logger and asserts) into the binary. Some come from our own sources, others
# from the CommonLibSSE static lib vcpkg built under the user folder — the latter are
# already compiled in, so no build flag can reach them. Both spell out
# C:\Users\<real name>\... . We overwrite the name in place with an equal-length
# neutral token, so byte offsets are untouched and the PE stays valid (Windows does
# not verify a DLL's PE checksum). Run on the STAGED copy only; the local build keeps
# its real paths for the author's own debugging.
# The name to scrub is derived from the machine at run time and never written down
# here. This file lives in git: hardcoding the string would publish exactly what the
# whole exercise exists to keep out of the world, and it would only work on one
# machine. ISEKAIHERO_SCRUB_NAME overrides it where the user folder is not the name
# (a build server, a renamed profile).
$secret = if ($env:ISEKAIHERO_SCRUB_NAME) {
    $env:ISEKAIHERO_SCRUB_NAME
} else {
    Split-Path $env:USERPROFILE -Leaf
}
if ([string]::IsNullOrWhiteSpace($secret) -or $secret.Length -lt 4) {
    throw "Privacy: cannot determine the name to scrub. Set ISEKAIHERO_SCRUB_NAME."
}
# Equal length, byte for byte, so offsets and the PE stay valid whatever the name is.
$cover = ("isekai-hero-" + ("x" * 128)).Substring(0, $secret.Length)
$bytes = [System.IO.File]::ReadAllBytes($stagedDll)
$find  = [System.Text.Encoding]::ASCII.GetBytes($secret)
$repl  = [System.Text.Encoding]::ASCII.GetBytes($cover)
$hits = 0
for ($i = 0; $i -le $bytes.Length - $find.Length; $i++) {
    $match = $true
    for ($j = 0; $j -lt $find.Length; $j++) {
        if ($bytes[$i + $j] -ne $find[$j]) { $match = $false; break }
    }
    if ($match) {
        [System.Array]::Copy($repl, 0, $bytes, $i, $repl.Length)
        $hits++
        $i += $find.Length - 1
    }
}
[System.IO.File]::WriteAllBytes($stagedDll, $bytes)
Write-Host "Name scrub: replaced $hits occurrence(s) in the DLL."

# --- licence ---
Copy-Item (Join-Path $root "LICENSE") $stage

# --- optional settings ini (ships with defaults; safe to delete in-game) ---
# Also the FOMOD's "Standard" preset, so a manual install and the recommended
# installer choice land on byte-identical files.
Copy-Item (Join-Path $root "IsekaiHero.ini") $plugins

# --- FOMOD installer, and the ini presets it chooses between ---
# Regenerated here rather than trusted from the repo: the presets are derived from
# IsekaiHero.ini, and shipping a stale copy would hand players settings that no longer
# match the file they were generated from.
& node (Join-Path $root "tools\make-fomod.mjs")
if ($LASTEXITCODE -ne 0) { throw "FOMOD generation failed." }
Copy-Item (Join-Path $root "fomod") $stage -Recurse
Copy-Item (Join-Path $root "fomod-presets") $stage -Recurse

# --- PrismaUI view, as an installer option rather than a separate download ---
# Only the HTML: the installer points the PrismaUI option at the icon folder staged
# above, so the 74 icons are carried once and installed to both places. That is nearly
# the whole size of what used to be two archives.
$prismaView = Join-Path $stage "PrismaUI\views\IsekaiHero"
New-Item -ItemType Directory -Force $prismaView | Out-Null
Copy-Item (Join-Path $root "prisma-patch\PrismaUI\views\IsekaiHero\index.html") $prismaView

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

# --- privacy gate: nothing leaves this folder with the author's name in it ---
# This used to check the DLL only, which is how v0.5.0 shipped a 130 MB .pdb with the
# name in thousands of records: the scrub ran, reported "clean", and never looked at the
# file next to it. The check now walks everything that is about to be packed, so it
# cannot be outgrown by adding a file to the archive.
$leaks = @()
foreach ($f in Get-ChildItem $stage -Recurse -File) {
    $b = [System.IO.File]::ReadAllBytes($f.FullName)
    for ($i = 0; $i -le $b.Length - $find.Length; $i++) {
        $match = $true
        for ($j = 0; $j -lt $find.Length; $j++) {
            if ($b[$i + $j] -ne $find[$j]) { $match = $false; break }
        }
        if ($match) { $leaks += $f.FullName; break }
    }
}
if ($leaks.Count -gt 0) {
    throw "Privacy gate: '$secret' is still present in:`n  " + ($leaks -join "`n  ")
}
Write-Host "Privacy gate: $((Get-ChildItem $stage -Recurse -File).Count) staged file(s) checked, all clean."

# --- pack ---
if (Test-Path $archive) { Remove-Item $archive -Force }
& "C:\Program Files\7-Zip\7z.exe" a -t7z -mx=7 $archive (Join-Path $stage "*") | Select-Object -Last 2

Write-Host ""
Write-Host "PACKAGE_OK -> $archive"
