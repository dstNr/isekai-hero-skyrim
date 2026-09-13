# Power Curve Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop the blessing from setting the player's character level, so Skyrim's levelled
lists stay where they are and the player overtakes the world instead of dragging it along.

**Architecture:** The blessing's `playerLevel` field is replaced by an `attrTarget` — Health,
Magicka and Stamina are raised *to* a number rather than by one derived from a level. The
whole mechanism that held a non-persistent level up across saves is deleted. Eight values
move into the ini (#17), of which the node price scale is snapshotted into the co-save at
reincarnation because refunds are computed from the live price. A one-shot co-save v16
migration puts existing characters back on the level they had before their blessing.

**Tech Stack:** C++23, CommonLibSSE-NG v7.5.4 (submodule at `extern/`), SKSE co-save
serialization, `node tools/check.mjs` as the test harness, MCM Helper for the in-game surface.

**Spec:** [`docs/superpowers/specs/2026-09-13-power-curve-design.md`](../specs/2026-09-13-power-curve-design.md)

## Global Constraints

- **Never bump the mod version.** `CMakeLists.txt` VERSION, `package.ps1`'s default
  `-Version`, git tags and `CHANGELOG.md` headings stay at 0.8.0. New entries go under
  `## [Unreleased]`.
- **Everything committed is in English** — code, comments, commit messages, docs.
- **No committed file may contain the author's real name.** Run
  `git grep -n -i "dustin\|reymendt"` before every commit; it must print nothing.
- **Co-save is append-only.** Every version has only ever added fields at the end, each read
  behind a `version >= N` gate. Co-save goes to **v16**; older saves must stay readable.
- **`node tools/check.mjs` must pass.** It is at 48/48 before this plan starts. Three existing
  checks do most of the guarding here and are used as the failing tests:
  `config: the ini and Config::Load name the same settings`,
  `co-save: kVersion matches the highest version gate`,
  `co-save: every state field written is also read`.
- **ini keys must be lowercase letters only.** The config check matches `key == "([a-z]+)"` —
  a digit or underscore in a key makes it invisible to the check.
- **Build with `build.bat`.** It deploys into the base game's `Data\` — that does **not** reach
  the MO2 test setup. In-game verification needs `package.ps1` and a reinstall in MO2.
- **The numbers, verbatim from the spec:** HERO — skills 50, attribute target 180, System
  Points 5, perks 10, gold 2000, souls 3. ASCENDED — skills 100, attribute target 600, System
  Points 10, perks `kMaxPerkPoints`, gold 25000, souls 20. Both multipliers default to 1.0.
- **`NodeEffectScale` is live; `NodeCostScale` is snapshotted at reincarnation.** Not a
  preference — `RespecRefund` reads `Node::cost` as it stands now, and the surrounding code
  exists to refund exactly what was paid.

## File Structure

| File | Responsibility in this change |
|---|---|
| `IsekaiHero.ini` | Documents the eight new settings in a `[Progression]` block |
| `src/Config.h` | Declares the eight accessors |
| `src/Config.cpp` | Globals, parser arms, defaults reset, the startup log line |
| `src/System.h` | `State::nodeCostScale`, `State::levelGrantRetired` |
| `src/System.cpp` | `Blessing::attrTarget`, `BlessingFor`, `ApplyBlessing`, deletion of the level machinery, co-save v16, `RetireLevelGrant` |
| `src/SkillTree.cpp` | `NodeCost()` as the one price path; `NodeEffectScale` at the two magnitude sites |
| `src/SelfTest.cpp` | Two in-game checks for the migration and the snapshot |
| `tools/check.mjs` | Two new static checks |
| `mcm-patch/MCM/Config/IsekaiHero/config.json` | The Progression section on the *The System* page |
| `mcm-patch/MCM/Config/IsekaiHero/settings.ini` | MCM defaults, which must match the shipped ini |
| `CHANGELOG.md`, `docs/MANUAL_TESTS.md` | The developer record and the in-game test table |

No new files. Everything lands in a file that already owns that responsibility.

---

### Task 1: The eight settings

**Files:**
- Modify: `IsekaiHero.ini` (append a `[Progression]` block)
- Modify: `src/Config.h`
- Modify: `src/Config.cpp:40-60` (globals), `:290-310` (parser), `:380-395` (reset), `:420-440` (log), `:570-585` (accessors)
- Test: `tools/check.mjs` — the existing check `config: the ini and Config::Load name the same settings`

**Interfaces:**
- Consumes: nothing.
- Produces: `Config::NodeCostScale() -> float`, `Config::NodeEffectScale() -> float`,
  `Config::HeroSkillLevel() -> std::uint32_t`, `Config::HeroAttributeTarget() -> std::uint32_t`,
  `Config::HeroSystemPoints() -> std::int32_t`, `Config::AscendedSkillLevel() -> std::uint32_t`,
  `Config::AscendedAttributeTarget() -> std::uint32_t`,
  `Config::AscendedSystemPoints() -> std::int32_t`.

- [ ] **Step 1: Write the failing test — document the settings in the ini**

Append to `IsekaiHero.ini`:

```ini
[Progression]

; How much a skill-tree node costs, as a multiplier on every price in the tree.
; 2.0 makes the whole tree twice as expensive, 0.5 half.
;
; This one is read ONCE, when your character is reincarnated, and then fixed for that
; character. A respec gives back exactly what was paid, and it could not keep that promise
; if the price moved underneath a purchase. Change it and REBOOT to apply it to an
; existing character.
NodeCostScale = 1.0

; How large a node's bonus is, as a multiplier on every magnitude in the tree.
; Unlike the price, this applies immediately and to existing characters: node effects are
; recomputed from scratch on every load, so nothing stacks or drifts.
NodeEffectScale = 1.0

; What each blessing hands over at the start.
;
; AttributeTarget raises Health, Magicka and Stamina TO this number - it never lowers them
; and never pays twice, so a DORMANT character who awakens later only receives the
; difference they are still missing. A fresh character starts at 100 in all three.
;
; The System does NOT set your character level. That is deliberate: Skyrim's enemies scale
; with your level, so granting one dragged the whole world up with it. Your skills,
; attributes and perks make you stronger; the world stays where it is.
HeroSkillLevel = 50
HeroAttributeTarget = 180
HeroSystemPoints = 5

AscendedSkillLevel = 100
AscendedAttributeTarget = 600
AscendedSystemPoints = 10
```

- [ ] **Step 2: Run the check to verify it fails**

Run: `node tools/check.mjs`
Expected: FAIL on `config: the ini and Config::Load name the same settings`, with
`the ini documents nodecostscale, nodeeffectscale, heroskilllevel, heroattributetarget, herosystempoints, ascendedskilllevel, ascendedattributetarget, ascendedsystempoints, which Config::Load never reads — setting it does nothing at all`

- [ ] **Step 3: Add the globals**

In `src/Config.cpp`, in the anonymous namespace beside `g_professionActionsPerPoint`:

```cpp
        // Read once at reincarnation and then fixed for that character — see
        // State::nodeCostScale. Live here only so a new character picks up the ini.
        float         g_nodeCostScale = 1.0f;
        // Live: node effects are re-derived on every load, so changing this mid-save
        // recomputes cleanly instead of stacking.
        float         g_nodeEffectScale = 1.0f;
        std::uint32_t g_heroSkillLevel = 50;
        std::uint32_t g_heroAttributeTarget = 180;
        std::int32_t  g_heroSystemPoints = 5;
        std::uint32_t g_ascendedSkillLevel = 100;
        std::uint32_t g_ascendedAttributeTarget = 600;
        std::int32_t  g_ascendedSystemPoints = 10;
```

- [ ] **Step 4: Add the parser arms**

In `src/Config.cpp`, in the `else if` chain beside `questrewardscale`:

```cpp
            } else if (key == "nodecostscale") {
                // Clamped, not trusted: 0 would make the whole tree free, which reads as
                // the mod being broken rather than as a setting.
                try {
                    g_nodeCostScale = std::clamp(std::stof(val), 0.1f, 10.0f);
                } catch (...) {
                }
            } else if (key == "nodeeffectscale") {
                try {
                    g_nodeEffectScale = std::clamp(std::stof(val), 0.1f, 10.0f);
                } catch (...) {
                }
            } else if (key == "heroskilllevel") {
                g_heroSkillLevel = AsCount(val, g_heroSkillLevel, 0u, 100u);
            } else if (key == "heroattributetarget") {
                g_heroAttributeTarget = AsCount(val, g_heroAttributeTarget, 0u, 10000u);
            } else if (key == "herosystempoints") {
                g_heroSystemPoints =
                    static_cast<std::int32_t>(AsCount(val, static_cast<std::uint32_t>(g_heroSystemPoints), 0u, 100000u));
            } else if (key == "ascendedskilllevel") {
                g_ascendedSkillLevel = AsCount(val, g_ascendedSkillLevel, 0u, 100u);
            } else if (key == "ascendedattributetarget") {
                g_ascendedAttributeTarget = AsCount(val, g_ascendedAttributeTarget, 0u, 10000u);
            } else if (key == "ascendedsystempoints") {
                g_ascendedSystemPoints =
                    static_cast<std::int32_t>(AsCount(val, static_cast<std::uint32_t>(g_ascendedSystemPoints), 0u, 100000u));
```

- [ ] **Step 5: Add the defaults reset**

In the reset block beside `g_professionActionsPerPoint = 25;`:

```cpp
        g_nodeCostScale = 1.0f;
        g_nodeEffectScale = 1.0f;
        g_heroSkillLevel = 50;
        g_heroAttributeTarget = 180;
        g_heroSystemPoints = 5;
        g_ascendedSkillLevel = 100;
        g_ascendedAttributeTarget = 600;
        g_ascendedSystemPoints = 10;
```

- [ ] **Step 6: Add the accessors**

In `src/Config.h`, beside `ProfessionActionsPerPoint()`:

```cpp
    // Skill-tree price scale. Read once at reincarnation and snapshotted into the
    // co-save — see State::nodeCostScale for why it must not be live.
    [[nodiscard]] float         NodeCostScale();
    // Skill-tree magnitude scale. Live: effects are re-derived on every load.
    [[nodiscard]] float         NodeEffectScale();
    [[nodiscard]] std::uint32_t HeroSkillLevel();
    [[nodiscard]] std::uint32_t HeroAttributeTarget();
    [[nodiscard]] std::int32_t  HeroSystemPoints();
    [[nodiscard]] std::uint32_t AscendedSkillLevel();
    [[nodiscard]] std::uint32_t AscendedAttributeTarget();
    [[nodiscard]] std::int32_t  AscendedSystemPoints();
```

And in `src/Config.cpp`, beside `QuestRewardScale()`:

```cpp
    float NodeCostScale() {
        return g_nodeCostScale;
    }

    float NodeEffectScale() {
        return g_nodeEffectScale;
    }

    std::uint32_t HeroSkillLevel() {
        return g_heroSkillLevel;
    }

    std::uint32_t HeroAttributeTarget() {
        return g_heroAttributeTarget;
    }

    std::int32_t HeroSystemPoints() {
        return g_heroSystemPoints;
    }

    std::uint32_t AscendedSkillLevel() {
        return g_ascendedSkillLevel;
    }

    std::uint32_t AscendedAttributeTarget() {
        return g_ascendedAttributeTarget;
    }

    std::int32_t AscendedSystemPoints() {
        return g_ascendedSystemPoints;
    }
```

- [ ] **Step 7: Add them to the startup log line**

The single `logger::info("Config: ...")` call in `Config::Load` is what a bug report shows.
Append to its format string, before the closing quote:

```
, NodeCostScale={}, NodeEffectScale={}, Hero={}/{}/{}, Ascended={}/{}/{}
```

and to its argument list, in the same order:

```cpp
                     g_nodeCostScale, g_nodeEffectScale, g_heroSkillLevel,
                     g_heroAttributeTarget, g_heroSystemPoints, g_ascendedSkillLevel,
                     g_ascendedAttributeTarget, g_ascendedSystemPoints,
```

- [ ] **Step 8: Run the check to verify it passes**

Run: `node tools/check.mjs`
Expected: PASS, `config: the ini and Config::Load name the same settings` reporting eight
more settings than before.

- [ ] **Step 9: Build**

Run: `build.bat`
Expected: `BUILD_OK`.

- [ ] **Step 10: Commit**

```bash
git grep -n -i "dustin\|reymendt"   # must print nothing
git add IsekaiHero.ini src/Config.h src/Config.cpp
git commit -m "feat(config): the progression curve becomes eight settings"
```

---

### Task 2: The blessing grants a body, not a level

**Files:**
- Modify: `src/System.cpp` — `Blessing` struct (~line 68), `BlessingFor` (~78),
  `AttributeBonusPerStat` (~97, deleted), `SetPlayerLevelAtLeast` (~128, deleted),
  `RestoreLevelBeforeGrant` (~162, deleted), `ApplyBlessing` (~210),
  `RevertCallback` call site (~1048), `ReapplyLevelOnLoad` (~1059, deleted), its call site (~1215)
- Test: `tools/check.mjs` — a new check

**Interfaces:**
- Consumes: `Config::HeroSkillLevel()`, `Config::HeroAttributeTarget()`,
  `Config::HeroSystemPoints()`, `Config::AscendedSkillLevel()`,
  `Config::AscendedAttributeTarget()`, `Config::AscendedSystemPoints()` from Task 1.
- Produces: `Blessing { std::uint16_t skillLevel; std::uint16_t attrTarget; std::int32_t perkPoints; std::int32_t gold; std::int32_t dragonSouls; std::int32_t systemPoints; }`.
  `ApplyBlessing(const Blessing&) -> float` now returns the largest single raise it made to
  Health/Magicka/Stamina, for the log line. `MilestonePerkPoints()` still reads
  `BlessingFor(g_state.power).perkPoints` and is unchanged.

- [ ] **Step 1: Write the failing test**

Add to `tools/check.mjs`, after the `co-save: every state field written is also read` check:

```js
check("system: the blessing grants no character level", () => {
  // Skyrim's levelled lists key off the player's character level, so granting one pulled
  // every scaling enemy to its ceiling in a single step. The grant is gone, and so is the
  // machinery that held the number up across loads — actorData.level does not persist for
  // the player, which is the only reason that machinery existed.
  const sys = read("src/System.cpp");
  const gone = ["SetPlayerLevelAtLeast", "ReapplyLevelOnLoad", "RestoreLevelBeforeGrant",
                "g_levelBeforeGrant", "AttributeBonusPerStat"];
  const left = gone.filter((n) => sys.includes(n));
  need(left.length === 0,
       `System.cpp still carries ${left.join(", ")} — the level grant is only half removed, ` +
       `and a half-removed grant still moves the world`);
  need(!/std::uint16_t\s+playerLevel;/.test(sys),
       "Blessing still has a playerLevel field");
  need(/std::uint16_t\s+attrTarget;/.test(sys),
       "Blessing has no attrTarget field — attributes must be a target, not a level derivation");
  return "no level grant";
});
```

- [ ] **Step 2: Run the check to verify it fails**

Run: `node tools/check.mjs`
Expected: FAIL on `system: the blessing grants no character level`, listing all five names.

- [ ] **Step 3: Replace the Blessing field and the table**

In `src/System.cpp`, change the struct:

```cpp
        // What each blessing grants. 0 = leave that stat untouched.
        struct Blessing {
            std::uint16_t skillLevel;    // set all 18 skills to at least this
            std::uint16_t attrTarget;    // raise Health/Magicka/Stamina to at least this
            std::int32_t  perkPoints;    // add to available perk points
            std::int32_t  gold;          // add to inventory
            std::int32_t  dragonSouls;   // add to the unspent soul pool
            std::int32_t  systemPoints;  // seed for the skill tree
        };
```

and the table, which now reads the ini:

```cpp
        // No character level in here any more, and that is the whole of #29. Skyrim's
        // levelled lists key off the player's level, so setting it to 150 pulled every
        // scaling enemy to its ceiling at once — "insanely hard before quickly catching
        // up, then trivialized" is one fact seen from both ends, not two complaints.
        //
        // Attributes used to be derived from that level ((150-1)*10/3 = 497 per stat).
        // They are now their own target, and both halves are settings: the curve is the
        // thing players asked to be able to tune.
        Blessing BlessingFor(PowerLevel a_power) {
            switch (a_power) {
            case PowerLevel::Hero:
                return { static_cast<std::uint16_t>(Config::HeroSkillLevel()),
                         static_cast<std::uint16_t>(Config::HeroAttributeTarget()),
                         10, 2000, 3, Config::HeroSystemPoints() };
            case PowerLevel::Ascended:
                return { static_cast<std::uint16_t>(Config::AscendedSkillLevel()),
                         static_cast<std::uint16_t>(Config::AscendedAttributeTarget()),
                         kMaxPerkPoints, 25000, 20, Config::AscendedSystemPoints() };
            default:  // Normal — pure challenge, no boosts
                return { 0, 0, 0, 0, 0, 0 };
            }
        }
```

- [ ] **Step 4: Delete `AttributeBonusPerStat` and rewrite `ApplyBlessing`**

Delete the whole `AttributeBonusPerStat` function and its comment block. Replace the
attribute and level part of `ApplyBlessing` so the function reads:

```cpp
        float ApplyBlessing(const Blessing& b) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return 0.0f;
            }
            // Before a single value is written.
            if (b.skillLevel > 0 || b.attrTarget > 0) {
                CaptureBaseline();
            }
            auto* avOwner = player->AsActorValueOwner();

            // Skills: the 18 skill actor values are contiguous (kOneHanded..kEnchanting).
            if (b.skillLevel > 0 && avOwner) {
                for (int av = static_cast<int>(RE::ActorValue::kOneHanded);
                     av <= static_cast<int>(RE::ActorValue::kEnchanting); ++av) {
                    const auto avEnum = static_cast<RE::ActorValue>(av);
                    if (avOwner->GetBaseActorValue(avEnum) < static_cast<float>(b.skillLevel)) {
                        avOwner->SetBaseActorValue(avEnum, static_cast<float>(b.skillLevel));
                    }
                }
            }

            // Health/Magicka/Stamina are raised TO the target, never past it and never
            // lowered — the same "only if below" rule the skills above use.
            //
            // A target rather than a flat bonus, because the formula this replaces paid
            // out only the DIFFERENCE between the character's level and the granted one.
            // A DORMANT character awakening to ASCENDED at character level 80 received
            // 233 per stat, not 497, and must keep receiving only what they are missing.
            // A flat field would hand them the whole amount a second time.
            float granted = 0.0f;
            if (b.attrTarget > 0 && avOwner) {
                const float target = static_cast<float>(b.attrTarget);
                for (auto av : { RE::ActorValue::kHealth, RE::ActorValue::kMagicka,
                                 RE::ActorValue::kStamina }) {
                    const float have = avOwner->GetBaseActorValue(av);
                    if (have < target) {
                        avOwner->SetBaseActorValue(av, target);
                        granted = std::max(granted, target - have);
                    }
                }
            }

            GrantPerkPoints(b.perkPoints);
            GrantDragonSouls(b.dragonSouls);
            GrantSystemPoints(b.systemPoints);

            // Gold (Gold001 = 0x0000000F).
            if (b.gold > 0) {
                if (auto* gold = RE::TESForm::LookupByID<RE::TESObjectMISC>(0x0000000F)) {
                    player->AddObjectToContainer(gold, nullptr, b.gold, nullptr);
                }
            }

            return granted;
        }
```

- [ ] **Step 5: Delete the level machinery**

Delete these four things entirely, comments included:
- `std::uint16_t g_levelBeforeGrant = 0;` and its comment
- `void SetPlayerLevelAtLeast(std::uint16_t a_target)`
- `void RestoreLevelBeforeGrant()`
- `void ReapplyLevelOnLoad()`

Then remove their two call sites. In `RevertCallback`, delete the `RestoreLevelBeforeGrant();`
line and replace the comment above it, so the function starts:

```cpp
        void RevertCallback(SKSE::SerializationInterface*) {
            g_state = State{};
            g_firstReadyCell = 0;  // the next game gets a fresh chargen-room detection
            logger::info("State reverted to defaults (new game / pre-load)");
        }
```

In the post-load sequence (~line 1215), delete these three lines:

```cpp
                // The character level doesn't persist for the player either — re-assert
                // the blessing's level so it stops dropping to 1 on load.
                ReapplyLevelOnLoad();
```

Task 3 puts `RetireLevelGrant()` in that slot. Leave it empty for now.

`RestoreBaseline` keeps its `preLevel` restore untouched — it is a no-op for a character
granted no level, and it is what corrects the ones that were.

- [ ] **Step 6: Update the reincarnation log line**

In `ApplyReincarnation`, the log line still says `level={}`. Change it to name what is
actually granted:

```cpp
            logger::info(
                "Reincarnation applied: power={} skills={} attrTarget={} perks=+{} attr=+{} "
                "gold={} souls=+{}",
                PowerName(g_state.power), b.skillLevel, b.attrTarget, b.perkPoints, attrBonus,
                b.gold, b.dragonSouls);
```

- [ ] **Step 7: Run the check to verify it passes**

Run: `node tools/check.mjs`
Expected: PASS, `system: the blessing grants no character level` reporting `no level grant`.

- [ ] **Step 8: Build**

Run: `build.bat`
Expected: `BUILD_OK`. If the compiler reports an unused `#include <cmath>` warning in
System.cpp, leave the include — other call sites use it.

- [ ] **Step 9: Commit**

```bash
git grep -n -i "dustin\|reymendt"   # must print nothing
git add src/System.cpp tools/check.mjs
git commit -m "feat(system): the blessing grants a body, not a character level"
```

---

### Task 3: Co-save v16 — the price snapshot and the one-shot level migration

**Files:**
- Modify: `src/System.h` — `State`, beside `questsGiven`
- Modify: `src/System.cpp:49` (`kVersion`), `SaveCallback` tail (~line 830), `LoadCallback`
  tail (~line 1000), `ApplyReincarnation` (~line 265), the post-load sequence (~line 1213)
- Test: `tools/check.mjs` — the existing checks `co-save: kVersion matches the highest version gate`
  and `co-save: every state field written is also read`

**Interfaces:**
- Consumes: `Config::NodeCostScale()` from Task 1; the emptied post-load slot from Task 2.
- Produces: `State::nodeCostScale` (float, default 1.0f) and `State::levelGrantRetired`
  (bool, default false), both reachable through the existing `Isekai::GetState()`.
  `RetireLevelGrant()` — file-local, no parameters, no return.

- [ ] **Step 1: Write the failing test — add the state fields**

In `src/System.h`, in `State`, after `questsGiven`:

```cpp
        // v16: the skill-tree price scale this character was created under.
        //
        // Config::NodeCostScale() may be changed in the ini at any time, but
        // SkillTree::RespecRefund gives back what RankCost charged, and MasteryCostToRank
        // sums every individual tier purchase so that a respec returns exactly what was
        // paid. A live scale would break that promise — buy at 1.0, refund at 2.0. So it
        // is read once, here, the way questReward is snapshotted when an objective is
        // handed out. REBOOT is how an existing character gets a new one.
        float nodeCostScale = 1.0f;

        // v16: the one-shot level migration has run.
        //
        // Before this build the blessing wrote actorData.level and re-asserted it after
        // every load, because that field does not persist for the player. The grant is
        // gone, so the number has to come down exactly once. A save with no v14 baseline
        // has nothing to come down to and is marked done without being touched.
        bool levelGrantRetired = false;
```

- [ ] **Step 2: Run the check to verify it fails**

Run: `node tools/check.mjs`
Expected: PASS still — the state fields alone break nothing. This step exists so the next
one's failure is unambiguous. Now bump `kVersion`:

In `src/System.cpp:49`, change `constexpr std::uint32_t kVersion = 15;` to `= 16;` and run
`node tools/check.mjs` again.
Expected: FAIL on `co-save: kVersion matches the highest version gate` with
`kVersion is 16 but the highest read gate is 15 — a field was added without bumping, or bumped without being read`

- [ ] **Step 3: Write the fields**

In `SaveCallback`, after the v15 `professionActions` write:

```cpp
            // v16: the price scale this character bought at, and whether the old level
            // grant has already been undone for them.
            a_intf->WriteRecordData(g_state.nodeCostScale);
            a_intf->WriteRecordData(g_state.levelGrantRetired);
```

- [ ] **Step 4: Read the fields**

In `LoadCallback`, after the v15 read block:

```cpp
                // v16: an older save bought its nodes at 1.0 — that is what it paid, and
                // that is what a respec has to give back. A character from before this
                // build still holds a granted level, so the migration has not run.
                g_state.nodeCostScale = 1.0f;
                g_state.levelGrantRetired = false;
                if (version >= 16) {
                    a_intf->ReadRecordData(g_state.nodeCostScale);
                    a_intf->ReadRecordData(g_state.levelGrantRetired);
                    if (!(g_state.nodeCostScale > 0.0f)) {
                        // A corrupt or zeroed scale would make the tree free. Read out of
                        // a file, so bounded before it is used.
                        logger::error("Co-save: node price scale read as {} — using 1.0",
                                      g_state.nodeCostScale);
                        g_state.nodeCostScale = 1.0f;
                    }
                }
```

- [ ] **Step 5: Snapshot the scale at reincarnation**

In `ApplyReincarnation`, immediately before `const Blessing b = BlessingFor(g_state.grantTier);`:

```cpp
            // Fix the tree's prices for this character before a single point can be spent.
            // See State::nodeCostScale.
            g_state.nodeCostScale = Config::NodeCostScale();
```

- [ ] **Step 6: Add the migration**

In `src/System.cpp`, in the same anonymous namespace the deleted `ReapplyLevelOnLoad` lived in:

```cpp
        // Put the character back on the level they had before their blessing — once.
        //
        // Until this build the blessing wrote actorData.level and re-asserted it after
        // every load. The grant is gone, and rather than wager on what the engine leaves
        // behind when nothing re-asserts it, the answer is stated here: the v14 baseline
        // is what this character was, so that is what they go back to.
        //
        // A save with no baseline (from before v14, or a SHATTERED start, which captures
        // none) has nothing to go back to. It is marked done and left alone; REBOOT
        // already tells such a character it cannot hand the body back.
        void RetireLevelGrant() {
            if (!g_state.reincarnated || g_state.levelGrantRetired) {
                return;
            }
            if (g_state.preLevel == 0) {
                g_state.levelGrantRetired = true;
                logger::info("Level grant retired: no baseline in this save — level left as it is");
                return;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* base = player ? player->GetActorBase() : nullptr;
            if (!base) {
                // Not marked done: try again on the next load rather than losing the
                // migration to a moment the player form was not ready.
                return;
            }
            const auto before = base->actorData.level;
            base->actorData.level = g_state.preLevel;
            g_state.levelGrantRetired = true;
            logger::info("Level grant retired: level {} -> {} (one-shot migration)", before,
                         g_state.preLevel);
        }
```

And call it in the post-load sequence, in the slot Task 2 emptied, between `Passives::Refresh();`
and `Storage::PruneForeignStock();`:

```cpp
                // One-shot: undo the character level earlier builds granted and kept
                // re-asserting. After this the mod never touches the level again.
                RetireLevelGrant();
```

- [ ] **Step 7: Run the checks to verify they pass**

Run: `node tools/check.mjs`
Expected: PASS. `co-save: kVersion matches the highest version gate` reports `v16`, and
`co-save: every state field written is also read` reports two more fields than before.

- [ ] **Step 8: Build**

Run: `build.bat`
Expected: `BUILD_OK`.

- [ ] **Step 9: Commit**

```bash
git grep -n -i "dustin\|reymendt"   # must print nothing
git add src/System.h src/System.cpp
git commit -m "feat(system): co-save v16 — the price snapshot and the level migration"
```

---

### Task 4: The node multipliers

**Files:**
- Modify: `src/SkillTree.cpp:246-270` (add `NodeCost`, route `RankCost` and
  `RepeatableCostToRank`), `:516` (`kDirectStat` magnitude), `:725` (attribute totals),
  `:795`, `:808`, `:834` (the three raw `node.cost` reads)
- Test: `tools/check.mjs` — a new check

**Interfaces:**
- Consumes: `Config::NodeEffectScale()` from Task 1; `State::nodeCostScale` from Task 3,
  through the existing `Isekai::GetState()`.
- Produces: `NodeCost(const Node&) -> std::int32_t`, file-local to `SkillTree.cpp`.

- [ ] **Step 1: Write the failing test**

Add to `tools/check.mjs`, after the check added in Task 2:

```js
check("skill tree: every node price goes through NodeCost", () => {
  // RespecRefund gives back what RankCost charged. If one of them reads Node::cost raw
  // while the other applies the character's price scale, a respec refunds at a different
  // rate than was paid — which is exactly the promise MasteryCostToRank exists to keep.
  const src = read("src/SkillTree.cpp");
  const helper = src.match(/std::int32_t NodeCost\(const Node& a_node\)[\s\S]*?\n        \}/);
  need(helper, "NodeCost() not found in SkillTree.cpp");
  const raw = [...src.replace(helper[0], "").matchAll(/\b[A-Za-z_]\w*\.cost\b/g)]
    .map((m) => m[0]);
  need(raw.length === 0,
       `these read a node's price directly instead of through NodeCost(): ${raw.join(", ")}`);
  return "all prices scaled";
});
```

- [ ] **Step 2: Run the check to verify it fails**

Run: `node tools/check.mjs`
Expected: FAIL on `skill tree: every node price goes through NodeCost` with
`NodeCost() not found in SkillTree.cpp`.

- [ ] **Step 3: Add the helper and route the cost sites**

In `src/SkillTree.cpp`, immediately above `RankCost`:

```cpp
        // Every node price in the tree goes through here.
        //
        // The scale comes from the co-save, not from Config: it was fixed when this
        // character was reincarnated, because RespecRefund reads a node's price as it
        // stands at the moment of the refund. A live scale would refund at a rate the
        // purchase was never made at. See State::nodeCostScale.
        //
        // Never free: a scale the player set very low must still leave a price, or the
        // tree stops being a choice.
        [[nodiscard]] std::int32_t NodeCost(const Node& a_node) {
            const float scale = GetState().nodeCostScale;
            const float scaled = static_cast<float>(a_node.cost) * (scale > 0.0f ? scale : 1.0f);
            return std::max(1, static_cast<std::int32_t>(std::lround(scaled)));
        }
