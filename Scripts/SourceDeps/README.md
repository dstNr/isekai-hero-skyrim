# SourceDeps — Third-Party Papyrus Sources (for compiling only)

`compile.ps1` adds this folder to the compiler's import path **before** the
vanilla sources, so the SKSE-extended scripts here override the base game ones.

These files are **git-ignored** (`Scripts/SourceDeps/*.psc`) — they are external
modder resources with their own licenses and must not be committed to this repo.
Each developer drops them in locally.

## What to put here

Copy the `.psc` **source** files (not the `.pex`) from each resource:

| Resource | Where to get it | Files needed (examples) |
|---|---|---|
| **SKSE64** | https://skse.silverlock.org (match your game version: SE 1.5.97 or AE 1.6.x) | The whole `Scripts/Source/*.psc` from the archive — incl. the SKSE versions of `Form.psc`, `Actor.psc`, `Game.psc` (these add `GetName()` etc.), plus `SKSE.psc`, `UI.psc`, `StringUtil.psc`, `Input.psc`, `Math.psc` … |
| **SkyUI SDK** | SkyUI on Nexus → "SkyUI SDK" / the modder-resource source scripts | `SKI_ConfigBase.psc`, `SKI_ConfigManager.psc`, `SKI_QuestBase.psc`, `SKI_PlayerLoadGameAlias.psc` and any `SKI_*` they reference |
| **UIExtensions** | "UIExtensions" by Expired on Nexus | `UIExtensions.psc`, `UIListMenu.psc`, `UIMenuBase.psc` (and any others they reference) |

After copying, run from the project root:

```powershell
./compile.ps1
```

All six Isekai scripts should then compile with `0 error(s)`.

> Tip: you only need the **source** `.psc` files for compiling. The actual mods
> (SKSE runtime, SkyUI, UIExtensions `.esp`) must be installed in your game for
> the mod to run.
