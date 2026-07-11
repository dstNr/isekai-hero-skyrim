# Isekai Hero — Skyrim SE (SKSE Plugin)

An Isekai / "reincarnated hero" system for Skyrim Special Edition, built as a
native **SKSE C++ plugin** using [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG).

> **The System has chosen you. Your new life begins.**

The plugin lets a reincarnated character choose an origin world, a power level,
skills, equipment and wealth, and rewards progression (quests, milestones) with
MMO-style effects — all driven from native code, no Creation Kit / ESP required.

---

## Status

🚧 **Early C++ rewrite.** The project was previously a Papyrus mod (see
[`papyrus/`](papyrus/) and the `papyrus-v1.0` git tag) and is being rebuilt as a
native SKSE plugin for nicer UI and effects. Right now the repo contains a
minimal, verified-building plugin skeleton.

## Requirements

- **Skyrim Special Edition** + **SKSE64**
- Runtime: works across SE / AE / VR (CommonLibSSE-NG multi-targeting)

## Building

Needs the C++ toolchain: **Visual Studio 2022 Build Tools** (MSVC + Windows SDK +
CMake + Ninja) and **vcpkg**. Then:

```powershell
./build.bat
```

This sets up the MSVC environment, runs CMake with the vcpkg toolchain (which
fetches/builds CommonLibSSE-NG on first run — slow once, cached after) and
produces `build/IsekaiHeroSKSE.dll`.

To test in-game, copy the DLL to `Data/SKSE/Plugins/` and launch via SKSE — it
logs to `Documents/My Games/Skyrim Special Edition/SKSE/IsekaiHeroSKSE.log`.

## Layout

```
├── CMakeLists.txt            # CommonLibSSE-NG plugin (add_commonlibsse_plugin)
├── vcpkg.json                # dependency: commonlibsse-ng
├── vcpkg-configuration.json  # vcpkg registries + baselines
├── build.bat                 # one-shot build (vcvars + cmake)
├── src/
│   ├── PCH.h                 # precompiled header (CommonLibSSE-NG setup)
│   └── main.cpp              # plugin entry point
└── papyrus/                  # ARCHIVED original Papyrus version (v1.0)
```

## The archived Papyrus version

The complete, working Papyrus implementation lives in [`papyrus/`](papyrus/) and
at the git tag **`papyrus-v1.0`** (`git checkout papyrus-v1.0`). It includes the
full System dialog flow, progression/milestones, perks, MCM, quest-reward
tracker and the Creation Kit guide.