```

Then replace all six raw reads:

| Line | Was | Becomes |
|---|---|---|
| 250 | `? a_node.cost * TierOfRank(a_rank) : a_node.cost` | `? NodeCost(a_node) * TierOfRank(a_rank) : NodeCost(a_node)` |
| 269 | `: a_node.cost * a_rank` | `: NodeCost(a_node) * a_rank` |
| 795 | `? node.cost : 0` | `? NodeCost(node) : 0` |
| 808 | `? node.cost : 0` | `? NodeCost(node) : 0` |
| 834 | `refund += node.cost;` | `refund += NodeCost(node);` |

(Line 250 has two occurrences on the one line — both change.)

- [ ] **Step 4: Scale the two magnitude sites**

At line 516, in the `kDirectStat` branch — the baseline is where the actor value sits with
no ranks bought, so only the per-rank step scales:

```cpp
                        avOwner->SetBaseActorValue(
                            b.av, a_node.baseline + b.amount * Config::NodeEffectScale() * rank);
```

At line 725, in the attribute totals fed to `Passives::Refresh`:

```cpp
                    a_totals[bonus.av] +=
                        bonus.amount * Config::NodeEffectScale() * static_cast<float>(times);
```

`src/SkillTree.cpp` does **not** include `Config.h` today — add it to the include block,
alphabetically between `"../include/IsekaiHeroAPI.h"`-style project headers already there:

```cpp
#include "Config.h"
```

No new standard header is needed for `std::lround`: the same file already calls
`std::round` at line 777 without including `<cmath>`, because CommonLibSSE-NG's
force-included `SKSE/Impl/PCH.h` brings it in.

- [ ] **Step 5: Run the check to verify it passes**

Run: `node tools/check.mjs`
Expected: PASS, `skill tree: every node price goes through NodeCost` reporting
`all prices scaled`. The existing `skill tree:` checks must all still pass — they read the
node table, not the prices.

- [ ] **Step 6: Build**

Run: `build.bat`
Expected: `BUILD_OK`.

- [ ] **Step 7: Commit**

```bash
git grep -n -i "dustin\|reymendt"   # must print nothing
git add src/SkillTree.cpp tools/check.mjs
git commit -m "feat(tree): node prices and magnitudes become multipliers"
```

---

### Task 5: The MCM surface, the self-test and the docs

**Files:**
- Modify: `mcm-patch/MCM/Config/IsekaiHero/config.json` — the *The System* page
- Modify: `mcm-patch/MCM/Config/IsekaiHero/settings.ini` — the `[Progression]` section
- Modify: `src/SelfTest.cpp` (~line 107, after the storage ownership check)
- Modify: `CHANGELOG.md` (`## [Unreleased]` → `### Changed`)
- Modify: `docs/MANUAL_TESTS.md`

