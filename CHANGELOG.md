# Changelog

All notable changes to Isekai Hero are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/), and the project uses
[Semantic Versioning](https://semver.org/) (still in the `0.x` pre-release line).

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

[0.2.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.2.1
[0.2.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.2.0
[0.1.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.1.0
