# Changelog

All notable changes to Isekai Hero are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/), and the project uses
[Semantic Versioning](https://semver.org/) (still in the `0.x` pre-release line).

## [Unreleased]

### Changed
- **The System no longer sets your character level** (#29, #17, and the pace half of #32).
  A player reported that there was "no good way to set the difficulty" — enemies were either
  impossible and then trivial, or trivial from the start. Both halves were one fact: the
  blessing set the character level (150 for ASCENDED), Skyrim's levelled lists key off that
  level, and so every scaling enemy was pulled to its ceiling in a single step. The
  attribute grant was derived from the same number, which is why it was worth `+497` per
  stat and why the skill tree's own attribute nodes read as noise beside it.
  The blessing now grants a **body**: Health, Magicka and Stamina are raised *to* a target
  (180 for HERO, 600 for ASCENDED — what the old level worked out to), never past it and
  never twice, so a DORMANT character awakening at level 80 still receives only what they
  are missing. Your skills, attributes and perks make you stronger; the world stays where it
  is. DORMANT is fixed by the same change — it used to jump twice, and the second jump was
  the whole 150.
  ASCENDED's starting System Points drop from 500 to 10. The 500 were marked in the source
  as a test value, and they bought the entire 260-point tree twice over; HERO seeds 5 at a
  2× reward scale, so 4× seeds 10. A tier's advantage is its scale, which is what the design
  always said.
  Eight values move into the ini and the MCM: both skill-tree multipliers and each tier's
  skills, body and starting points. The price multiplier is fixed when a character is
  reincarnated rather than read live, because a respec gives back exactly what was paid and
  could not keep that promise if the price moved underneath the purchase; the strength
  multiplier is live, because node effects are recomputed from scratch on every load.
  **Existing characters** are corrected once on the next load, back to the level they had
  before their blessing. Nothing else is touched — attributes, skills, perks and points all
  stay. A save from before the pre-blessing baseline existed (or a SHATTERED start, which
  never captures one) keeps what it has; REBOOT already says plainly that it cannot hand
  that body back.
  This also removes a reported bug on its way out: the granted level lived on the shared
  ActorBase, so once ASCENDED had written 150 there, loading a save the System had never
  touched left *that* character at 150 and fired level-gated quest mods. There is no grant
  left to leak.
- **The threat labels are one line over one underline** (#26). The plate is gone. What it
  drew was the verdict colour three times over — the level badge, the plate's two edges
  and the whole health bar — with magicka blue and stamina green competing underneath;
  four saturated things at once, which is what the report meant by "too many
  high-contrast elements competing". Restyling a plate does not fix a frame that says the
  same thing four ways, so now exactly one element carries colour: the underline, which is
  the health bar and keeps the verdict's colour, so red still means lethal rather than
  low. The level, the name and the health figure share one dim row, the verdict names in a
  word what the bar says in a colour, and the text is outlined rather than backed by a
  panel — which is what lets the panel go away without the name vanishing over snow.
  Nothing was dropped: level, name, health figure, verdict and the resource hairline are
  all still there, and the frame is shorter than it was.
- **Skyrim 1.7.104 support.** The plugin now builds against `alandtse/CommonLibSSE-NG`
  v7.5.4, pulled in as the `extern/CommonLibSSE-NG` submodule, instead of the
  `commonlibsse-ng` vcpkg port. That port still ships CharmedBaryon's 3.7.0 from May 2023,
  and upstream itself marks it unmaintained. It cannot run 1.7: it classifies the runtime
  by minor version, so `1.7` falls through to "SE", and its address library reader only
  knows database formats 1 and 2 while 1.7.99+ ships format 5. One DLL still serves SE, AE
  and VR. What the move required:
  - `RE::DebugNotification` became `RE::SendHUDMessage::ShowHUDMessage`.
  - `BSGraphics::Renderer`'s `data` member is gone; the device, context and swap chain now
    come from `GetDevice()`, `GetRendererDataSingleton()` and `GetCurrentRenderWindow()`.
    The swap chain used to be read from `renderWindows[0]`, which the library warns is not
    necessarily the current window — this fixes that too.
  - VR wands are no longer one `kVRLeft`/`kVRRight` pair but a primary and a secondary per
    headset family; hotkeys now bind across all six.
  - `MenuGuard` grew three vtable layouts. 1.7.99 inserted two virtual functions into
    `MenuEventHandler`, moving `ProcessButton` from slot 05 to 07 (VR has it at 08), so
    CommonLibSSE-NG made the `Process*` methods non-virtual dispatchers — leaving nothing
    to override. The logic sits in a base class and three thin subclasses place the slot;
    the runtime picks one and logs which.

### Added
- **A Mod Configuration Menu, as an optional installer component** (#27). Five pages
  covering 28 of the 32 ini settings, each row carrying the explanation the ini gives it —
  the reported problem was that the settings existed and nobody could find them, so no new
  knobs were added. Requires SkyUI and MCM Helper, which is why it is optional: a player
  happy with the built-in panel installs nothing new and loses nothing.
  - The plugin uses no MCM Helper API. MCM Helper writes a plain ini, and `Config` layers
    it over the shipped one — a key the MCM never mentions falls through to the shipped
    value rather than resetting. Nothing is ever written back.
  - Settings take hold when the journal closes, which is where SkyUI's MCM lives. Hotkeys
    are the exception: they are registered once at load and still need a restart.
  - `Language`, the controller buttons and the VR wand button stay ini-only. MCM Helper
    stores engine settings, which have no string type, and the pad buttons are masks the
    keymap control cannot capture. The menu says so on the page where they would have been.
- **The right stick scrolls** (#2). The controller already reached every screen — the left
  stick drives a cursor and A clicks, which covers the panels and the skill-tree canvas
  with one mechanism — but not the skill tree's mastery rail: dragging inside a child
  window pans the tree behind it rather than scrolling the rail, so the one scrollable
  surface in the mod was unreachable without a mouse wheel.
- **The System notices what you actually do** (#30). Two tracks, both off a single setting
  each, and both paying into System Points rather than into new records:
  - **Kill bounties.** Every kill is tallied against the quarry types it matches — the same
    ActorType keywords the objectives use — whether or not an objective is standing. Each
    time a type crosses another multiple of `KillsPerBounty` (50 by default) the System
    pays `KillBountyPoints`. An actor that is honestly two types counts for both.
  - **Professions.** Harvesting, smelting, tanning, smithing, alchemy and enchanting count
    as one track: every `ProfessionActionsPerPoint` of them (25 by default) pays a System
    Point. Taking materials out of the Dimensional Storage at a station does not count —
    a crafted item arrives with no previous container, a withdrawn one carries the chest.
  - The co-save grows to version 15 for the tally and the action count. Older saves load
    with an empty tally, which is the truthful answer rather than a loss.
- **`QuestRewardScale`, so the System's objectives can be made worth more or less** (#32).
  A multiplier on what an objective pays, in the `[Quests]` block, clamped to 0.1..10.0.
  It applies where the payout is snapshotted — when the objective is handed out — so an
  objective already standing keeps the number it was written with and changing the setting
  cannot move a finish line the player is already running at. How often objectives arrive
  was already tunable through `FirstTaskHours` and `TaskIntervalHours`.
- **A public API other SKSE plugins can use** (#28). A versioned pure-virtual interface
  behind an exported `RequestPluginAPI`, the same pattern this mod already consumes from
  PrismaUI. Foreign plugins can read whether the System is active, the tier, points, rank
  and whether the player holds a given skill-tree node, and can grant System Points with an
  attributed source; a state-changed message is dispatched for listeners. Nothing mutates
  structure, and there is no Papyrus surface — the mod stays script-free. Ships as the
  single dependency-free header `include/IsekaiHeroAPI.h`.
- **The Flameforged Oathblade**, the first weapon the System sells — a new ARMAMENTS shelf
  in the shop, at 40 System Points. The mesh is generated geometry converted to a Skyrim
  NIF with collision, BSX flags and an attachment point built from measured numbers rather
  than copied blocks (`tools/add-weapon-collision.py`); the pipeline is written up in
  `docs/WEAPON_ASSET_GUIDE.md`.
- A weapon's shop card shows its **damage and critical damage**. `DescribeEffects()` used
  to return early for anything that is not a potion, which would have left a 40-point item
  showing a name and a price and nothing else.
- `tools/icon-cutout.py`, because image models asked for a transparent background paint the
  checkerboard into the pixels instead. It removes only background reachable from the
  border, so highlights inside the subject survive.

### Fixed
- **`UiScale` now reaches the threat labels in VR** (#26). It was the one surface the
  setting did not cover: the VR path drew its frames at a fixed size. The flat path has
  scaled with both resolution and `UiScale` since 0.8.0, so the 4K complaint behind the
  report does not apply there. The VR scale is clamped so a frame can never grow out of
  its band into the one above it, and distance falloff stays off in VR, where the
  billboard already shrinks in world space. **Untested — there is no VR install here.**
- **A second Dimensional Storage chest no longer appears at your feet when you open it.**
  `Open()` moves the chest into the player's cell and then asked `Is3DLoaded()` on the very
  next line to decide whether the reference was still alive. `MoveTo` updates a reference's
  cell and position at once but hands the 3D attach to the engine's queue, so a perfectly
  healthy chest reads as having none — and every open in a cell it had not been in was
  treated as an orphaned reference, rebuilt, and the fresh one `PlaceObjectAtMe` drops at
  the player's feet is the chest players saw appear. The check never protected the
  activation it was guarding either: the rebuild branch went straight on to activate its
  fresh chest without asking about 3D, and that path worked. A genuinely orphaned
  reference is a disabled or deleted one, which `Open()` and `ResolveChest` still catch —
  that is the cell-reset repair from 0.5.0, and it is unchanged. Contents were never at
  risk: each false rebuild migrated them.
- **The Dimensional Storage is the player's property.** Transferring items in or out while
  standing in an NPC-owned cell — an inn, a shop, a house — was a crime: a bounty, items
  flagged stolen, and witnesses turning hostile, a follower included. A reference that
  carries no owner of its own inherits the owner of the cell it sits in, and `Open()` moves
  the chest into whatever cell the player is standing in, so the storage became the
  innkeeper's. It now carries an explicit `ExtraOwnership` naming the player, applied on
  every access rather than only at creation, so saves that already hold an unowned chest
  are repaired on the next open. Reported on Nexus by two players independently.
  Items already flagged stolen stay flagged: clearing the flag on anything put into the
  storage would make it a laundering machine, which a vanilla container is not.
- `package.ps1` and the FOMOD now carry `meshes\` and `textures\`. Without them the ESP
  would have shipped pointing at a mesh that was not in the archive.

### Notes
- The Creation Kit assigned the two new records FormIDs above the ESL ceiling (`0x2311`
  and `0x2313`) and blanked their object bounds. Both were repaired and the failure is
  documented as Part K of `docs/CREATION_KIT_ESP.md`; `tools/check.mjs` is what caught the
  FormIDs.

## [0.8.0] — 2026-08-16

The release where the mod stopped assuming its player sits at a keyboard, reads English,
and looks at a flat screen. It also stopped guessing at what it sells: a potion card now
reads its effects off the potion.

Co-save format **v14** (the REBOOT snapshot). Older saves load untouched; a save written by
this version cannot be read by 0.7.x.

### Added
- **Translations** (#4). Every player-visible string — 335 of them — goes through `L()` /
  `LF()`, with the English text staying in the source as the fallback. Drop a
  `lang/<code>.txt` next to the plugin and set `Language`; anything a translation does not
  cover falls back rather than showing a key.
  - `tools/extract-strings.mjs` harvests the template from the data tables and the web
    view, and **rejects a key it cannot see at build time** — a key built at runtime
    (`L(x ? "a" : "b")`) is invisible to the extractor and would be a string that can never
    be translated. It found one the day it was written.
  - Keys come from an identifier the tables already have (a milestone key, a node key) or
    from a written-out `key` field, never from a slug of the English name. A slug is a rule
    implemented twice — here and in the extractor — and the day the two disagree the
    translation silently dies.
  - **The PrismaUI view translates itself.** The whole table is pushed into the page as
    JSON when the DOM is ready, and its own chrome is HTML, so it needs no round trip.
- **Controller support for the interface** (#2). Hold **LB** and tap **Back** to open the
  System menu; inside a panel the **left stick** moves the cursor, **A** clicks and **B**
  closes. Both bindings are in the ini (`GamepadMenuButton` / `GamepadMenuModifier`, by
  name: `A`, `LB`, `Back`, `DPadUp`, …), and `None` switches the controller off.
  - A cursor rather than ImGui's gamepad focus navigation, deliberately. Focus nav only
    reaches widgets, and the skill tree is a pannable canvas with no widgets in it — a
    controller player would have got the panels and not the tree. One mechanism covers
    every screen.
  - A combination rather than a single button because a controller has none spare: every
    face button, both shoulders and both sticks are bound in vanilla. Holding LB fires its
    own action (a shout) on the way in, which is the price of not stealing a binding.
  - **The hotkey table now keys on the input device as well as the code.** It had to:
    a gamepad button arrives as its XInput mask, and those collide head-on with keyboard
    scan codes — D-pad up is `0x0001` and so is ESCAPE, Start is `0x0010` and so is Q. A
    single map keyed by the bare code would have silently made them the same hotkey.
  - **The PrismaUI patch is covered too**, by the opposite mechanism: PrismaUI hands its
    view the keyboard and mouse but not a controller, so the plugin forwards the pad's
    buttons into the page and the page moves DOM **focus** — D-pad steps through targets,
    A activates, B closes. A page has elements to focus; an ImGui canvas does not. Each
    renderer gets whichever is the cheap answer for it.
- **An interface inside the headset** (#7, #11), through **ImGuiVRHelper** — threat labels
  as world-anchored billboards over the actors they belong to, toasts on the HUD layer, and
  a **B + Y controller chord** that opens the System menu without a keyboard. Untested
  against a real headset; there is none here.
  - `VRThreatLabelHeight` sets how tall a billboard stands in the world, in metres. The
    chord is rebindable through `SKSE\Plugins\IsekaiHero\vrbindings.json`.
  - Two helper clients rather than one: the labels are world quads, the toasts are HUD, and
    the helper treats those as different kinds of client.
- **The shop says what a potion does.** Hovering a card shows its effect list — and the
  list is **read off the form at runtime**, not written into the catalog beside the name.
  Spelling the effects out a second time is a promise the ESP is free to break, which is
  exactly what #24 turned out to be. The effect names arrive already translated by the game.
- **Magicka and stamina on the threat frame** (#25), sharing one thin strip under the health
  bar — magicka blue on the left, stamina green on the right. A full row each made the frame
  half again as tall over every caster, which is what kept this off before; sharing a row
  costs five pixels, so it is on by default now.
- **`IsekaiPoints [n]`**, a console command that grants System Points (default 100,
  negative takes them away). Points come from milestones and nowhere else, so testing the
  skill tree or the shop otherwise meant playing up to whatever was being tested. There is
  no slot to add a console command to in Skyrim, so it takes over an unused developer one;
  the log names which.
  - `DebugPointsKey` in the ini does the same on a key, for the one case the console cannot
    cover: it will not open while one of our own panels owns the screen.
- **`ThreatLabelAimRadius`**, how far off the crosshair an actor may be and still be read.
- **A FOMOD installer**, with the **PrismaUI patch folded into it** — that used to be a
  second download. It costs the archive nothing: the patch is one HTML file plus the same
  74 icons the mod already ships, so the installer points both destinations at one source
  folder. Pictures on the steps, and three questions — interface, start, threat readings.
- **`UiScale`** (default 1.0, clamped 0.5..3.0). One multiplier over everything the mod
  draws, on top of the resolution scaling that was already there — those answer different
  questions. Scaling with resolution keeps the interface the same *apparent* size on a 4K
  screen, which is not the same as whether someone can read it. Asked for by a player with
  impaired vision, and it covers the PrismaUI patch too.
- **REBOOT hands the body back.** It used to say outright that it could not claw back a
  FULL blessing's stats, which made the button half a promise: it re-opened the choice and
  left an ASCENDED physique behind, so "reboot into a Shattered run" changed the label and
  not the character. The mortal is now captured once — level, the 18 skills, base
  Health/Magicka/Stamina — at the moment the first grant is applied, and restored before
  the choice re-opens. Captured once and never overwritten, so a second reboot returns to
  the *original* person rather than to whoever the last blessing made. **This is the
  co-save v14 change.** Nothing is captured where nothing was granted (NORMAL, Shattered, a
  gift-less CUSTOM), and the panel says so instead of pretending.
- **The health figure on the threat label** (#25), centred on the bar — `ThreatLabelNumbers`,
  default on. A bar answers "roughly how much is left"; a fight where it matters whether
  that is 40 or 400 wants the number.

### Changed
- **The aim test measures the whole actor, not its head.** The label hangs over the head, so
  that is what the crosshair was compared against — and at conversational range a person's
  chest is hundreds of pixels below their skull. Aiming at someone's face produced a
  reading and aiming at *them* did not. It now measures against the segment from their feet
  to their head, and the radius above is the sideways tolerance it was always meant to be.
- **The threat frame's level badge is a diamond**, not a disc. Everything else the System
  draws is angular — the plate's own lean, the panels' corner brackets — and the circle was
  the one round thing among it.
- **The installer asks two independent questions** where it used to offer one list of named
  presets. "Alternate start" and "Quiet HUD" were mutually exclusive options in the same
  radio group despite having nothing to do with each other. A FOMOD cannot edit a file, only
  choose one, so the options raise flags and the settings file is picked from the pair.
  - **The "Modlist testing" preset is gone.** Its settings are all one ini line away, and it
    was the one preset that was not about how you want to play.
- **The two restorative potion icons swapped**, so the red bottle is the one that restores
  health (see #24 below).
- **The repo has a LICENSE** (#13): MIT, with the sound effects and the icons carved out —
  those are not ours to relicense. MIT also keeps the ImGui VR Helper route open, since MIT
  code may link an LGPL library without becoming LGPL.

### Fixed
- **The Vigor and Vitality restoratives were the wrong way round** (#24), reported on Nexus.
  Vitality restores health and Vigor restores stamina, which is also what the mod's own
  passive names have always said. The two records swapped their magic effect rather than
  their names, so editor IDs, FormIDs and the shop's catalog are untouched. Which effect is
  which was read out of `Skyrim.esm` rather than taken from the documentation — the
  documentation is where the mistake came from.
- **The storage codex finally does something** (#6, #11). It is used from inside the
  Inventory menu — that is where a potion is always drunk — and the old handler acted
  right there: it called `Open()` straight out of the event, on the event thread, with the
  inventory still on screen. A container menu cannot open over an inventory menu, so the
  whole sequence ran, logged nothing and produced no visible result. It now marks itself
  pending and opens on the next menu close, on the main thread.
  - It opens the **System menu** rather than the chest. The storage is one button inside
    it, so this is strictly more than it did before — and it is the only way in that needs
    no key at all, which is what Skyrim VR and anyone with a colliding hotkey need (#11).
  - Still `StorageCodex = 0` by default, but for a different reason: no longer "it does
    nothing", now "it has not been tested in a running game yet".

- **Threat labels read through walls and terrain** (#23). Reported: a reading on Thadgeir
  inside Falkreath, taken from the road above through both a tree and the ground. The filter
  chain asked mode, distance and projection — and projection only ever answers "is this
  point on screen". A wall is on screen too. It now asks the game's own
  `Actor::HasLineOfSight`, which is what the AI uses to decide whether it can see *you*, so
  the label agrees with the thing it is a label for.
- **A blessing left its level in a save that predates the System** (#21). Reported: new
  character, save before the System runs, take ASCENDED by mistake (level 150), reload the
  earlier save to undo it — and the character is still 150 in a game the System never
  touched, firing level-gated quest mods. The character level is set through
  `actorData.level`, which belongs to the ActorBase and is loaded once per process; a save
  load does not restore it, and the old save carries no record of ours to lower it from.
  Restarting Skyrim cleared it, which was the clue.
- **Crafting materials turned up in the inventory and never left.** Recipes that gate on a
  carried material need one real item in the inventory to become visible, so the plugin
  lends one of each and takes it back when the menu closes. The return list was cleared
  unconditionally — including when there was no chest to return anything to — and a
  forgotten entry is stranded for good, because nothing else in the game knows the item was
  ever on loan. Each station visit lost a few more; one session went from 275 lent tokens
  down to 34, with the difference sitting in the player's pockets. The return is now
  verified by reading the inventory back, and whatever did not move stays on the books.
- **The skill tree closed itself when a mastery node hit Grandmaster** (PrismaUI only). The
  celebration took the view over as if it were a screen of its own, the tree re-push put the
  tree back but not the screen id, and the celebration's own timer then hid the tree the
  player was still using. It is an overlay now, and plays over whatever is on screen.
- **"GRANDMASTER" hung out of its row** in the mastery rail. The tier tag could not shrink
  and could not wrap; it wraps now, which is also the only answer that survives translation
  — every language spells the five tiers a different length.
- **The renderer struct is no longer read under VR**, which is what crashed Skyrim VR on
  startup: `BSGraphics::Renderer`'s layout is flat-screen only. The swap chain is found by
  creating a throwaway one on a hidden window instead.
- **The storage codex finally does something** (#6, #11). It is used from inside the
  Inventory menu — that is where a potion is always drunk — and the old handler acted
  right there: it called `Open()` straight out of the event, on the event thread, with the
  inventory still on screen. A container menu cannot open over an inventory menu, so the
  whole sequence ran, logged nothing and produced no visible result. It now marks itself
  pending and opens on the next menu close, on the main thread.
  - It opens the **System menu** rather than the chest. The storage is one button inside
    it, so this is strictly more than it did before — and it is the only way in that needs
    no key at all, which is what Skyrim VR and anyone with a colliding hotkey need (#11).
  - Still `StorageCodex = 0` by default, but for a different reason: no longer "it does
    nothing", now "it has not been tested in a running game yet".
  - Its item is no longer called "Dimensional Storage" (#5), which is the container's own
    name and made it read as a stray copy of the chest rather than the key to it.

### Security
- **The release archive is checked for the author's real name in full.** The check used to
  look at the DLL only, which is how 0.5.0 shipped a 130 MB `.pdb` with the name in
  thousands of records: the scrub ran, reported clean, and never looked at the file next to
  it. It now walks every staged file before packing, so it cannot be outgrown by adding
  something to the archive.

## [0.7.1] — 2026-08-07

A player-feedback release, and every item in it comes from one report. A tester running
the mod through a modlist reads the same boot sequence on every restart; an alternate
start with a modern-world prologue puts that sequence somewhere it does not belong; and
the threat labels danced over anything that moved.

No save-format change — 0.7.0 saves load untouched.

### Added
- **`AutoStart`** (`[System]`, default 1). Off, the System does not bind itself when the
  player first takes control — it waits for the menu hotkey and boots wherever the player
  is standing when they press it. `TryTrigger`'s cell heuristic can tell a chargen room
  from the world, but nothing can tell an intended prologue from a start gone wrong; this
  hands that one judgement to the player rather than guessing at it per start mod.
  The hotkey already had nothing to show before reincarnation (the status ledger of an
  unbound character is an empty form), so it now runs the boot sequence instead — which
  doubles as a way back in if the automatic trigger ever fails to fire.
- **`TextSpeed`** (`[System]`, default 1.0, clamped 0..20). A multiplier on the typewriter;
  0 types nothing and shows the whole text at once. Applied once, in `ShowSystemWindow`
  before the renderer split, so the built-in panel and the PrismaUI view stay in step.
- **Clicking a panel skips the rest of its reveal**, in both renderers. The buttons are
  hidden until the reveal finishes, so the skip click can never also press one.

### Fixed
- **Threat labels no longer hop about over a moving actor.** They were anchored to
  `worldBound.center + radius * 0.9`, and `worldBound` is recomputed from the current
  *pose*: it swells when a bandit swings, when a wolf leaps, when anything draws a weapon,
  and its centre bobs with every stride — so the label danced around the head instead of
  sitting over it. The anchor is now the 3D root (the actor's placement, which the pose
  hangs off) plus `Actor::GetHeight` (base bounds times scale). Neither term changes with
  animation. The animated bound stays as a fallback for actors whose base object carries
  no usable one. This also steadies the crosshair pick, which projects the same point.

## [0.7.0] — 2026-08-06

The System starts giving you work: kill objectives on its own schedule, threat readings
over the enemies themselves, and a HUD layer to announce both. The ten System potions go
live, the shop grows categories, and the whole UI gets a colour and legibility pass.
Co-save version 13 — existing characters pick everything up on the next load.

### Fixed
- **Two milestones have never granted anything.** *Hitting the Books* (College of
  Winterhold) and *Trinity Restored* (Thieves Guild) both award Shock Resist, but the ESP
  carried ability spells for only eight actor values and Shock Resist was not among them —
  a passive with no ability behind it grants nothing and reports nothing. The ESP now has
  a ninth ability and both pay out, retroactively on the next load for existing characters.
  Found by counting: the milestone table names nine distinct actor values against eight
  abilities, which `tools/check.mjs` now asserts permanently.

### Added
- **The ten System potions are live** — four instant Restoratives (health, magicka,
  stamina, cure disease and poison) and six one-hour Elixirs, sold in the System Shop at
  3 and 6 System Points for ten at a time. Their ESP records now exist, so the cards that
  had been waiting on them appear.
  The built-in shop moved from four columns to six in the same change: eighteen cards at
  four columns produced a 1454px-tall window, taller than a 1080p screen and with no
  scrolling to fall back on.
- **Consistency checks (`node tools/check.mjs`)** and an **in-game self-test**
  (`SelfTestKey` in the ini, off by default). Deliberately not a unit-test suite: every
  translation unit force-includes CommonLibSSE through the PCH and most of the code
  orchestrates game API calls, so unit tests would have caught very little of what
  actually broke. The recurring failure was different — the same data described in two
  places drifting apart, and layout terms that did not all scale together.
  The static checks cover the node table vs. the playground mock, the shop catalog and its
  icons, the status JSON contract across all three layers, zone/tile/label geometry,
  `kVersion` against the highest co-save read gate, save/load field symmetry, and
  `CMakeLists` completeness. The self-test covers what only a running game knows: form
  resolution, **whether every quest target keyword exists in the load order** (a mistyped
  one fails silently), shop deliverability, and hook installation.
- **System Quests** — the System hands out a standing objective ("Slay 25 Draugr") and pays
  System Points for it. This closes an imbalance the shop created: milestones were the only
  *source* of points and there are a finite 79 of them, while the sinks kept growing (skill
  tree, then material packs, gold, potions, and a Dimensional Storage that no longer arrives
  pre-stocked). Quests give points an income that comes from playing.
  - Targets are matched by **keyword**, not by race — `ActorTypeUndead`, `ActorTypeAnimal`,
    `ActorTypeDaedra` and so on. A creature mod's draugr counts without the mod knowing we
    exist, the same semantic-filter approach the skill tree's knowledge unlocks use; a race
    list would go stale the moment anyone installs a creature pack.
  - One objective at a time, and the System keeps its own schedule: the first arrives 12
    game hours into a character's life, each later one a day after the last is finished
    (`FirstTaskHours` / `TaskIntervalHours` in the ini, 0 for "immediately"). An objective
    that is simply there the moment you are reincarnated, and again the second you finish
    one, is a chore list rather than something that happens to you.
  - The **first** objective of a character's life is always the same reachable one (wild
    beasts). Every later one is rolled, never the quarry just finished, and never one the
    character is too low to go and find — no level-4 character is sent after dragons.
  - Counts and payouts scale with character level and are snapshotted when the objective is
    handed over, so levelling mid-hunt cannot move the finish line you are walking towards.
  - A kill counts when the killer is the player **or one of their teammates** (followers,
    summons, reanimated thralls). Strict "player only" reads as broken in play — a poison, a
    rune or a follower's final blow is most of a mage's or a sneak's kills.
  - Announced and tracked the way an MMO does it: a banner when one is handed over, and a
    single line top-centre counting up on each kill that replaces itself rather than
    stacking. The status panel carries the full objective.
  - A **NEW TASK** button that re-rolls the objective on the spot exists behind
    `QuestRerollButton` in the ini, off by default. It is a testing tool, not a feature:
    free and instant, it walks straight around the schedule the objectives are supposed to
    arrive on. `docs/MANUAL_TESTS.md` turns it on.
  - Co-save version 13 (the objective, its snapshotted target and payout, the clock and how
    many objectives this character has had), appended behind version gates like every field
    since v3. **Existing characters pick up an objective on the next load** — no reboot, no
    new game.
- **Five new repeatable skill-tree nodes**, closing the resistance/regen gaps a player
  pointed out (fire and frost resist existed, shock resist didn't): **Storm Ward**
  (Shock Resist), **Warded Mind** (Magic Resist), **Arcane Absorption** (Spell
  Absorption), **Iron Skin** (Armor Rating) and **Rapid Recovery** (Health/Magicka/
  Stamina regeneration). Same utility-rail rules as Fleet of Foot and friends — NORMAL
  tier, no prerequisites.
- **Mastery tiers** for every utility-rail stat (Beast of Burden, Fleet of Foot,
  Enduring Vigor and the five above): capped at 10 ranks, split into 5 named tiers
  (Novice through Grandmaster, 2 ranks each) with the price rising per tier instead of
  staying flat. Reaching Grandmaster fires the same flourish (rings, title punch,
  sound) a milestone gets. Earlier builds left these uncapped — buying the same node
  forever read as a shop, not a skill tree; a real, celebrated finish line reads as
  actual progression. Perk Synthesis is unaffected (a flat SP-to-perk-point exchange,
  not a stat to master). Both renderers show the tier on the node (a roman-numeral
  badge/pip escalating bronze to gold) and in its tooltip.
- **Scrollable utility rail** (both renderers): the rail no longer lays out nodes at a
  fixed height that silently overflowed past the window once enough of them existed —
  it now scrolls, so future additions don't need the whole rail re-tuned.

- **System Rank** in the status panel (both renderers): a derived E through S label next
  to the tier, computed from milestones earned, character level and System Points ever
  invested in the tree. No new state — a pure readout, the same isekai/tower-climbing
  "what rank am I" every save already has the numbers for.
- **Threat labels** — the signature "Observation"/"Appraisal" isekai move, floating over
  the actor itself: a colour-coded TRIVIAL / MANAGEABLE / DANGEROUS / LETHAL from the level
  difference, shrinking and dimming with distance.
  - This replaces the **System Analysis** node and its `V` hotkey, both removed. Gating a
    threat read behind a purchase and a key press is backwards for something you want to
    know *before* you commit to a fight — by the time you have pressed a key and read a
    panel, the decision has been made for you. A save that bought the node has its 10
    System Points refunded on the next load.
  - By default the labels appear on what you are **aiming at** and on whatever is
    **actually fighting you** (`ThreatLabelTargets = aggro`). `hostile` restores the older
    behaviour of marking every enemy in range whether or not it has noticed you; `all` adds
    townspeople; `crosshair` is aim only. `ThreatLabelKey` (default `F10`) switches the whole
    display off and on mid-session.
  - Drawn as a **target frame**: an angular plate leaning right, the level in a disc on the
    left, the name over a health bar, the verdict closing the right end. Text is outlined
    rather than boxed — an outline is what keeps small text readable over grass or snow
    without putting a rectangle on the screen.
  - SE/AE only — they are drawn by the same ImGui overlay as the mod's panels, which Skyrim
    VR does not get.
- **System Shop** — a third icon button in the status panel, next to Skill Tree and
  Storage: spend System Points on filled soul gems (Grand ×1, Common ×5) or gold ×1000,
  delivered straight into the Dimensional Storage. Gives points a second place to go once
  the tree is bought out, so they never stop mattering.
- **Storage codex** — a physical fallback to open the Dimensional Storage: the
  `IsekaiStorageToken` item already sat unused in the ESP; using it from the inventory now
  opens the chest directly, table-free, no System panel needed. It never runs out (the
  same token is handed straight back) and every reincarnated soul is granted one — on
  reincarnation, and backfilled on load for saves from before this existed.

### Changed
- **Every icon stands on a lit plate.** Measured, not guessed: the art is transparent-backed
  but dark-bodied (average 38-60 of 255 across its opaque pixels), so on a navy panel only
  the cyan edges survived and each icon read as a few floating strokes. The built-in
  renderer can only change what is behind — ImGui's image tint multiplies, so it darkens and
  never lifts — so both renderers now put a lit plate under every icon, and the web view
  additionally lifts the art itself. Two rules were actively dimming icons on the darkest
  surfaces in the UI and are gone.
- **The System panels read in colour**, in both renderers. The panel body was one flat
  wall of text in which the numbers you opened it for were no easier to find than the
  words around them. Row labels are now the System's cyan and their values stay bright,
  and a heading standing on its own is an accent line. The rule comes from the text's own
  shape — a short all-caps run before a column gap is a label — rather than from markup:
  one string feeds both renderers, so anything else would have to be understood by both.
- **The System Rank is a block, not a line**: the crest in the built-in status header now
  carries the rank spelled out beside it, and the web view's rank letter is no longer
  white. Both use the same ramp, the one every ranked game has trained people to read —
  steel at E, the System's cyan at C, gold at S. Written once per renderer because
  neither can read the other's, and compared by `tools/check.mjs` so they cannot drift:
  two screens showing one character's rank in different colours would be worse than
  neither being coloured at all.
- **Panel buttons stack their icon above the label** in both renderers when a row is too
  crowded for both side by side. Five blessing choices split the panel five ways, and an
  icon beside a letter-spaced "ASCENDED" does not fit in a fifth of it — the built-in UI
  had started dropping the icons entirely, and the web view drew the label under the icon
  and off the edge of the button.
- **Skill tree redesigned** in both renderers, after the old one read as "23 identical
  dark tiles in a spider web":
  - **Named zones.** Nodes now declare a `SkillTree::Zone` (CORE / MIGHT / ARCANA /
    SHADOW / MASTERY) and each branch is drawn inside its own tinted, labelled frame.
    The three branches existed only in the source's section comments before; nothing on
    screen grouped them.
  - **Always-on node names.** Every graph tile carries its name underneath. Finding out
    what a node was previously meant hovering it, one at a time.
  - **Size hierarchy** via `Node::scale`: the hub, the World Tree capstone and the four
    Omniscience gifts are drawn larger, so the tree's landmarks read as landmarks
    instead of as another +100 Health leaf.
  - **Fitted layout.** Both renderers now scale and centre the node coordinates into the
    available box rather than using them as literal offsets, so the graph fills the
    window instead of leaving a third of it empty, and the table can be re-arranged
    without re-tuning against a window size.
  - **The mastery rail became rows**: icon, stat name, ten rank pips grouped into the
    five tiers, and the current tier — a column of bare icons said nothing about what
    any of them did or how far along it was.
- **The System Shop is its own screen** instead of the generic dialog panel with text
  buttons: item cards with icon, quantity, price and a Buy button, the balance in the
  header, and unaffordable entries visibly out of reach but still readable (the price is
  what you are saving up for). Purchases refresh in place. The catalog moved into
  `Shop::Catalog()`/`Shop::Buy()` so the web patch and the built-in UI offer exactly the
  same goods at the same prices, rather than each hardcoding a button list.
- **The Dimensional Storage now starts empty, for every blessing.** It used to arrive
  pre-stocked for a HERO/ASCENDED starting gift, and every load quietly topped it up with
  any material type it was missing. Both are gone: the chest is filled through the shop's
  new **material packs** instead, so its contents are something you chose to spend System
  Points on. HERO and ASCENDED still get there sooner — their reward scale multiplies
  point income — but nothing is handed over unearned.
  **Existing saves keep whatever their chest already holds**; they simply stop gaining
  new material types on load.
- **Shop catalog rebuilt around material packs**: Smithing / Alchemy / Soul Gems, each in
  a small and a large size (the large one is five times the materials for three times the
  price), plus gold in two sizes — eight cards, and both renderers now wrap them into
  rows. Quantities are *per material type*, and the packs are the same three sweeps the
  chest used to be stocked from, so there is still only one definition of what counts as a
  smithing material. The individual Grand/Common soul gem entries are gone, replaced by
  the soul gem packs.
  Prices are deliberately low — 5 System Points for a pack, 15 for a crate, so kitting out
  all three categories costs 45 and fits inside a starting blessing. Two things set that:
  the chest used to hold 500 x the reward scale of every material for free (2000 of each
  at ASCENDED), so a paid pack has to land in the same league or the change reads as a
  nerf; and points are genuinely scarce — a full ASCENDED run earns roughly 550 on top of
  its 500 starting points while the skill tree alone can absorb ~1170. The shop must not
  compete with the tree for them.
  Icon art for the eight cards is not included — see `docs/SHOP_ICON_PROMPTS.md`; cards
  render without an image until the files exist.
- `UI::IsSystemScreenOpen()` replaces the hand-written
  `IsSystemWindowOpen() || IsSkillTreeOpen()` in the four "don't act over a live screen"
  guards. Each new screen previously had to be remembered at every call site — and the
  new shop window was in fact missed at all four until this existed.
- `Storage::Available()`'s doc comment corrected — it has never required a HERO/ASCENDED
  blessing (NORMAL gets the same empty storage as a stash); the comment just said so.
  Noticed while giving the Shop the same gate. No behaviour change.

## [0.6.1] — 2026-07-28

A small follow-up to 0.6.0: a VR bug introduced by 0.6.0's own VR fix, a performance pass
on the PrismaUI menu, and save hygiene for the storage chest. No save format change.

### Fixed
- **The System hotkey did nothing in Skyrim VR.** 0.6.0's fix for the VR overlay crash
  skipped the whole `Overlay::Install` function in VR — including the input registration
  that makes the hotkey fire at all, which happened to sit after the early return. So VR
  loaded and ran, but the System menu never opened on keypress. Input registration now
  runs before the VR check; only the crash-prone swap-chain hook is still skipped in VR.
- **Dimensional Storage no longer leaves orphaned chest references behind.** When the
  storage's hidden container was rebuilt after a cell reset, the old reference was only
  disabled, so a long save could accumulate several dormant husks (save bloat, and the
  kind of "unattached" entries a save cleaner like ReSaver flags). Rebuilds now delete
  the old reference, and any husks left by earlier builds are swept on load.

### Changed
- **Reduced heavy CSS effects in the PrismaUI menu** (masks, stacked shadows, gradients)
  that PrismaUI's own performance notes flag as costly on its CPU renderer. The menu
  looks the same at a glance but repaints (opening a panel, hovering, the flourish) cost
  noticeably less, for steadier framerates.

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

[0.8.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.8.0
[0.7.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.7.1
[0.7.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.7.0
[0.6.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.6.1
[0.6.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.6.0
[0.5.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.5.1
[0.5.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.5.0
[0.4.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.4.0
[0.3.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.3.1
[0.3.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.3.0
[0.2.1]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.2.1
[0.2.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.2.0
[0.1.0]: https://github.com/dstNr/isekai-hero-skyrim/releases/tag/v0.1.0