**Interfaces:**
- Consumes: everything from Tasks 1–4. `GetState().nodeCostScale`,
  `GetState().levelGrantRetired`, `GetState().reincarnated`.
- Produces: nothing further tasks depend on.

`src/SelfTest.cpp` already includes `System.h` and calls `GetState()` (lines 250, 362, 465),
and `std::format` is available without a new include — `Config.cpp` uses it the same way,
because CommonLibSSE-NG's force-included `SKSE/Impl/PCH.h` brings `<format>` with it.

- [ ] **Step 1: Add the MCM defaults**

In `mcm-patch/MCM/Config/IsekaiHero/settings.ini`, in the existing `[Progression]` section,
after `iProfessionActionsPerPoint=25`:

```ini
fNodeCostScale=1.0
fNodeEffectScale=1.0
iHeroSkillLevel=50
iHeroAttributeTarget=180
iHeroSystemPoints=5
iAscendedSkillLevel=100
iAscendedAttributeTarget=600
iAscendedSystemPoints=10
```

The `b`/`i`/`f` prefixes are required — MCM Helper stores these as engine settings and the
engine takes a setting's type from its first letter. `Config::ReadIni` strips the prefix
when reading the MCM file, which is why the key names match the shipped ini.

- [ ] **Step 2: Add the MCM controls**

