What was uploaded to Nexus as **0.6.0**.

A big feature release on top of 0.5.x. Save-safe: existing saves keep everything and
derive the new fields from their tier (co-save v10).

Archives: `IsekaiHero-v0.6.0.7z` (main mod) and `IsekaiHero-PrismaUI-Patch-v0.6.0.7z` (optional UI patch).

### Added

- **Custom blessing** — a fifth path: choose the starting gift, reward pace and skill-tree depth independently (NORMAL/HERO/ASCENDED). "The whole tree open, but a normal start and normal rewards" is a valid build; it self-balances through the System-Point economy.
- **Reboot button** in the System menu — re-pick your blessing on an existing character, no reinstall; milestones, skill tree and points are kept.
- **Reworked status menu** (PrismaUI patch) — a proper dashboard: tier header with Skill Tree / Storage icons, milestone/point tiles, attunements, and a scrolling titles list that stays on screen no matter how many you have.
- **Optional SkyrimNet integration (experimental, untested)** — with SkyrimNet installed, AI NPCs can react to your reincarnation and the deeds the System recognises. Not yet verified in a running game with SkyrimNet; completely inert without SkyrimNet, so safe for everyone else. Can be switched off in the ini.
- **Skyrim VR now loads** (experimental) — no more crash on SkyrimVR, and its forms resolve there. UI in VR needs the PrismaUI VR build. Still community-testing.

### Fixed

- **Character level no longer drops to 1 after loading.** (Skills, attributes, perks and gold were never affected — only the level number.)

### Internal

- No developer real name in the shipped DLL; the debug `.pdb` is not shipped.
