# Nexus Mods — Changelog-Einträge

Was pro Version im Nexus-Changelog-Feld steht. Bewusst kürzer und spielernäher
als `CHANGELOG.md`, das der Entwickler-Record bleibt.

---

## 0.5.1

Fixed: The level-up sound is no longer missing when the PrismaUI patch is installed. Closing a System panel briefly unpauses the game, and the engine was discarding the sting that played in that same instant — so it never reached the speakers. It now plays reliably. (The built-in UI was never affected.)

Fixed: Higher and steadier FPS while the System menu is open with the PrismaUI patch. Two menu accents animated non-stop — a sweep under the headings and a pulsing ring on every affordable node — which forced the web UI to redraw every frame. Both are now static; the menu looks the same but lets the framerate recover.

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