In `mcm-patch/MCM/Config/IsekaiHero/config.json`, on the page whose `pageDisplayName` is
`The System`, append to its `content` array after the `bSkyrimNetIntegration:SkyrimNet`
entry:

```json
        { "text": "The power curve", "type": "header" },
        {
          "text": "The System does not set your character level. Skyrim's enemies scale with it, so granting one dragged the whole world up too. These change what a blessing hands over instead.",
          "type": "text"
        },
        {
          "id": "fNodeCostScale:Progression",
          "text": "Node price",
          "help": "Multiplies every skill-tree price. Fixed when your character is reincarnated, because a respec gives back exactly what was paid — REBOOT to apply a new value to this character.",
          "type": "slider",
          "valueOptions": { "min": 0.1, "max": 10.0, "step": 0.1, "formatString": "{1}", "sourceType": "ModSettingFloat", "defaultValue": 1.0 }
        },
        {
          "id": "fNodeEffectScale:Progression",
          "text": "Node strength",
          "help": "Multiplies every skill-tree bonus. Applies immediately, to this character too — node effects are recomputed from scratch on every load.",
          "type": "slider",
          "valueOptions": { "min": 0.1, "max": 10.0, "step": 0.1, "formatString": "{1}", "sourceType": "ModSettingFloat", "defaultValue": 1.0 }
        },
        {
          "id": "iHeroAttributeTarget:Progression",
          "text": "HERO body",
          "help": "Raises Health, Magicka and Stamina to this number. It never lowers them and never pays twice. A fresh character starts at 100 in all three.",
          "type": "slider",
          "valueOptions": { "min": 100, "max": 2000, "step": 10, "formatString": "{0}", "sourceType": "ModSettingInt", "defaultValue": 180 }
        },
        {
          "id": "iAscendedAttributeTarget:Progression",
          "text": "ASCENDED body",
          "help": "The same, for ASCENDED. 600 is what the old level-150 grant worked out to.",
          "type": "slider",
          "valueOptions": { "min": 100, "max": 2000, "step": 10, "formatString": "{0}", "sourceType": "ModSettingInt", "defaultValue": 600 }
        },
        {
          "id": "iHeroSkillLevel:Progression",
          "text": "HERO skills",
          "help": "Every skill is raised to at least this. Skills already higher are left alone.",
          "type": "slider",
          "valueOptions": { "min": 0, "max": 100, "step": 5, "formatString": "{0}", "sourceType": "ModSettingInt", "defaultValue": 50 }
        },
        {
          "id": "iAscendedSkillLevel:Progression",
          "text": "ASCENDED skills",
          "help": "At 100 no skill can rise any further, so an ASCENDED character stops gaining levels. That is deliberate; lower this if you want room to grow.",
          "type": "slider",
          "valueOptions": { "min": 0, "max": 100, "step": 5, "formatString": "{0}", "sourceType": "ModSettingInt", "defaultValue": 100 }
        },
        {
          "id": "iHeroSystemPoints:Progression",
          "text": "HERO starting points",
          "help": "System Points handed over at the start. A tier's real advantage is its reward scale, not this seed.",
          "type": "slider",
          "valueOptions": { "min": 0, "max": 500, "step": 5, "formatString": "{0}", "sourceType": "ModSettingInt", "defaultValue": 5 }
        },
        {
          "id": "iAscendedSystemPoints:Progression",
          "text": "ASCENDED starting points",
          "help": "The same, for ASCENDED. The whole tree costs 260, so a large number here skips the tree rather than opening it.",
          "type": "slider",
          "valueOptions": { "min": 0, "max": 500, "step": 5, "formatString": "{0}", "sourceType": "ModSettingInt", "defaultValue": 10 }
        }
```

