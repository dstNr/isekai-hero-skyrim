What was uploaded to Nexus as **0.5.1**.

A small follow-up to 0.5.0 — both fixes are in the optional PrismaUI patch.
No save-format change; a 0.5.0 save runs unchanged.

Archives: `IsekaiHero-v0.5.1.7z` (main mod) and `IsekaiHero-PrismaUI-Patch-v0.5.1.7z` (optional UI patch).

### Fixed

- **The level-up sound is no longer missing under PrismaUI.** Closing a System panel briefly takes the game out of menu-pause, and the engine discarded the sting started in that same frame — it never reached the speakers. Sounds that follow a panel closing now play just past the unpause. (The built-in ImGui UI never pauses and was never affected.)
- **Higher, steadier FPS in the System menu (PrismaUI).** Two accents animated continuously — a sheen sweep under the headings and a pulsing ring on every affordable node — which made the web renderer repaint the whole view every frame. Both are now static; the menu looks the same at rest but lets the renderer idle, so the framerate holds.

### Internal (no player-facing effect)

- The developer's real name is no longer embedded in the shipped DLL, and the debug `.pdb` is no longer packaged.
