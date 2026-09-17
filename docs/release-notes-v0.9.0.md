What was uploaded to Nexus as **0.9.0**.

Seventy commits, and the reason for the minor bump rather than a patch: this release
supports a new Skyrim runtime, changes how the System's blessings work, and puts the
settings somewhere players can actually find them.

**Save-format change.** The co-save goes to **version 16**. A save written by this build
cannot be read by 0.8.0 — the mod ignores the record and the character reads as one the
System never touched. Going forward is safe; going back is not.

**Existing characters are corrected once on the next load**, back to the character level
they had before their blessing. Attributes, skills, perks and System Points are untouched.

Archive: `IsekaiHero-v0.9.0.7z` — one FOMOD carrying the main mod, the optional PrismaUI
patch and the new optional MCM.

**Not tested in game.** Everything here compiles, passes 50 static consistency checks and
packages clean, but no part of this release has been played end to end. Treat it as such,
and keep a save from before you install it.

### Added

- **Skyrim 1.7.104 support.** The plugin now builds against `alandtse/CommonLibSSE-NG`
  v7.5.4 instead of the `commonlibsse-ng` vcpkg port, which still ships a 2023 library
  that cannot classify a 1.7 runtime and cannot read the format-5 address library. One
  DLL still serves SE, AE and VR.