- [ ] **Step 3: Add the self-test checks**

In `src/SelfTest.cpp`, after the `storage chest ownership` block:

```cpp
        // The two things about this change a player cannot see from inside the game: did
        // the one-shot level migration run, and what price is this character's tree at.
        if (const auto& st = GetState(); st.reincarnated) {
            Add(out, st.levelGrantRetired, true, "level grant retired",
                st.levelGrantRetired
                    ? "the System no longer sets the character level"
                    : "pending — it runs on the next load, or this save has no baseline");
            Add(out, st.nodeCostScale > 0.0f, true, "node price scale",
                std::format("x{:.2f}, fixed at reincarnation", st.nodeCostScale));
        }
```

- [ ] **Step 4: Run the checks and build**

Run: `node tools/check.mjs && build.bat`
Expected: all checks pass, `BUILD_OK`. The check
`config: the ini and Config::Load name the same settings` covers the shipped ini only — the
MCM files are validated by the FOMOD check, which must also still pass.

- [ ] **Step 5: Write the changelog entry**

In `CHANGELOG.md`, under `## [Unreleased]` → `### Changed`, as the first entry:

```markdown
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
```

- [ ] **Step 6: Write the manual test table**

In `docs/MANUAL_TESTS.md`, after section 1.2b (the storage ownership section), add:

