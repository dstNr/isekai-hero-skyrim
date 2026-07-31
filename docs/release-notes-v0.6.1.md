What was uploaded to Nexus as **0.6.1**.

A small follow-up to 0.6.0: a VR bug introduced by 0.6.0's own VR fix, a performance pass
on the PrismaUI menu, and save hygiene for the storage chest. No save format change.

Archives: `IsekaiHero-v0.6.1.7z` (main mod) and `IsekaiHero-PrismaUI-Patch-v0.6.1.7z` (optional UI patch).

### Fixed

- **The System hotkey did nothing in Skyrim VR.** 0.6.0's fix for the VR overlay crash skipped the whole `Overlay::Install` function in VR — including the input registration that makes the hotkey fire at all, which happened to sit after the early return. So VR loaded and ran, but the System menu never opened on keypress. Input registration now runs before the VR check; only the crash-prone swap-chain hook is still skipped in VR.
- **Dimensional Storage no longer leaves orphaned chest references behind.** When the storage's hidden container was rebuilt after a cell reset, the old reference was only disabled, so a long save could accumulate several dormant husks (save bloat, and the kind of "unattached" entries a save cleaner like ReSaver flags). Rebuilds now delete the old reference, and any husks left by earlier builds are swept on load.

### Changed

- **Reduced heavy CSS effects in the PrismaUI menu** (masks, stacked shadows, gradients) that PrismaUI's own performance notes flag as costly on its CPU renderer. The menu looks the same at a glance but repaints (opening a panel, hovering, the flourish) cost noticeably less, for steadier framerates.