- **A Mod Configuration Menu**, as an optional installer component (#27). Five pages
  covering 28 of the 32 ini settings, each row carrying the explanation the ini gives it.
  The reported problem was that the settings existed and nobody found them, so no new
  knobs were added. Needs SkyUI and MCM Helper; a player happy with the built-in panel
  installs nothing new and loses nothing. Settings take hold when the journal closes —
  hotkeys are the exception and still need a restart.
- **The System notices what you actually do** (#30). Kills are tallied against the quarry
  types they match, and every `KillsPerBounty` (50) of a type pays `KillBountyPoints`.
  Harvesting, smelting, tanning, smithing, alchemy and enchanting count as one track:
  every `ProfessionActionsPerPoint` (25) pays a System Point. Taking materials out of the
  Dimensional Storage at a station does not count.
- **The right stick scrolls** (#2), closing the last surface a controller could not reach:
  the skill tree's mastery rail, where dragging panned the tree behind it.
- **Eight settings for the power curve** (#17, #32), in the ini and the MCM: each tier's
  skills, body and starting points, plus a multiplier on every skill-tree price and one on
  every skill-tree bonus.
- **`QuestRewardScale`** (#32), a multiplier on what an objective pays, applied when the
  objective is handed out — so an objective already standing keeps the number it was
  written with.
- **A public API other SKSE plugins can use** (#28). A versioned pure-virtual interface
  behind an exported `RequestPluginAPI`. Foreign plugins can read whether the System is
  active, the tier, points, rank and node ownership, and can grant points with an
  attributed source. No Papyrus surface — the mod stays script-free. Ships as the single
  dependency-free header `include/IsekaiHeroAPI.h`.
- **The Flameforged Oathblade**, the first thing this mod ships as a mesh rather than as
  code — generated geometry converted to a Skyrim NIF with collision, BSX flags and an
  attachment point built from measured numbers. **It cannot be bought in this release**;
  see *Changed*.

### Changed

- **The System no longer sets your character level** (#29, #17, and the pace half of #32).
  A player reported there was "no good way to set the difficulty" — enemies were either
  impossible and then trivial, or trivial from the start. Both halves were one fact: the
  blessing set the character level (150 for ASCENDED), Skyrim's levelled lists key off
  that level, and every scaling enemy was pulled to its ceiling in a single step.

  The blessing now grants a **body**: Health, Magicka and Stamina are raised *to* a target
  (180 for HERO, 600 for ASCENDED — what the old level worked out to), never past it and
  never twice. Your skills, attributes and perks make you stronger; the world stays where
  it is. DORMANT is fixed by the same change — it used to jump twice, and the second jump
  was the whole 150.

  This removes a reported bug on its way out: the granted level lived on the shared
  ActorBase, so once ASCENDED had written 150 there, loading a save the System had never
  touched left *that* character at 150 and fired level-gated quest mods.

  **The consequence to know about:** all 18 skills at 100 means no skill can rise, and
  Skyrim only grants levels for skill increases. An ASCENDED character stays at the level
  they reincarnated at. Level-gated content in other mods stays locked at that level.
- **ASCENDED starts with 10 System Points instead of 500.** The 500 were marked in the
  source as a test value and bought the entire 260-point tree twice over.
- **REBOOT refunds your skill tree** instead of carrying it over, at what each node
  actually cost, and the new prices then apply to an empty tree. No points are lost, only
  their allocation. It no longer restores a character level either — the blessing grants
  none, so writing the baseline back would confiscate every level lived since.
- **The threat frame is one line over one underline** (#26). The plate is gone. It drew
  the verdict colour three times over with magicka blue and stamina green competing
  underneath — four saturated things at once, which is what the report meant by "too many
  high-contrast elements competing". Now exactly one element carries colour: the
  underline, which is the health bar. Nothing was dropped, and the frame is shorter.
- **The Oathblade is out of the shop for now.** Its balance is not settled, so the shop
  card and the ARMAMENTS shelf it lived on are parked. The record, the mesh, the textures
  and the icon all still ship.

### Fixed

- **ESC closes the System panel again.** It only acted on a panel carrying exactly one
  text choice; the status panel's buttons are all icon-only, so the press fell through to
  Skyrim's own menu — the sound players heard under a panel that stayed open. With the
  reroll button switched on it was worse: ESC fired NEW TASK.
- **The menu guard really goes to the front of the handler chain**, instead of only when
  its registration happened to land last while the log claimed success either way.
- **A second Dimensional Storage chest no longer appears at your feet when you open it.**
  `MoveTo` hands the 3D attach to the engine's queue, so the check one line later read a
  perfectly healthy chest as orphaned and rebuilt it — and the fresh one dropped at the
  player's feet is the chest players saw appear. Contents were never at risk: each false
  rebuild migrated them.
- **The Dimensional Storage is your property.** Using it in an inn, a shop or a house was
  a crime — a bounty, items flagged stolen, witnesses turning hostile. A reference with no
  owner inherits the cell's, and the chest moves into whatever cell you stand in. Saves
  holding an unowned chest are repaired on the next open. Reported by two players
  independently. Items already flagged stolen stay flagged.
- **`UiScale` now reaches the threat labels in VR** (#26), the one surface it did not
  cover. **Untested — there is no VR install here.**
- **A deleted, empty weapon record left the plugin** — the shell the Creation Kit keeps
  after a delete, which made the editor warn about a bad reach value on a nameless weapon
  every session.
- `package.ps1` and the FOMOD now carry the `meshes` and `textures` folders.

### Notes

- **The VR in-headset layer ships off.** A VR player reported that installing this mod
  alongside ImGuiVRHelper sends Skyrim VR to the desktop as the mods finish loading —
  every time, on a load order of five mods, without writing a crash log. Their two logs
  place the correlation exactly, but **the cause is not known** and there is no VR install
  here to find it on. So `VRInHeadsetLayer` is new, and it is `0`: off, the mod behaves as
  the working log shows and ImGuiVRHelper can stay installed for other mods. Set it to `1`
  and you get the world-anchored threat labels, the HUD-plane notifications and the B+Y
  chord — and you are the person helping find this. Tracked as #35.
- **The repository is public**, and the licence now names its carve-outs: the sound
  effects (CC BY 4.0, author credited), 40 purchased icon frames, and the generated 3D
  assets. The remaining 34 icons are ours under the MIT terms.
- **The asset pipeline moved** to
  [dstNr/isekai-asset-pipeline](https://github.com/dstNr/isekai-asset-pipeline). The
  shipped meshes and textures stay here.
- If you already have an `IsekaiHero.ini`, your mod manager will ask whether to overwrite
  it. Keeping yours is safe — the nine new settings use their defaults. Take the new file
  if you want them, or install the MCM and never edit an ini again.
