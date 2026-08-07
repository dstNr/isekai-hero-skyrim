# Nexus Mods — Changelog entries

What goes in the Nexus changelog field per version. **Keep it short: one line per
change, one sentence — what changes for the player, not why.** The why and the
technical detail live in `CHANGELOG.md`, the developer record. (0.5.0 and 0.4.0 below
are still in the older, longer style — that is how they already sit on Nexus.)

---

## 0.7.1

Fixed: **threat labels no longer hop around** over a moving enemy — the frame now sits over the head instead of dancing with the animation.

Added: **AutoStart** setting — set it to 0 and the System waits for your hotkey instead of booting itself, for alternate starts that begin somewhere the boot sequence does not belong.

Added: **TextSpeed** setting — speed the System's typewriter up, or set it to 0 to have panels appear instantly.

Added: **clicking a panel skips the rest of its text**, in both the built-in and the PrismaUI interface.

---

## 0.7.0

Added: **System Quests** — the System hands you a kill objective on its own schedule, announces it, counts your kills on screen and pays System Points when it is done.

Added: **Threat labels** — a target frame over each enemy with its level, health and a colour-coded TRIVIAL/MANAGEABLE/DANGEROUS/LETHAL verdict; shown for what you aim at and whatever is fighting you.

Added: **The ten System potions** are now buyable in the System Shop — four instant Restoratives and six one-hour Elixirs.

Added: **Shop categories** — Materials, Wealth, Restoratives and Elixirs on their own shelves instead of one long grid.

Added: **Five new skill-tree nodes** (Storm Ward, Warded Mind, Arcane Absorption, Iron Skin, Rapid Recovery) and **mastery tiers** — every utility stat now caps at 10 ranks across five named tiers.

Added: **System Rank** (E through S) with its own insignia in the status panel.

Added: **In-game self-test** that runs after every load and writes a PASS/FAIL report to the log, so a bug report only needs the log file.

Fixed: two milestones (*Hitting the Books*, *Trinity Restored*) never granted their Shock Resist bonus — both now pay out retroactively on the next load.

Fixed: skill-tree node names no longer overlap each other or the links between them.

Fixed: panel buttons no longer draw their icon and label over the frame when a row is crowded.

Changed: the Dimensional Storage now starts empty for every blessing and is filled from the shop's material packs; old saves keep what they hold.

Changed: the System Analysis skill-tree node and its `V` hotkey are gone — the threat reading is always on instead. A save that bought the node gets its 10 System Points back.

Changed: icons and panel text are easier to read — every icon now stands on a lit plate, and panel labels, values and headings are colour-coded.

---

## 0.6.1

