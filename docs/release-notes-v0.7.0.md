What was uploaded to Nexus as **0.7.0**.

The System starts giving you work. Kill objectives arrive on its own schedule, threat
readings float over the enemies themselves, and a HUD layer announces both. The ten System
potions go live, the shop grows categories, and the whole UI gets a colour and legibility
pass.

**Co-save version 13.** Existing characters pick everything up on the next load — no
reboot, no new game. Older saves stay readable.

Archives: `IsekaiHero-v0.7.0.7z` (main mod) and `IsekaiHero-PrismaUI-Patch-v0.7.0.7z` (optional UI patch).

### Added

- **System Quests.** The System hands you a kill objective — "Slay 25 Draugr" — announces
  it with a banner, counts your kills top-centre as they happen, and pays System Points
  when it is done. Then it goes quiet and offers the next one on its own. This is what
  keeps points coming in once the 79 milestones run out.
  - The first objective of a character's life arrives 12 game hours in and is always the
    same reachable one (wild beasts); later ones come a day after each is finished, are
    rolled at random, and are never gated above your level. Both waits are in the ini.
  - Targets are matched by actor **keyword**, so creatures added by other mods count.
  - A kill counts when the killer is you **or one of your teammates** — followers, summons,
    reanimated thralls, poisons and runes all land.
- **Threat labels.** A target frame over each enemy: level in a disc, name over a health
  bar, and a colour-coded TRIVIAL / MANAGEABLE / DANGEROUS / LETHAL from the level
  difference. The signature isekai "Observation" move, always on rather than bought.
  - By default they appear on what you are **aiming at** and on whatever is **actually
    fighting you**. `ThreatLabelTargets` in the ini offers three other policies, and `F10`
    switches the whole display off and on.
  - This replaces the old `System Analysis` skill-tree node and its `V` hotkey, both
    removed. A save that bought the node gets its **10 System Points refunded** on the
    next load.
- **The ten System potions are live** — four instant Restoratives and six one-hour
  Elixirs, sold in the System Shop at 3 and 6 System Points for ten at a time.
- **Shop categories.** Eighteen cards in one flat grid was a wall; the shop now has a
  category rail (Materials, Wealth, Restoratives, Elixirs) in both renderers.
- **Five repeatable skill-tree nodes** closing the resistance and regen gaps: Storm Ward,
  Warded Mind, Arcane Absorption, Iron Skin and Rapid Recovery.
- **Mastery tiers.** Every utility stat now caps at 10 ranks across 5 named tiers
  (Novice → Grandmaster), price rising per tier, with a level-up flourish on reaching
  Grandmaster — a celebrated ceiling instead of an open-ended grind.
- **System Rank** (E through S) in the status panel, with its own insignia and colour.
- **Consistency checks and an in-game self-test.** The self-test runs by itself after
  every load and writes a PASS/FAIL report to the log, so a bug report never needs more
  than the log file. It found a real one before release: a quest objective hunting a
  keyword that does not exist, which could be handed out but never completed.

### Fixed

- **Two milestones have never granted anything.** *Hitting the Books* and *Trinity
  Restored* both award Shock Resist, but the plugin carried no ability for that stat, so
  the passive silently did nothing. Both now pay out, retroactively on the next load.
- **The Dimensional Storage arrives empty for every blessing** and is filled from the
  shop's material packs. It used to arrive pre-stocked and quietly top itself up on every
  load, which made buying materials pointless. Old saves keep what they already hold.
- **Skill-tree labels no longer overlap.** The layout measured tiles, not the names under
  them, so longer names ran into their neighbours and into the connecting links.
- **Panel buttons fit their icons.** With five choices in one row the icon and label were
  wider than the button and ran over the frame; both renderers now stack icon above label
  when a row is crowded.

### Changed

- **Every icon stands on a lit plate.** The art is dark-bodied, so on a dark panel only its
  cyan edges survived and each icon read as a few floating strokes. Two rules were also
  actively dimming icons on the darkest surfaces in the UI.
- **The System panels read in colour.** Row labels in the System's cyan, values bright,
  headings as accent lines — the numbers you opened the panel for are no longer as hard to
  find as the words around them.
- **The System Shop is its own screen** with item cards, prices and affordability, instead
  of a dialog panel with text buttons.
- **The skill tree was redesigned** in both renderers: named, framed zones (CORE / MIGHT /
  ARCANA / SHADOW / MASTERY), always-on node names, larger landmarks, and a mastery rail
  that shows each stat's rank and tier instead of a column of bare icons.

### Notes

- The threat labels and the HUD layer are drawn by the same overlay as the mod's menus,
  which **Skyrim VR does not get**. In VR the quest announcements fall back to the game's
  own corner notifications and the threat labels do not appear.
- `IsekaiHero.ini` gained a `[Threat]`, a `[Quests]` and a `[Diagnostics]` section. Delete
  the file and the mod uses the defaults described in it.
