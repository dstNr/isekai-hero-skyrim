# A Mod Configuration Menu, through MCM Helper

**Issue:** #27 — "The ini settings exist but players never find them"

## The problem, restated

Every setting a player asked for already exists. `UiScale`, the whole `ThreatLabel*` block,
the hotkeys, `FirstTaskHours`, `TaskIntervalHours`, and now `QuestRewardScale`,
`KillsPerBounty` and `ProfessionActionsPerPoint`. The reporter was annoyed that the F10
threat-label toggle does not survive a restart — which is documented behaviour, and
`ThreatLabels = 0` gives them exactly what they wanted, permanently.

They had no way to learn that from inside the game. **This is a discoverability problem
before it is a settings problem**, so the first job is surfacing what is already
configurable, not adding knobs.

## Goals

- Every setting in `IsekaiHero.ini` reachable from inside the game, with its explanation.
- A player who never opens a text editor can configure the mod.
- Changes take effect without a restart where the setting allows it.

## Non-goals

- **New settings.** This exposes what exists. Balance multipliers that do not exist yet are
  #17 and #29, and they belong to that design pass, not this one.
- **Replacing the ini.** The ini stays the file of record and keeps its comments — they are
  the long-form documentation an MCM row has no space for.
- **An MCM for the PrismaUI view.** That view has its own surface; this is about the game's
  own menu system.

## Why MCM Helper

A classic MCM is what players expect and look for, and
[MCM Helper](https://github.com/Exit-9B/MCM-Helper) (MIT, 1.6.3, last updated 2026-08-26)
means it costs no Papyrus of our own: the menu is declared in JSON and MCM Helper's own
script does the work.

Checked against the documentation rather than assumed, because the answer decides whether
this mod's script-free promise survives:

| | |
|---|---|
| A custom Papyrus script | **Not needed.** MCM Helper's script attaches to our quest. |
| A quest record in the ESP | Needed. Creation Kit work — see "What has to be done by hand". |
| Menu definition | `Data/MCM/Config/IsekaiHero/config.json` |
| Default values | `Data/MCM/Config/IsekaiHero/settings.ini` |
| Where the player's choices land | `Data/MCM/Settings/IsekaiHero.ini` |
| SkyUI | Hard requirement of MCM Helper |

The README's claim that the mod "ships no scripts" stays literally true. What changes is
that a scripted framework becomes a runtime requirement — which is why it is optional.

## It is an optional component

Shipped the way the PrismaUI view already is: a FOMOD option, off by default. Picking it
installs `config.json` and `settings.ini`; not picking it leaves a mod that still has no
dependency beyond SKSE.

Without this, every player who is happy with the built-in panel would be made to install
SkyUI and MCM Helper to run a mod they were already running. The FOMOD pattern for exactly
this case exists (`tools/make-fomod.mjs`, the PrismaUI plugin block), so it costs a block,
not an invention.

## How the settings actually reach the plugin

**No MCM Helper API is used.** MCM Helper writes a plain ini, and `Config` already parses
one. So:

1. `Config::Load` reads `Data/SKSE/Plugins/IsekaiHero.ini` as it does today. This is the
   floor: documented defaults, full comments, the file of record.
2. It then reads `Data/MCM/Settings/IsekaiHero.ini` **if it exists**, and every key found
   there overrides the value from step 1. That file is what the player just clicked, so it
   wins.
3. A key absent from the MCM ini is not an instruction to reset — it falls through to the
   value from step 1.

The precedence is one-directional and there is no write-back: the plugin never edits either
file. Two writers on one file is how settings silently revert, and nothing here needs it.

**Reload.** `Config::Load` is re-run when the MCM closes, off the `MenuOpenCloseEvent` sink
that already exists. Settings read once at load and cached on the render thread
(`Style::g_userScale`) are re-pushed there, as the existing load path already does.

Settings whose effect cannot be re-applied live — none today, but hotkeys are the obvious
future case — are documented in the MCM row itself rather than silently doing nothing.

## What the menu contains

One page per ini section, in the ini's own order, so the two documents can be read against
each other:

| Page | Rows |
|---|---|
| Interface | `UiScale`, `Language`, `TextSpeed`, `HideSealedNodes` |
| Hotkeys | `SystemMenuKey` + modifier, `GamepadMenuButton` + modifier, `VRMenuButton`, threat-label key |
| Threat labels | `ThreatLabels`, `ThreatLabelTargets`, `ThreatLabelNumbers`, `ThreatLabelResources`, `ThreatLabelRange`, `VRThreatLabelHeight` |
| Quests | `FirstTaskHours`, `TaskIntervalHours`, `QuestRewardScale` |
| Progression | `KillsPerBounty`, `KillBountyPoints`, `ProfessionActionsPerPoint` |
| Diagnostics | `QuestRerollButton`, `SelfTestKey`, `DebugPointsKey`, `LogInputDiagnostics` |

Every row carries the ini comment's first sentence as its help text. That is the whole
point of the ticket: the explanation travels with the setting.

The ranges and defaults are the ones `Config.cpp` already clamps to, taken from there
rather than written again — a row that offers a value the parser refuses is worse than no
row.

## What has to be done by hand

The Creation Kit part cannot be scripted from here:

1. In `IsekaiHero.esp`, create a quest — suggested editor ID `IsekaiHeroMCM`. Start Game
   Enabled, not run-once.
2. Attach MCM Helper's config script to it.
3. Save. **The plugin is ESL-flagged**, so the new FormID must land under `0xFFF`; the
   highest in use today is `0xd8f`, so there is room, but check it after saving — the
   Creation Kit has blanked object bounds and mis-assigned FormIDs in this plugin before
   (`docs/CREATION_KIT_ESP.md`).

## Verification

- With the component installed and SkyUI + MCM Helper present: the menu appears, every page
  lists its rows, and each row shows its current value from the ini.
- Change `UiScale` in the MCM, close it: the panel scales without a restart.
- Change nothing and close: `Data/MCM/Settings/IsekaiHero.ini` may exist, and every value in
  the game still matches the shipped ini.
- Delete `Data/MCM/Settings/IsekaiHero.ini` and load: every value falls back to the shipped
  ini, no errors.
- **Without** the component: the mod loads and behaves exactly as before, with no reference
  to SkyUI or MCM Helper in the log.
- A `git grep` for the author's name runs before each commit.
