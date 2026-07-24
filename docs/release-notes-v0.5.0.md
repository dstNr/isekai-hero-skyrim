Was als **0.5.0** auf Nexus hochgeladen wurde.

Archive: `IsekaiHero-v0.5.0.7z` (Hauptmod) und `IsekaiHero-PrismaUI-Patch-v0.5.0.7z` (optionaler UI-Patch).
Save-safe: ein bestehender Spielstand behält Segen, Nodes und Punkte; die neuen Co-Save-Felder (v9) defaulten sauber.

### Added

- **Two new ways to receive the blessing.** After picking HERO or ASCENDED you now choose *how* it reaches you — **Full** grants the flat start as before, **Shattered** keeps only what the tier is worth over time (reward multiplier, deeper skill tree) and starts you at the same mortal floor as NORMAL. On top of that a fourth blessing, **DORMANT**, takes no tier at all: you begin as an ordinary mortal and the System wakes on its own, to HERO at level 25 and ASCENDED at level 80. Nothing arrives early, nothing stays sealed for good. Thresholds configurable.
- **Skill-tree respec.** Refunds the System Points spent on stat nodes and reverts their effects, with a confirm click. The four Omniscience unlocks and Perk Synthesis are excluded on purpose — the System cannot un-teach a shout you already know.
- **Repeatable utility nodes:** Fleet of Foot (move speed), Beast of Burden (carry weight), Enduring Vigor (Health/Magicka/Stamina).
- **Optional settings ini** (`Data/SKSE/Plugins/IsekaiHero.ini`): remappable System-menu hotkey, hide-sealed-nodes option, DORMANT thresholds.
- **Optional PrismaUI patch** — renders the whole UI as an HTML/CSS view instead of the built-in one. Auto-detected, safe fallback; only the patch needs PrismaUI.

### Fixed

- **The System hotkey no longer fires while a menu is open** — no more panel popping up while typing in an inventory search or the console.
