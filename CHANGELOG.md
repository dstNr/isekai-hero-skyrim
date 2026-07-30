# Changelog

All notable changes to Isekai Hero are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/), and the project uses
[Semantic Versioning](https://semver.org/) (still in the `0.x` pre-release line).

## [Unreleased]

### Fixed
- **Dimensional Storage no longer leaves orphaned chest references behind.** When the
  storage's hidden container was rebuilt after a cell reset, the old reference was only
  disabled, so a long save could accumulate several dormant husks (save bloat, and the
  kind of "unattached" entries a save cleaner like ReSaver flags). Rebuilds now delete
  the old reference, and any husks left by earlier builds are swept on load.

## [0.6.0] — 2026-07-27

A big content-and-polish release on top of 0.5.x: a fully configurable blessing, a
proper status dashboard, a way to re-choose your path mid-run, optional AI-NPC
integration, and the groundwork for Skyrim VR — plus a fix for the character level
resetting on load. Save-safe: existing saves keep everything and derive the new fields
from their tier.

### Added
- **"Custom" blessing.** A fifth path that unbundles the tier into three independent
  dials — starting gift, reward pace and skill-tree depth, each NORMAL/HERO/ASCENDED —
  chosen one at a time. Lets you build, say, "the whole skill tree open but a normal
  start and normal rewards"; it self-balances because nodes are still bought with System
  Points earned at your chosen pace. Presets are unchanged. Save format is now v10
  (existing saves derive the new fields from their tier, so they behave exactly as
  before).
- **Dedicated PrismaUI status screen.** With the PrismaUI patch the System status is now
  its own dashboard — tier header with an awakening badge and the Skill Tree / Storage
  icon buttons, milestone/System-Point tiles, an attunements grid and a scrolling titles
  ledger (bounded, so it stays on screen even with every milestone earned) — instead of
  the plain monospace text the generic panel showed. Reboot and Close sit in the footer
  (Reboot two-click). The built-in ImGui ledger is unchanged.
- **"Reboot System" button** in the status panel: re-opens the blessing choice on an
  existing character (e.g. Hero → a Shattered Dormant run) without the fragile
  uninstall/reinstall dance. Two-click confirm; milestones, skill tree and System Points
  are kept. Stats a previous *Full* blessing already handed out are not clawed back.
- **Optional SkyrimNet integration — experimental, untested** (AI-driven NPCs): when
  SkyrimNet is installed, the mod pushes the player's System status — reincarnation,
  tier, each milestone — as persistent world-knowledge so AI NPCs can react to the isekai
  premise. One-way, soft-detected, does nothing without SkyrimNet, toggle in the ini.
  **Not yet verified in-game with SkyrimNet running** (like the VR support below); it is
  inert for anyone without SkyrimNet, so it cannot affect other setups.

### Fixed
- **Character level no longer drops to 1 after a load.** The blessing's level (e.g. 150
  for a full ASCENDED) is stored on the player's actor base, which does not persist for
  the player — so it reverted on load. It is now re-asserted from the save's memory on
  every load, the same way the passives are. (The rest of the blessing — skills,
  attributes, perks, gold — always persisted; only the level number was affected.)
- **Skyrim VR no longer crashes on load.** The ImGui overlay hooked the desktop swap
  chain, which is invalid on the VR renderer — it now skips that hook in VR. In VR all
  UI goes through the PrismaUI patch (needs PrismaUI's 1.5.0 VR build); the reincarnation
  prompt is held rather than stranding the character when no VR UI is available.
- **VR form resolution.** Our spells/sounds/storage container resolved to null in VR;
  they now resolve via the plugin's partial index, the way the form dump already did.

## [0.5.1] — 2026-07-26

A small follow-up to 0.5.0, both fixes in the PrismaUI path. No save format change.

### Fixed
- **The level-up sting is no longer swallowed under PrismaUI.** Closing a System
  panel takes the game out of menu-pause, and the engine's resume pass discards any
  sound started in that same frame — so the reincarnation/milestone sting never
  reached the speakers. Sounds that follow a panel closing now play just past the
  unpause frame. (The built-in ImGui UI never paused, so it was never affected.)
- **Higher, steadier FPS while the System menu is open (PrismaUI).** Two UI accents
  animated continuously — a sweep under the section headers and a pulsing ring on
  every affordable node — which kept the web renderer repainting the whole view every
  frame. Both are now static; the menu looks the same at rest but lets the renderer
  idle, so the framerate holds.

### Internal
- The developer's real name is no longer embedded in the shipped DLL (SKSE author
  field, PDB path, source-path strings) and the debug `.pdb` is no longer packaged.
  No player-facing effect.