Fixed: the System hotkey did nothing in Skyrim VR (a bug introduced by 0.6.0's own VR crash fix) — it now opens the menu as expected.

Fixed: the Dimensional Storage no longer leaves disabled "husk" chest references behind after a cell reset rebuilds it.

Changed: reduced heavy CSS effects in the PrismaUI menu for steadier framerates; looks the same, costs less to redraw.

---

## 0.6.0

Added: **Custom blessing** — a fifth path that lets you set the three pieces separately: starting gift, reward pace and skill-tree depth, each NORMAL/HERO/ASCENDED. So "the whole tree open but a normal start and rewards" is a valid build.

Added: **Reboot button** in the System menu — re-pick your blessing on an existing character (e.g. switch to a Shattered Dormant run) without uninstalling; your milestones, skill tree and points are kept.

Added: **Reworked System status menu** (PrismaUI patch) — a proper dashboard with tier header, milestone/point tiles, attunements and a scrolling titles list that stays on screen no matter how many you have.

Added: **Optional SkyrimNet integration (experimental, untested)** — if you run SkyrimNet, AI NPCs can react to your reincarnation and the deeds the System recognises. Not yet verified in-game with SkyrimNet running; does nothing at all without SkyrimNet, so it is safe for everyone else. Toggle in the ini.

Added: **Skyrim VR now loads** (experimental) — the mod no longer crashes on SkyrimVR, and its forms resolve there. In VR the UI needs the PrismaUI VR build. Still community-testing.

Fixed: **Character level no longer resets to 1 after loading a save.** (Skills, attributes, perks and gold were never affected — only the level number.)

---

## 0.5.1

Fixed: Level-up sound no longer missing with the PrismaUI patch installed.

Fixed: Better, steadier FPS in the System menu with the PrismaUI patch.

---

## 0.5.0

Added: **Two new ways to receive the blessing.** After picking HERO or ASCENDED you now choose *how* it reaches you — **Full** grants the flat start as before, **Shattered** keeps only what the tier is worth over time (the reward multiplier and the deeper skill tree) and starts you at the same mortal floor as NORMAL. On top of that a fourth blessing, **DORMANT**, takes no tier at all: you begin as an ordinary mortal and the System wakes on its own, to HERO at level 25 and ASCENDED at level 80. Nothing arrives early, nothing stays sealed for good — sealed skill-tree nodes even tell you the level they awaken at. Thresholds are configurable.

Added: **Skill-tree respec.** A Respec button refunds the System Points spent on stat nodes and reverts their effects, with a second click to confirm. The four Omniscience unlocks and Perk Synthesis are excluded on purpose: the System cannot un-teach a shout you already know, and those perk points are long since spent in your perk trees.

Added: **Repeatable utility nodes**, so there is always something worth saving points for — Fleet of Foot (move speed), Beast of Burden (carry weight) and Enduring Vigor (Health/Magicka/Stamina), each buyable again and again, each showing its current rank.

Added: **Optional settings ini** at *Data/SKSE/Plugins/IsekaiHero.ini* — a remappable System-menu hotkey (any scan code, modifier optional), an option to hide sealed skill-tree nodes instead of greying them, and the DORMANT thresholds. Ships with defaults and is safe to delete.

Added: **Optional PrismaUI patch.** A separate download renders the whole interface — skill tree, System panels, level-up flourish — as an HTML/CSS view instead of the built-in one. The mod detects it at load and falls back to its own UI when absent, so it is entirely optional and needs no new save. Only the patch requires PrismaUI; the base mod does not.

Fixed: **The System hotkey no longer fires while a menu is open.** Right Shift + S used to pop the panel open mid-typing — searching an inventory for "Salmon", entering a console command. It now stays quiet whenever a menu has the keyboard.

---

## 0.4.0

Added: Recipes gated behind "do you carry this material" now show up. Crafting overhauls like Complete Crafting Overhaul Remastered hide a recipe until you hold one of its components — a check the game reads straight off your real inventory, which no material-count hook can reach. When you step up to a forge/tanning rack/smelter/grindstone the storage now lends a single unit of each stored recipe material you lack, so the recipe appears; the full stock is still shown and spent from the chest, and the loaner goes back when you leave. One item per type, not stacks — no event flood. (Same trick Workbench Containers uses, done in-engine.)

Added: Other mods' pre-craft prompts see the storage too. An "empower this enchantment with a flawless gem?" prompt reads your carried inventory before the menu opens; the enchanter now lends stored gems the same way, so those prompts offer what's in the Dimensional Storage.

Added: DLC and add-on crafting materials are stocked. Materials are drawn from what recipes actually require, so DLC components come along — chitin plate, netch leather, corkbulb root and the rest of Solstheim's smithing/alchemy set. Existing chests gain them on load without a fresh start.

Fixed: Storage that stopped opening after a long questline is repaired. Finishing the Dragonborn main quest (many in-game days without opening the chest) let a cell reset leave the storage's hidden container disabled — clicking it just dropped you back to the game. It now detects an orphaned container and rebuilds it, keeping every stored item. Hardened against any cell-reset scenario, not just that one.
