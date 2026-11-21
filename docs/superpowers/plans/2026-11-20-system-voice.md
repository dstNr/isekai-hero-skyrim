# System Voice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The System speaks short English lines at twelve key moments, in one ElevenLabs voice with several delivery styles, played through the game's sound system (Voice slider) and switchable off.

**Architecture:** Lines are generated with the ElevenLabs connector (controller only, it spends credits), converted to WAV and shipped under `Sound\fx\isekai\voice\`. Each moment (and each blessing and rank) gets one SNDR in `IsekaiHero.esp` listing all its variant files; the engine picks one at random. A new `Voice` module resolves the descriptors like `Sounds.cpp`, decides with pure logic (`VoiceLogic.h`, unit-tested) whether a line may play now, and plays it delayed past the UI resume pass. The twelve existing code paths call `Voice::Say`.

**Tech Stack:** C++ / CommonLibSSE-NG (one DLL, SE/AE/VR, 1.6.1170–1.7.104), hand-built light ESP, ffmpeg, ElevenLabs connector (`eleven_v4`), Node `tools/check.mjs`, PowerShell packaging.

**Spec:** `docs/superpowers/specs/2026-11-20-system-voice-design.md`

**Branch:** `feat/system-voice`, cut from `feat/system-enchantments` @ bc7e93b (both add records to `IsekaiHero.esp`; the voice records start after the enchantments' last FormID 0xDB6).

## Global Constraints

- Voice ID `kdmDKE6EkgrWrrykO9Qt`, model `eleven_v4`, one voice throughout. Generation only by the controller session, never a subagent.
- English lines; no subtitles. Text the player reads (MCM) stays translatable.
- No lines on window open, shop purchase or skill-tree purchase.
- Playback only through SNDR descriptors (category Voice), never raw WAV playback.
- `SystemVoice` ini and MCM switch, default on.
- `IsekaiHero.esp` stays light, no new master; new local FormIDs from `0xDB7`, all ≤ `0xFFF`.
- WAV: PCM 16-bit, mono, 44.1 kHz, peak-normalised to −1 dBFS, metadata stripped; no path or name of the owner in any file.
- `node tools/check.mjs`, `pwsh -NoProfile -File tools/run-unit-tests.ps1` and `build.bat` (`BUILD_OK`) must pass.
- The README credits ElevenLabs. Audio is generated while the paid plan is active.
- Commits in English, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Never `--no-verify`. No push without the owner.

## Review Focus

1. **A level-up that also awakens DORMANT:** one line plays (the awakening), not two on top of each other. → `ShouldSpeak` priority test (Task 4).
2. **Loading a save with a higher rank than the last one:** no "rank increased" line. → `RankRose` test with a reset (Task 4).
3. **A fight with many LETHAL enemies:** at most one lethal line per cooldown, not one per enemy. → `LethalShouldSpeak` test (Task 4).
4. **Voice switched off in the MCM mid-game:** silent from the next event. → `Say` checks `Config::SystemVoice()` on every call (Task 4 test of the gate in logic, Task 6 in game).
5. **A line right after a System window closes:** not swallowed by the engine's resume pass. → every `Say` is delayed at least `Sounds::kResumeGraceMs` (Task 4).

## The lines (data for Tasks 2 and 3)

Styles: **plain** (no tag), **solemn** (`[solemnly]`), **cold** (`[coldly] [clipped]`), **impressed** (`[faintly impressed]`). Final wording may change in Task 1 by the owner's ear.

| SNDR | Moment | Style × text |
|---|---|---|
| reincarnation | System boots | solemn: "System initializing... Soul signature confirmed. Welcome, Reincarnator." · solemn: "Boot sequence complete. A new soul has been registered. Welcome to your second life." |
| blessing_normal | Blessing NORMAL | plain: "No blessing selected. You will walk this world as you are. The System will be watching." |
| blessing_hero | HERO | solemn: "Hero blessing accepted. Your new life begins now." |
| blessing_ascended | ASCENDED | solemn: "Ascended blessing accepted. Your limits... have been removed." |
| blessing_dormant | DORMANT | cold: "Dormant blessing accepted. Its power sleeps. For now." |
| blessing_custom | CUSTOM | plain: "Custom configuration accepted. Your new life begins now." |
| levelup | Level up | each of plain, solemn, cold, impressed × "Level up." / "Level up. Your limits have been raised." / "Level increased. Growth confirmed." / "Experience threshold reached. Level up." (16) |
| milestone | Quest Complete | plain, cold, impressed × "Quest complete. Reward granted." / "Objective achieved. Rewards distributed." / "Milestone recorded. The System acknowledges your progress." (9) |
| rank_d … rank_s | Rank up | solemn: "System rank increased. Rank D." … "Rank A."; S: "System rank increased. Rank S. Maximum authority granted." (5) |
| quest_new | New System quest | plain, cold, impressed × "New quest issued." / "A new objective has been assigned." / "Quest received. Do not disappoint." (9) |
| quest_done | Quest fulfilled | plain, cold, impressed × "Objective fulfilled. System Points awarded." / "Quest complete. Points transferred." / "Target eliminated. Reward issued." (9) |
| dormant | DORMANT awakens | solemn: "Dormant blessing... awakening." · solemn: "The sleeping power stirs. Dormant blessing awakened." |
| reboot | REBOOT | cold: "System reboot. Rewriting fate." · cold: "Rebooting... Previous configuration erased." |
| lethal | LETHAL threat | plain, cold, solemn × "Warning. Lethal threat detected." / "Danger. This enemy exceeds your limits." / "Lethal threat. Retreat is recommended." (9) |
| guild | Guild promotion | plain: "Guild rank recognized." · impressed: "Guild promotion confirmed. Your standing has risen." |
| cosave_lost | Lost co-save | cold: "Error. Soul record not found." |

20 SNDRs, 71 files.

---

### Task 1: Voice samples and the owner's choice (controller)

**Files:** `tools/voice-convert.ps1` (create), samples under the scratchpad.

- [ ] **Step 1:** Generate "Level up. Your limits have been raised." once per style (4 generations, `generations_count: 1`, one flow named "Isekai System Voice"), poll, download each `full_res_url` with curl into the scratchpad.
- [ ] **Step 2:** Write `tools/voice-convert.ps1 -In <dir> -Out <dir> [-Filter]`: ffmpeg to mono 44.1 kHz s16 WAV, peak −1 dBFS (two-pass: `volumedetect`, then `volume`), `-map_metadata -1 -fflags +bitexact`; `-Filter` adds a light digital character (`highpass=f=140,acrusher=bits=12:mix=0.15,aecho=0.6:0.4:9:0.15`). Fails loudly when ffmpeg is missing.
- [ ] **Step 3:** Convert the four samples plain and filtered; send all eight to the owner (SendUserFile) with one question: which styles stay, filter yes or no, any wording change.
- [ ] **Step 4:** Ledger the owner's answer as the binding style and filter choice. Commit the script.

### Task 2: Generate, check and convert all lines (controller)

**Files:** `voice/lines.tsv` (create: `sndr, file, style, text`), `sounds/voice/*.wav` (create), `voice/README.md` (generation date, model, voice ID, plan tier; no account data).

- [ ] **Step 1:** Write `voice/lines.tsv` from the table above with Task 1's changes; file names `<sndr>_<nn>.wav`.
- [ ] **Step 2:** Estimate the whole run (`estimate_only`), then generate every line once (`generations_count: 1`) on the same flow; never re-call to retry a running generation.
- [ ] **Step 3:** Download, then transcribe every take (`creative_transcribe_audio`) and compare with the text (case and punctuation ignored); a mismatch or a clipped/odd take is regenerated once and ledgered.
- [ ] **Step 4:** Convert with `tools/voice-convert.ps1` (filter per Task 1) into `sounds/voice/`; scan the WAVs for `Users`, the owner's name and `C:\`; check duration (each < 8 s) and format with ffprobe.
- [ ] **Step 5:** Commit "feat(voice): the System's lines, generated and converted".

### Task 3: Twenty SNDR records (Part N)

**Files:** `plugin/IsekaiHero.esp`, `docs/CREATION_KIT_ESP.md` (new Part N).

**Interfaces:** Produces local FormIDs `0xDB7`–`0xDCA` in the table order above (reincarnation 0xDB7, blessing_normal 0xDB8, blessing_hero 0xDB9, blessing_ascended 0xDBA, blessing_dormant 0xDBB, blessing_custom 0xDBC, levelup 0xDBD, milestone 0xDBE, rank_d 0xDBF, rank_c 0xDC0, rank_b 0xDC1, rank_a 0xDC2, rank_s 0xDC3, quest_new 0xDC4, quest_done 0xDC5, dormant 0xDC6, reboot 0xDC7, lethal 0xDC8, guild 0xDC9, cosave_lost 0xDCA); editor IDs `IsekaiVOX_<sndr>`.

- [ ] **Step 1:** Read a vanilla voice-category SNDR from Skyrim.esm (read-only: `housecarl_records` or a parser) for the Voice category (GNAM) and output model (ONAM); ledger both FormIDs.
- [ ] **Step 2:** Write the 20 SNDRs byte-wise, built like `0xD7B` (Part E) but with that category and output model and one `ANAM` per variant (`fx\isekai\voice\<file>`), random selection. Update the group size, `HEDR` record count and next-object ID.
- [ ] **Step 3:** Read back: 20 SNDRs, every ANAM path has a file in `sounds/voice/`, ESL flag and masters unchanged, every byte outside the new group/header fields identical to the base.
- [ ] **Step 4:** Part N in `CREATION_KIT_ESP.md` (table and how it was built). Commit "feat(esp): voice descriptors for the System".

### Task 4: The Voice module, its logic and the switch

**Files:** `src/VoiceLogic.h`, `tests/voice_logic_tests.cpp` (+ `tests/CMakeLists.txt`), `src/Voice.h/.cpp`, `CMakeLists.txt`, `src/Config.h/.cpp`, `IsekaiHero.ini` (+ `node tools/make-presets.mjs`), `mcm-patch/MCM/Config/IsekaiHero/config.json` and `settings.ini`, `src/SelfTest.cpp`, `tools/check.mjs`, `package.ps1`.

**Interfaces:**
- `VoiceLogic.h` (namespace `Isekai::Voice::Logic`): `bool ShouldSpeak(double now, double lastAt, int lastPriority, int priority, double minGap)` — true when `now − lastAt ≥ minGap` or `priority > lastPriority`; `bool RankRose(int prev, int now)` — false when `prev < 0` (unknown after load/new game); `bool LethalShouldSpeak(double now, double lastLethalAt, double cooldown)`; constants `kMinGapSeconds = 4.0`, `kLethalCooldownSeconds = 180.0`.
- `Voice.h` (namespace `Isekai::Voice`): `enum class Line { Reincarnation, BlessingNormal, BlessingHero, BlessingAscended, BlessingDormant, BlessingCustom, LevelUp, Milestone, RankD, RankC, RankB, RankA, RankS, QuestNew, QuestDone, Dormant, Reboot, Lethal, Guild, CoSaveLost }` (same order as the FormIDs); `void Install()` (kDataLoaded); `std::pair<size_t,size_t> Resolved()`; `void Say(Line, std::uint32_t a_delayMs = Sounds::kResumeGraceMs)`; `void ResetSession()` (load/new game: forget last line and rank).
- `Config::SystemVoice()`.

- [ ] **Step 1: Write the failing tests** in `tests/voice_logic_tests.cpp`: `ShouldSpeak` blocks within the gap at equal priority, allows a higher priority inside the gap (Review Focus 1), allows after the gap; `RankRose(-1, 3) == false`, `RankRose(2, 3) == true`, `RankRose(3, 2) == false` (Review Focus 2); `LethalShouldSpeak` false inside 180 s, true after (Review Focus 3).
- [ ] **Step 2:** `pwsh -NoProfile -File tools/run-unit-tests.ps1` → the new test fails to compile/link.
- [ ] **Step 3:** Implement `VoiceLogic.h`; tests pass.
- [ ] **Step 4:** Implement `Voice`: FormID table scraped-compatible with check.mjs (`0x000DB7, // IsekaiVOX_reincarnation` style), lookup via `Plugin::LookupOurForm<RE::BGSSoundDescriptorForm>`; `Say` returns at once when `!Config::SystemVoice()` (checked per call, Review Focus 4), applies `ShouldSpeak` with priorities (CoSaveLost, Reincarnation, Blessing*, Reboot = 3; Dormant, Rank* = 2; everything else = 1), plays via `DelayedMainThread` with at least `kResumeGraceMs` (Review Focus 5), keeps the `BSSoundHandle` so a higher-priority line stops the running one. Real time from `std::chrono::steady_clock`.
- [ ] **Step 5:** `SystemVoice` end to end like `ThreatLabels`: ini key under a `[Voice]` comment block, Config field/Apply/Load reset/accessor, MCM toggle `bSystemVoice:Voice` (text "System voice"), `settings.ini` default 1, presets regenerated.
- [ ] **Step 6:** Self-test row "System voice descriptors N/20 resolved" (non-critical). `check.mjs`: scrape `Voice.cpp` like `Sounds.cpp`, need 20, every ID an SNDR in the ESP, every ANAM file present in `sounds/voice/`; `package.ps1` stages `sounds/voice/*.wav` to `Sound\fx\isekai\voice\`.
- [ ] **Step 7:** `build.bat` → `BUILD_OK`; `node tools/check.mjs`; unit tests 100 %. Commit "feat(voice): the Voice module and its switch".

### Task 5: The twelve triggers

**Files:** `src/System.cpp`, `src/Progression.cpp`, `src/Quests.cpp`, `src/ModAPI.cpp`, `src/UI/ThreatLabels.cpp`, `src/GuildBridge.cpp`.

**Interfaces:** Consumes `Voice::Say`, `Voice::ResetSession`, `Voice::OnRank(int)`, `Voice::SayLethal()` (Task 4 fix round 1: Voice owns the rank and lethal-cooldown state).

Call sites (from the code map):
- Reincarnation: `BeginReincarnation()` after `g_state.reincarnated = true;` (System.cpp ~675).
- Blessing: `ApplyReincarnation()` (System.cpp ~234), line chosen from `power`, `dormant`, `custom` (NORMAL/HERO/ASCENDED/DORMANT/CUSTOM).
- Level up: `MenuWatcher::ProcessEvent(LevelIncrease)` (System.cpp ~775), before `CheckDormantAwakening()`.
- Milestone: `CelebrateMilestone()` inside `if (Grant(...))` (Progression.cpp ~330), delay 900 ms (after the LevelUp sting).
- Rank: `PublishIfChanged()` (ModAPI.cpp ~139) calls `Voice::OnRank(RankValue)`; Voice decides with `RankRose` and `ResetSession` sets the previous rank unknown.
- New quest: `Quests::Roll()` at the toast (~191), not for `Reroll()` by the player's button.
- Quest done: `Quests::Complete()` after the LevelUp sting (~200), delay 900 ms.
- Dormant: `AwakenTo()` (System.cpp ~595), delay 900 ms.
- REBOOT: `RebootSystem()` before `ShowPowerSelection()` (~1417).
- Lethal: `ThreatLabels.cpp` `SelectLabels()` after `VerdictFor` (~312): edge per actor (a set of FormIDs announced this session, cleared when the player levels), then `Voice::SayLethal()` (the cooldown lives in Voice). Only while threat labels run (builder's call in the spec: the line belongs to the threat reading).
- Guild: `GuildBridge.cpp` `Pay()` on `AG_RankChanged`.
- Lost co-save: `CheckCoSave()` after `g_stateLost = true;` (~1213), delay 2000 ms.
- `ResetSession()` on `kPostLoadGame` and `kNewGame`.

- [ ] **Step 1:** Wire the calls; no other behaviour changes.
- [ ] **Step 2:** `build.bat` → `BUILD_OK`, `node tools/check.mjs`, unit tests 100 %.
- [ ] **Step 3:** Commit "feat(voice): the System speaks at its twelve moments".

### Task 6: README, test list, install (controller)

**Files:** `README.md` (Voice section + ElevenLabs credit), `CHANGELOG.md` unreleased entry, `docs/superpowers/plans/2026-11-20-system-voice-testlist.md`.

- [ ] **Step 1:** README and changelog.
- [ ] **Step 2:** Test list, each point measurable through the test bridge (`game_audio` / `game_audio_level` for playback): V1 every moment speaks once (levels via console, `isekai` commands for quests/milestones/rank/reboot); V2 level-up plus dormant awakening → one line; V3 reload with a higher rank → silent; V4 three LETHAL enemies → one line within 180 s; V5 MCM off → silent, on → speaks; V6 the Voice slider changes the loudness, the effects slider does not; V7 self-test row 20/20; V8 no line on window open, shop or tree purchase.
- [ ] **Step 3:** Hand to the SotN project lead for a collected run; install only through the project lead with the owner's yes.