## [0.5.0] — 2026-07-24

Acting on a detailed player report: more ways to play, more to spend points on, an
optional web UI, and a couple of quality-of-life fixes. Save-safe — an existing save
keeps its blessing, nodes and points; the new fields default cleanly on load.

### Added
- **"Shattered" awakening for HERO and ASCENDED.** After choosing the blessing you
  now choose how you receive it. **Full** is the blessing as before — skills, level
  and fortune granted at once. **Shattered** keeps the tier's payoff (the ×2 / ×4
  reward pace and the deeper skill tree it unlocks) but takes **no** flat starting
  grant: you begin at the same mortal floor as NORMAL and earn every step. Higher
  ceiling, same floor — for players who want the power but want to work for it.
- **"Dormant" blessing — the System grows with you.** A fourth choice at rebirth: the
  blessing sleeps instead of being taken. You begin as an ordinary mortal, and the
  System wakes on its own — to **HERO at level 25**, then to **ASCENDED at level 80**,
  each with its own awakening. Nothing is handed to you early and nothing is sealed
  away for good: the deeper skill-tree nodes open as you grow into them, and their
  tooltip names the level they awaken at rather than a rebirth you cannot take back.
  Combines with the Full/Shattered question above — **Dormant + Shattered** grows only
  the System's reach (reward pace, tree depth) and never hands over a grant at all.
  Thresholds are configurable (`[Dormant]` in the ini).
- **Skill-tree respec.** A **Respec** button in the tree refunds the System Points spent
  on stat nodes (attributes, Thu'um cooldown, move speed) and reverts their effects; it
  asks for a second click to confirm and hides itself when there is nothing to refund.
  The four *Omniscience* unlocks and *Perk Synthesis* are deliberately **not** refunded —
  the System cannot un-teach a shout you already know, and those perk points are long
  since spent in your perk trees. (Vanilla perks are out of scope; dedicated respec mods
  handle those.)
- **Repeatable utility nodes in the skill tree.** Three new NORMAL-tier nodes you can
  buy again and again, so there is always something to spend System Points on:
  **Fleet of Foot** (+3% move speed per rank, up to +30%), **Beast of Burden**
  (+25 carry weight per rank) and **Enduring Vigor** (+25 Health/Magicka/Stamina per
  rank). Each purchase shows its current rank. (No attack-speed node — that road is
  a well-known source of animation and mod conflicts.)
- **Optional settings ini** (`Data/SKSE/Plugins/IsekaiHero.ini`) — ships with defaults,
  safe to delete.
  - **`HideSealedNodes`** — hide skill-tree nodes gated above your rebirth tier instead
    of showing them greyed with a "requires HERO/ASCENDED" hint.
  - **`[Dormant]`** — `DormantHeroLevel` (25) and `DormantAscendedLevel` (80), the levels
    at which a dormant blessing wakes. Changing them affects a running save.
  - **Remappable System-menu hotkey** (`[Hotkey]`): `SystemMenuKey` and
    `SystemMenuModifier` take any DirectInput scan code (hex or decimal; modifier `0` =
    none). Default unchanged — Right Shift + S. The ini lists the common codes.
- **Optional PrismaUI UI patch.** A separate download renders the **whole** Isekai UI —
  the skill tree, the System dialog panels (blessing/awakening choice, the status
  ledger, milestone payouts) and the level-up flourish — as an HTML/CSS view through
  the [PrismaUI](https://www.prismaui.dev) framework instead of the built-in ImGui one.
  The base mod's DLL auto-detects at load: with PrismaUI **and** the patch's view files
  both present the web UI is used; otherwise it falls back to ImGui, always safe. UI
  data stays single-source in the plugin (handed to the view as JSON, clicks call back
  into the same logic) — nothing to keep in sync by hand.
  - Patch archive: `IsekaiHero-PrismaUI-Patch-v*.7z` (just the view files under
    `Data/PrismaUI/views/IsekaiHero/`). No extra DLL.
  - Player requirements for the patch:
    **[PrismaUI](https://www.nexusmods.com/skyrimspecialedition/mods/148718)** and
    **Media Keys Fix** (PrismaUI's own dependency). The base mod needs neither.

### Fixed
- **The System hotkey no longer fires while a menu is open.** Right Shift + S used to
  pop the panel open mid-typing — searching an inventory for "Salmon", entering a
  console command. It now stays quiet whenever a menu has the keyboard.

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

[0.6.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.6.0
[0.5.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.5.1
[0.5.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.5.0
[0.4.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.4.0
[0.3.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.3.1
[0.3.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.3.0
[0.2.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.2.1
[0.2.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.2.0
[0.1.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.1.0