```markdown
### 1.2c The System no longer moves the world (#29)

The blessing used to set the character level, which pulled every scaling enemy to its
ceiling. Test on the modlist, from the packaged archive — `build.bat` does not reach it.

| Step | Expected |
|---|---|
| Reincarnate a fresh character as ASCENDED, read the log. | `attrTarget=600`, and `attr=+500` on a character still at the 100 baseline. No `level=` in the line at all. |
| `player.getlevel` in the console straight after. | The character's own level, unchanged by the blessing. |
| Fight a scaling enemy near the start. | It is at its own level, not at a ceiling. |
| Save, reload, `player.getlevel` again. | The same number. Nothing re-asserts it, nothing reverts it. |
| Load an **existing ASCENDED save** made before this build. | The log says `Level grant retired: level 150 -> N (one-shot migration)`. Attributes, skills, perks and points unchanged. |
| Load that same save a second time. | **No second migration line.** The flag held. |
| In the same session, load a save the System has never touched. | Level untouched. This is the reported ActorBase bug, and there is no grant left to leak. |
| A DORMANT character crossing level 80. | Awakens to ASCENDED; the world does not jump. |
| Set `NodeEffectScale = 2.0`, reload an existing character. | Node bonuses double. Reload again — they do not double a second time. |
| Set `NodeCostScale = 2.0`, load an existing character. | Prices unchanged: the snapshot holds. Self-test reports `node price scale x1.00`. |
| REBOOT that character and choose ASCENDED again. | Prices now doubled, self-test reports `x2.00`, and a respec refunds exactly what was paid. |
| Run the **self-test**. | `level grant retired — the System no longer sets the character level`. |
```

