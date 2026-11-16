# Roadmap after 0.9.0: fix, reach, then shape

**Date:** 2026-10-30
**Status:** roadmap approved. Each release below gets its own implementation plan before
code; the reward release gets its own design spec first.
**Version numbers** are the ones proposed when this was agreed. They name milestones only —
`CMakeLists.txt`, `package.ps1` and the tags move when a release is actually cut.

## Where things stand

0.9.0 shipped on 2026-09-17 and nothing has landed since. Its milestone holds eight issues,
every one code-complete and none confirmed in game. The bottleneck is verification, not
code — so the first release below is small on purpose, and its test pass doubles as the
0.9.0 verification run (`docs/MANUAL_TESTS.md`).

Two direction choices shape the rest: **accessibility and VR** next, then **rewards with a
shape**. "The System in the world" (#31, #33, #34) and new systems (#14, #15, New Game+)
stay in the Inbox.

## 0.9.1 — the bugs players are hitting

### Rebound hotkeys do nothing until a restart (new)

A player rebinds a key in the MCM and the old key keeps working. The cause is confirmed in
the code: closing the journal re-reads both inis (`MenuWatcher` → `ReloadSettings`,
`System.cpp`), but hotkeys are registered once at load from four modules (`Progression`,
`ThreatLabels`, `SelfTest`, `System`) into `g_hotkeys` in `UI/Input.cpp`, and nothing
registers them again. The comment in `ReloadSettings` says the MCM help text warns about
this; it does not, and neither does the ini.

Fix: make the hotkey table rebuildable. A reload clears the keyboard and gamepad entries
and every module registers again from `Config`. The table is read on the input thread and
rebuilt on the main thread, so the rebuild builds a new table and swaps it in under a lock
rather than mutating the live one. Correct the comment. If any binding still needs a
restart after this (the VR wand button is the candidate), the MCM help text and the ini
say so.

The report mentions keys stuck on "NumPg 9 and 8", which is none of our defaults (S + Right
Shift, F10). The `Config:` line in the reporter's log settles which codes were actually
loaded; ask for it rather than guess.

### The Dimensional Storage chest stands next to the player (new)

Reported after 0.9.0, whose fix (387d177) closed one path to the same symptom. Players see
it indoors and outdoors, sometimes after opening the chest several times; changing cells
does not clear it, reloading sometimes does.

Hypothesis, not yet proven: `Storage::Open()` calls `MoveTo(player)` and then sinks the
chest with `SetPosition(z - 3000)`. The code's own comment says `MoveTo` hands part of its
work to the engine's queue. If the queued move lands after the `SetPosition`, the chest
ends up at the player's height.

Plan, in order:

1. **Prove it.** Log the chest's position and cell after the activation, and on the next
   menu close. One log from a player who sees the chest decides it.
2. **Make it invisible regardless.** Give the CONT record an invisible model in the ESP.
   Wherever the reference ends up, there is nothing to see or to bump into. This is the
   fix that holds even if the hypothesis is wrong.
3. **Only if 1 shows the position cannot be held:** park the chest permanently in a
   holding cell and open it remotely, the pattern other storage mods use — the reporter's
   own suggestion. Bigger change, because the crafting token-lending assumes the chest is
   reachable from the player's cell; that has to be re-checked first.

### #36 — VR: the Awakening never passes the ready check

A VR player's log shows the hotkey recognised and the awakening refused as "not ready",
forever. `IsPlayerReady()` (`System.cpp`) checks five things and logs none of them.

1. Log which condition failed, once per change rather than every frame.
2. The leading suspect is `ControlMap::IsMovementControlsEnabled()` reading a VR layout
   wrong and reporting movement as locked. If the log confirms it, VR gets its own
   readiness test for that condition.

No VR install here, so this ships as a diagnostic build first and is verified by the
reporter.

While in there, the trigger code (`IsPlayerReady`, `TryTrigger`, the chargen-room
detection) moves out of `System.cpp` (1456 lines) into its own file. That is the only
structural change in this release, and only because the fix touches it.

### Carried, no code

- **#35** (VR crash with ImGui VR Helper) waits on the reporter's run with
  `VRInHeadsetLayer = 0`.
- **#6** (storage codex) waits on the test pass.
- **#26** is retitled to what is still true — the threat-label frame is too busy — and
  moves to 0.10.0. Its original premise (no scaling) was disproved in its own comments.

## 0.10.0 — reach more players

### CJK glyphs in the built-in interface (new)

A translator asked to publish a Chinese translation. The ImGui overlay loads only Consolas
(`UI/Overlay.cpp`, `UI/VROverlay.cpp`), which has no CJK glyphs, so Chinese text renders as
empty boxes. Only the PrismaUI patch shows it.

Fix: when `Language` is a CJK code (`zh`, `ja`, `ko`), merge a Windows system font into the
atlas for the missing glyphs — Microsoft YaHei, Yu Gothic, Malgun Gothic. Nothing is
redistributed, the same rule Consolas follows. Both overlays. The ini note that warns about
blank boxes goes, and the Nexus description says CJK works.

### #9 — toasts and threat labels in PrismaUI

Decide, then build or close. It matters more now: VR menus already run through PrismaUI,
and the in-headset ImGui layer is off by default after #35.

### #26 — the threat-label frame

Rework what the frame draws so it earns its space mid-fight, with `UiScale` as already
applied.

### Not in this release

**#7** (VR phase 2) stays blocked on #35's answer and on having no VR install to test with.

## 0.11.0 — rewards with a shape

The power-curve spec (`2026-09-13-power-curve-design.md`) fixed the curve and explicitly
deferred the rest: *"The nine orthogonal effects from #29's body … #16 belongs with them."*
This release is that content pass, and it is gated on the 0.9.0 curve being confirmed in
game — content on top of a curve that does not work yet would be tuned against the wrong
numbers.

It gets its own design spec before any code, covering at least:

- the orthogonal effects #29's body lists, and which of them earn a node;
- **#16** damage nodes — spell and melee damage have no global actor value, so this is a
  perk or magic-effect route, and that choice is the spec's main decision;
- **#20** Poison Resist, the one resistance the tree never covers.

## The board

- **#36** goes on the board; it was created without `item-add`.
- A **"Needs in-game test"** status sits between Confirmed and Done, so "code-complete,
  unverified" stops sharing a column with work still to do. The seven 0.9.0 issues left
  after #26 moves out (#2, #17, #27, #28, #29, #30, #32) and #6 move there.
- One milestone per release above.

## Deliberately not done

**A `src/` reorganisation into folders.** It would touch `CMakeLists.txt` and about thirty
hard-coded paths in `tools/check.mjs` without fixing anything. Files are split where a fix
already touches them, as with the trigger code in 0.9.1.

## Update 2026-11-16: Magelight UI and the Nexus feedback

**The interface moved to Magelight UI** (`feat/magelight-ui`), with the 0.9.1 fixes on the same
branch: rebound hotkeys take hold on menu close, and the storage chest has an empty mesh.
That retires two 0.10.0 items:

- **CJK glyphs in the built-in interface.** There is no ImGui atlas any more. `theme.css`
  falls back to Microsoft YaHei UI and Yu Gothic UI, and the ini note says glyphs no
  installed font covers come out as boxes. What is left is an in-game check with a `zh`
  language file.
- **#9, toasts and threat labels in PrismaUI.** Both live on the Magelight HUD page now.

**Nexus feedback, acted on** (`feat/nexus-feedback`, on top of Magelight):

- A save whose co-save did not come back is caught, not greeted as a new character. Loading
  one used to delete the storage chest with its contents.
- The blessing choice asks NORMAL or TRANSMIGRATOR first.
- Harvests count four to one profession action (`HarvestsPerAction`).
- `AllSpellsExclude` keeps plugins or spells out of the every-spell node.
- Adventurers Guild work pays System Points (`GuildBridge`, from the guild's SKSE mod
  events).
- Creation Club ingredients stay out of the material packs. The Nexus page says why.

**Into the Inbox:** other kinds of System (a player asked for more than one System flavour).
This goes with "new systems" (#14, #15, New Game+). It needs its own spec before any code,
because every panel, the skill tree and the milestone table assume the one System there is.
