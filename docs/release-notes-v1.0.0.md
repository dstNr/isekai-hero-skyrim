What is uploaded to Nexus as **1.0.0**.

1.0.0 is the first release out of the `0.x` pre-release line. The interface is new: every
screen is now drawn by Magelight UI. The Nexus feedback on 0.9.0 is in as well: a lost
co-save is recognised, the first blessing choice is simpler, harvesting pays less, and
there are Adventurers Guild points.

**New requirement: [Magelight UI](https://www.nexusmods.com/skyrimspecialedition/mods/193303)
1.7.0 or newer.** Install it first. Without it the mod does not start the reincarnation: it
shows one note and waits. PrismaUI and the ImGui overlay are no longer needed.

**Saves carry over.** The co-save stays at version 16, the same as 0.9.0.

Archive: `IsekaiHero-v1.0.0.7z`. It is one FOMOD with the main mod and the optional MCM.
The installer no longer asks how the interface is drawn.

**Test status.** Played in a full modlist on 1.6.1170, with these results:

- Passed:
  - the Magelight screens and the blessing choice
  - the storage chest (#38)
  - the readiness gate (#36, flat screen)
  - the Ordinator token fix
  - the milestone payout: a finished quest (Take Up Arms) pays its milestone and opens the gold panel
- Not confirmed in game:
  - the hotkey rebind after the MCM closes (#37)
  - anything in a VR headset

### Added

- **Adventurers Guild support.** Guild missives, cleared dungeons and promotions pay
  System Points by their rank (`AdventurersGuildPoints`, ini and MCM, 0 = off). No
  dependency: without the guild the events never come.
- **A lost co-save is caught.** A save that lost its `.skse` file is no longer greeted as
  a new soul. The log and a HUD line say so, and the automatic reincarnation waits.
- **`AllSpellsExclude`** (ini) keeps chosen plugins or single spells out of the every-spell
  node.
- **`HarvestsPerAction`** (ini and MCM, default 4): picking plants paid far faster than any
  crafting station.
- **Machine-readable self-test lines** (`[SELFTEST] isekai.<id> PASS|FAIL`) and two Papyrus
  natives:
  - `IsekaiHeroCraft.RunSelfTest()`
  - `IsekaiHeroCraft.DebugGrantNext()`, for test runs only: it pays out the next owed
    milestone.
- **`IsekaiHeroCraft.IsStorageTransfer(ObjectReference)`** lets item listeners skip the
  storage's own moves. Ordinator patches use it.
- **The `IsekaiHero_RebirthComplete` ModEvent** is sent after every rebirth.
- **The English language template** now really ships, in
  `SKSE\Plugins\IsekaiHero\lang\template.txt`.

### Changed

- **The interface is drawn by Magelight UI** in a holographic "projection" look. It
  replaces ImGui and PrismaUI. One DLL still serves SE, AE and VR.
- **The first choice is NORMAL or TRANSMIGRATOR.** HERO, ASCENDED, DORMANT and CUSTOM are
  one step in, with a way back.
- **Controller:** the pad drives the pages by focus through Magelight's controller layer.
- **VR threat labels** are a head-locked scan list. VR stays experimental and untested.
- **The reboot confirmation** cancels on Escape, pad B or a lost window.

### Fixed

- **Loading a save without its co-save no longer deletes the storage chest.**
- **Ordinator's "Recycle Materials" and "Fuel the Inferno"** no longer react to the
  storage handing crafting materials back.
- **A rebound hotkey takes hold when the MCM closes** (#37). An unmapped key now means no
  key.
- **The storage chest has nothing left to see** (#38). Its model is empty, so it stays
  invisible even when the game leaves it near the player.

### Removed

- The ImGui overlay, the PrismaUI bridge and patch, and the ImGui VR Helper layer. The ini
  keys `VRInHeadsetLayer` and `VRThreatLabelHeight` are retired. An old ini that still
  has them loads as before.

Full technical detail: [CHANGELOG.md](../CHANGELOG.md#100--2026-11-18).
