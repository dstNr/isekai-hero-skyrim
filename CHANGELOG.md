# Changelog

All notable changes to Isekai Hero are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/), and the project uses
[Semantic Versioning](https://semver.org/) (still in the `0.x` pre-release line).

## [0.4.0] — 2026-07-23

Crafting now holds up inside a full overhaul stack, and the Dimensional Storage
survives a long playthrough. Save-safe: existing chests are repaired and topped up
on load; no save format change.

### Added
- **Recipes gated behind "do you carry this material" now show up.** Crafting
  overhauls like Complete Crafting Overhaul Remastered hide a recipe until you hold
  one of its components — a check the game reads straight off your real inventory,
  which no material-count hook can reach. When you step up to a forge/tanning
  rack/smelter/grindstone the storage now lends a single unit of each stored recipe
  material you lack, so the recipe appears; the full stock is still shown and spent
  from the chest, and the loaner goes back when you leave. One item per type, not
  stacks — no event flood. (Same trick Workbench Containers uses, done in-engine.)
- **Other mods' pre-craft prompts see the storage too.** An "empower this
  enchantment with a flawless gem?" prompt (e.g. Thaumaturgy) reads your carried
  inventory before the menu opens; the enchanter now lends stored gems the same way,
  so those prompts offer what's in the Dimensional Storage.
- **DLC and add-on crafting materials are stocked.** Materials are drawn from what
  recipes actually require, so DLC components come along — chitin plate, netch
  leather, corkbulb root and the rest of Solstheim's smithing/alchemy set. Existing
  chests gain them on load without a fresh start.

### Fixed
- **Storage that stopped opening after a long questline is repaired.** Finishing the
  Dragonborn main quest (many in-game days without opening the chest) let a cell
  reset leave the storage's hidden container disabled — clicking it just dropped you
  back to the game. It now detects an orphaned container and rebuilds it, **keeping
  every stored item**. Hardened against any cell-reset scenario, not just that one.
- **No more endless bear traps / stutter after visiting a station.** Lending scripted
  modded materials (a craftable bear trap, a campfire kit) tripped their
  "container changed" scripts into spawning copies of themselves. Everything the
  storage moves or stocks is now limited to the official masters (Skyrim + DLC),
  which carry no such scripts — the requested DLC materials still come through, since
  DLC *is* official.
- **Materials are reliably available at every station again.** A thread-safety rework
  of the in-place crafting hooks fixed a case where the storage's materials were
  counted but not offered, and removed two count hooks that never actually reached
  the recipe/prompt code they were meant for.

## [0.3.1] — 2026-07-17

Follow-up fixes to the crafting rework. No save format change.

### Fixed
- **Other mods' pre-craft checks now see the Dimensional Storage.** Scripts and
  recipe conditions read a different item-count path (`PlayerCharacter::GetItemCount`)
  than the crafting menu does, so anything gated on "do you have this material" only
  saw your carried inventory. Two symptoms this fixes:
  - Enchantment "empower" prompts (e.g. spend a flawless gem) now offer gems that
    live in the storage.
  - Crafting overhauls that hide recipes behind an item-in-inventory condition no
    longer hide the ones whose materials are in the storage. The count hook is scoped
    to crafting stations, so nothing changes elsewhere.
- **No more duplicate enchantments at the table.** "Arcane Omniscience" marked every
  same-named base enchantment known, listing an effect several times. It now keeps one
  per name (harmless — applied strength scales with your skill, not the base form).

## [0.3.0] — 2026-07-17

Crafting reworked to read the Dimensional Storage in place, and the storage stock
completed. Save-safe: existing chests are topped up on load; no save format change.

### Added
- **Zero-transfer crafting.** Crafting stations now read *and consume* the
  Dimensional Storage directly — nothing is shuttled into your inventory, so there
  is no script-event lag and no timing race. Covers the whole forge family
  (smithing, tempering, smelting, tanning), alchemy, and enchanting. Materials are
  hooked in place only while you stand at a station; everything else is untouched.
  (Technique adapted from [SCIE](https://github.com/ohfor/scie), MIT.)
- **Every filled soul gem in storage**, black included — enumerated from the game
  data so none is missed. Older chests are brought up to the full set on load.

### Fixed
- **All material types are available at every station.** The old lending filtered
  by item type, so ingredient-based recipes (e.g. Daedra Hearts for Daedric
  smithing) had no materials at the forge. The in-place hooks don't filter — if it
  is in the chest, the recipe can use it.
- Crafting no longer stutters when opening a station in a heavily scripted load
  order (no more mass item shuffling).

## [0.2.1] — 2026-07-17

Shout unlocking, fixed properly. No save changes — an existing save loads as-is
(shouts already added to a save by 0.2.0 stay in the menu; a fresh character sees
the clean, corrected list).

### Fixed
- **"All shouts" now actually makes them usable.** Thu'um Omniscience added the
  shouts to the menu but the words stayed locked and could not even be unlocked
  with dragon souls — the word-unlock call was a no-op on some setups. The word's
  `kKnown` flag is now set directly (the same mechanism that already works for
  enchantments), so every learned word is immediately usable, no soul needed.
- **Only real player dragon shouts are unlocked now.** The unlock filters to
  described, three-word Thu'um and excludes the non-player forms that used to slip
  in: racial/beast greater powers (Battle Cry, Voice of the Emperor, Beast Tongue,
  the werewolf howls), the dragon-AI copies (`Dragon Unrelenting Force` and kin,
  while the real `Dragon Aspect` is kept), and the "for DRAGONS only" variants.

## [0.2.0] — 2026-07-17

The first round of full-modlist testing and feedback. Save-safe: existing saves
carry over cleanly — every skill node you already unlocked stays unlocked (the new
tier gate only limits *new* purchases), and existing storage chests are never
touched on load.

### Added
- **Dimensional Storage for NORMAL souls.** Every reincarnated soul now gets the
  pocket dimension — panel button, chest, automatic crafting-station lending.
  NORMAL receives it **empty**, as a personal stash to fill; HERO and ASCENDED
  remain pre-stocked (blessing-scaled).
- **Safe mid-playthrough installation.** Add the mod to an existing save and the
  System boots on the next load: pick your blessing, and every milestone quest you
  already completed is recognised and rewarded retroactively.
- **Nexus header image** and an AI-usage disclaimer in the mod description.

### Changed
- **The skill tree now deepens with your rebirth tier.** NORMAL walks the
  self-made half (stats, resistances, Perk Synthesis, faster Thu'um). HERO
  additionally unlocks the four **Omniscience** gifts (shouts, enchantments,
  ingredients, spells). ASCENDED additionally unlocks the **World Tree** capstone.
  Sealed nodes stay visible but greyed, with a "requires HERO/ASCENDED rebirth"
  hint — one shared graph, no duplicate tables.
- **System menu hotkey moved to Right Shift + S** ("S" for System; was RShift+F10),
  to stay clear of contested keys in large modlists.
- **Blessings only ever raise stats.** On an existing save an established character
  never loses levels, skills or perks; attribute bonuses count only the levels
  actually gained.

### Fixed
- **Perk points are read unsigned — the cap is 255, not 127.** A character with 200+
  banked perk points used to display as a negative number in the tree; the ASCENDED
  blessing now grants a genuine full pool.
- **No milestone rewards before the reincarnation intro.** Alternate-start mods that
  auto-complete early main-quest stages no longer trigger payouts before the System
  is bound.
- **Reincarnation waits for the game world.** In character-creation rooms (e.g. NYA)
  the System now holds until you actually enter the world instead of firing in the
  creation room.

## [0.1.0] — 2026-07-16

Initial release — the native SKSE C++ / CommonLibSSE-NG rewrite with its own
ImGui UI (the archived Papyrus original lives under `papyrus/`, git tag
`papyrus-v1.0`).

### Added
- **Reincarnation on first control**, start-mod independent (vanilla, `coc`,
  Alternate Start, Skyrim Unbound) with a paced `[ SYSTEM ]` boot sequence.
- **Three blessings** — NORMAL / HERO / ASCENDED — that scale every reward
  ×1 / ×2 / ×4 for the whole run.
- **79 quest milestones** across the main quest, Companions, College, Thieves
  Guild, Dark Brotherhood, Civil War, Dawnguard and Dragonborn — each paying a
  passive stat bonus, System Points, and dragon souls / perk points on the big
  beats. Already-completed quests are recognised retroactively.
- **Passives as real abilities**, aggregated under Active Effects and recomputed
  from scratch on every load.
- **System skill tree** with **mod-aware knowledge unlocks** spanning the entire
  load order (all shouts, enchantments, ingredient effects and spells), plus a
  repeatable **Perk Synthesis** node (1 System Point → 5 perk points).
- **Dimensional Storage** whose contents are automatically available at crafting
  stations — station-aware, so the forge never shuttles alchemy ingredients.
- **Custom ImGui UI** in a Solo-Leveling look and **custom sound effects** routed
  through the game's audio system.
- **ESL-flagged plugin** that overrides nothing — load-order position is irrelevant.

[0.4.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.4.0
[0.3.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.3.1
[0.3.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.3.0
[0.2.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.2.1
[0.2.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.2.0
[0.1.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.1.0