- [ ] **Step 7: Package and verify the archive**

Run: `package.ps1`
Expected: `Privacy gate: ... all clean` and `PACKAGE_OK`. Confirm
`MCM\Config\IsekaiHero\config.json` and `settings.ini` in the archive carry the new entries.

- [ ] **Step 8: Commit**

```bash
git grep -n -i "dustin\|reymendt"   # must print nothing
git add mcm-patch src/SelfTest.cpp CHANGELOG.md docs/MANUAL_TESTS.md
git commit -m "feat(mcm,docs): the power curve becomes visible and testable"
```

---

## What is deliberately not here

- **The nine orthogonal effects from #29's body** — magicka cost reduction, poison refunds,
  stagger on power attacks, the infinite quiver. Each needs its own hook into combat,
  casting or crafting. They are a content pass on a curve that works, and the curve has to
  work first. #16 (damage nodes) belongs with them.
- **Gold, dragon souls and perk points as settings.** Flavour, not curve. They can be
  exposed when someone asks.
- **Per-node ini keys.** 22 nodes × 4 values is 88 chances to be wrong. Two multipliers
  cover what #17 and #32 actually asked for.
- **A Nexus changelog line.** `docs/NEXUS_CHANGELOG.md` has no Unreleased section; player
  lines are written when the version is cut, which is not this plan's job.
