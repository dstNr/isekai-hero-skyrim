# System Enchantments Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eight skill-tree nodes that each teach one strong System enchantment at the vanilla enchanting table, each with its own look, four of them backed by plugin logic.

**Architecture:** The records (ENCH, MGEF, EFSH, ARTO, one hidden SPEL) go into `plugin/IsekaiHero.esp`, written the way Part L was (from the format, verified by read-back), because CK 1.7.99 crashes. A new module `src/Enchantments.{h,cpp}` owns the eight forms: teaching them, keeping Arcane Omniscience away from them, and the plugin effects (Stormchain, System Breach, Phoenix Heart). Every decision that can be made without the engine lives in `src/EnchantLogic.h` and is unit-tested.

**Tech Stack:** C++23, CommonLibSSE-NG, SKSE co-save, Magelight page `tree.js`, Node tools (`check.mjs`, `extract-strings.mjs`), houseCARL record tools for the ESP.

**Spec:** `docs/superpowers/specs/2026-11-20-system-enchantments-design.md`

## Global Constraints

- One DLL for SE, AE and VR, runtime 1.6.1170–1.7.104. VR untested is acceptable.
- `IsekaiHero.esp` stays a light plugin: every new local FormID ≤ `0xFFF`.
- Node keys are never reused. New keys: **24–31**.
- In-game text is English and goes through the language table (`lang/template.txt` via `node tools/extract-strings.mjs`).
- No dependency on another mod, no assets copied from another mod.
- `node tools/check.mjs` must pass; `pwsh -NoProfile -File tools/run-unit-tests.ps1` must pass; `build.bat` must end in `BUILD_OK`.
- Co-save version **16 → 17**; a v16 save loads with no Phoenix cooldowns running.
- The effects work on **any** actor wearing or wielding the item. "Hostile" means hostile to the attacker.
- Nodes are knowledge nodes: not refundable, untouched by Respec and REBOOT.
- Commits in English, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Never `--no-verify`.

## Review Focus

1. **A follower in the fight:** a follower wielding Stormchain must never chain onto the player or another follower. → `PickChainTargets` test with a non-hostile candidate (Task 1).
2. **Two Phoenix Heart items on one actor:** one rescue, one cooldown, not two. → `StrongestMagnitude` test and once-per-actor rule (Tasks 1, 5).
3. **Loading a save that never bought a node** after one that did, in the same session: the enchantment must disappear from the table. → `SyncKnown` clears flags (Task 3), self-test line (Task 6).
4. **An empty weapon (no charge left):** no chain, no breach damage. → covered because the triggers are the carrier effects, which the engine only applies while charge remains (Task 5); checked in game (Task 8).
5. **A corrupt co-save cooldown count:** must not allocate unbounded memory. → bounded read, test via `LoadCooldowns` guard (Task 4).

---

### Task 1: Engine-free logic and its tests

**Files:**
- Create: `src/EnchantLogic.h`
- Create: `tests/enchant_logic_tests.cpp`
- Modify: `tests/CMakeLists.txt` (add `enchant_logic` to the `foreach` list)

**Interfaces:**
- Produces (namespace `Isekai::Enchant::Logic`, header-only, no engine includes):
  - `struct Candidate { float distance; bool hostileToAttacker; bool alive; bool isAttacker; bool isFirstTarget; };`
  - `std::vector<std::size_t> PickChainTargets(std::span<const Candidate> a_candidates, std::size_t a_maxJumps, float a_range)` — indices of alive, hostile, in-range candidates that are neither the attacker nor the first target, nearest first, at most `a_maxJumps`.
  - `float ReflectFraction(float a_summedPercent)` — `clamp(a_summedPercent, 0, 90) / 100`.
  - `inline constexpr float kPhoenixThreshold = 0.25f; inline constexpr double kPhoenixCooldownSeconds = 60.0;`
  - `bool PhoenixShouldFire(float a_healthFraction, double a_now, double a_readyAt)` — `a_healthFraction > 0 && a_healthFraction < kPhoenixThreshold && a_now >= a_readyAt`.
  - `std::optional<float> StrongestMagnitude(std::span<const float> a_magnitudes)` — max, or nullopt when empty.
  - `inline constexpr std::uint32_t kMaxSavedCooldowns = 4096;`

- [ ] **Step 1: Write the failing tests** in `tests/enchant_logic_tests.cpp`, same `EXPECT` harness as `tests/ui_logic_tests.cpp`:

```cpp
void Chain() {
    std::vector<Candidate> c = {
        {300.f, true,  true,  false, true },   // the first target: never again
        {500.f, true,  true,  false, false},   // ok, 2nd nearest
        {200.f, true,  true,  false, false},   // ok, nearest
        {100.f, false, true,  false, false},   // follower: not hostile to the attacker
        {150.f, true,  false, false, false},   // dead
        {50.f,  true,  true,  true,  false},   // the attacker himself
        {900.f, true,  true,  false, false},   // out of range
        {600.f, true,  true,  false, false},   // ok, 3rd
        {650.f, true,  true,  false, false},   // ok but over the jump limit
    };
    const auto picked = PickChainTargets(c, 3, 800.f);
    EXPECT((picked == std::vector<std::size_t>{2, 1, 7}));
    EXPECT(PickChainTargets(c, 0, 800.f).empty());
    EXPECT(PickChainTargets({}, 3, 800.f).empty());
}
void Reflect() {
    EXPECT(ReflectFraction(30.f) == 0.30f);
    EXPECT(ReflectFraction(250.f) == 0.90f);   // five Thorncrowns never reach 100 %
    EXPECT(ReflectFraction(-5.f) == 0.0f);
}
void Phoenix() {
    EXPECT(PhoenixShouldFire(0.20f, 100.0, 50.0));
    EXPECT(!PhoenixShouldFire(0.30f, 100.0, 50.0));   // not low enough
    EXPECT(!PhoenixShouldFire(0.20f, 40.0, 50.0));    // still cooling down
    EXPECT(!PhoenixShouldFire(0.0f, 100.0, 50.0));    // already dead
}
void Strongest() {
    const float two[] = {40.f, 55.f};
    EXPECT(StrongestMagnitude(two) == 55.f);          // two items: one effect, the stronger
    EXPECT(!StrongestMagnitude({}).has_value());
}
```

- [ ] **Step 2: Run them and see them fail**

Run: `pwsh -NoProfile -File tools/run-unit-tests.ps1`
Expected: build error in `enchant_logic_tests` (`EnchantLogic.h` not found).

- [ ] **Step 3: Implement `src/EnchantLogic.h`** with the interfaces above. `PickChainTargets` filters, then `std::stable_sort` by distance, then truncates.

- [ ] **Step 4: Run the tests**

Run: `pwsh -NoProfile -File tools/run-unit-tests.ps1`
Expected: last line `100% tests passed, 0 tests failed out of 5`.

- [ ] **Step 5: Commit** `src/EnchantLogic.h tests/enchant_logic_tests.cpp tests/CMakeLists.txt` — "feat(enchant): engine-free rules for chain, reflect and the phoenix rescue".

---

### Task 2: The records in IsekaiHero.esp

**Files:**
- Modify: `plugin/IsekaiHero.esp`
- Modify: `docs/CREATION_KIT_ESP.md` (new **Part M — System enchantments**, written like Part L: what is in the plugin, how it was verified)

**Interfaces:**
- Produces: a table of local FormIDs that Task 3 copies into `src/Enchantments.cpp`, one row per enchantment: `node key, ENCH id, carrier MGEF id` plus the Stormchain jump `SPEL` id and the System Breach `ARTO` id.

Records (EditorIDs fixed; magnitudes are the first-test values from the spec table, each MGEF base cost set so the table's charge cost lands near vanilla's strongest):

| Node | ENCH (EDID) | Cast / target | MGEFs |
|---|---|---|---|
| 24 | `IsekaiEnchDivineJudgment` | Fire and Forget, Touch | Sun damage on Health (fire resist does not apply), burn-on 3 s; a second damage MGEF conditioned `HasKeyword ActorTypeUndead` doubling it. Look: Dawnguard sun-magic hit shader/art (vanilla DLC forms only). |
| 25 | `IsekaiEnchStormchain` | Fire and Forget, Touch | Shock damage; carrier `IsekaiCarrierStormchain` (Value Modifier, 1 s, hidden, magnitude = jump damage). Look: shock shader recoloured violet. |
| 26 | `IsekaiEnchSoulRend` | Fire and Forget, Touch | Absorb Health + Absorb Magicka. Look: absorb art recoloured dark red. |
| 27 | `IsekaiEnchSystemBreach` | Fire and Forget, Touch | Carrier `IsekaiCarrierSystemBreach` (magnitude = true damage). Look: `IsekaiArtSystemBreach` ARTO, path `meshes/isekai/effects/systembreach_hit.nif`, until Task 7 pointing at a recoloured vanilla shock art. |
| 28 | `IsekaiEnchBulwark` | Constant, Self | Armor rating + damage resist. Look: blue shield hit shader on the wearer. |
| 29 | `IsekaiEnchThorncrown` | Constant, Self | `ReflectDamage` value modifier (magnitude in percent). Look: red thorn shader. |
| 30 | `IsekaiEnchPhoenixHeart` | Constant, Self | Health regen; carrier `IsekaiCarrierPhoenixHeart` (magnitude = rescue heal in % of max health). Look: ember shader. |
| 31 | `IsekaiEnchVoidstep` | Constant, Self | SpeedMult, StaminaRate, fall-damage reduction if a vanilla actor value or perk entry provides it. Look: dark violet shader at the feet. |

Plus `IsekaiSpellStormchainJump` (SPEL, Fire and Forget, Target Actor, shock damage MGEF, violet lightning projectile/art).
Worn restrictions (FormLists of keywords): weapons — none; Bulwark, Thorncrown → `ArmorCuirass`, `ArmorShield`; Phoenix Heart → `ArmorCuirass`, `ClothingNecklace`; Voidstep → `ArmorBoots`.
**No** ENCH record may be referenced as `baseEnchantment` by any other record.

- [ ] **Step 1: Read the current records** with `housecarl_records` on `plugin/IsekaiHero.esp`: highest local FormID in use, and a vanilla reference for each record type (e.g. `EnchWeaponFireDamage04`, `EnchArmorFortifyHealth04`).
- [ ] **Step 2: Write the records** into a copy of the plugin, then replace `plugin/IsekaiHero.esp` with the verified copy.
- [ ] **Step 3: Verify by read-back**

Run: `housecarl_records` on every new FormID; then `node tools/check.mjs`
Expected: every record reads back with the fields above; no `baseEnchantment` points at the eight; check passes including the light-plugin ceiling line.

- [ ] **Step 4: Write Part M** in `docs/CREATION_KIT_ESP.md` with the FormID table and how it was verified.
- [ ] **Step 5: Commit** `plugin/IsekaiHero.esp docs/CREATION_KIT_ESP.md` — "feat(esp): the eight System enchantments, their carriers and looks".

---

### Task 3: Teaching the eight through the skill tree

**Files:**
- Create: `src/Enchantments.h`, `src/Enchantments.cpp`
- Modify: `src/SkillTree.h` (`Zone::kEnchant`, `Effect::kTeachEnchantment`)
- Modify: `src/SkillTree.cpp` (eight nodes, `ZoneName`, `ApplyEffect`, `ApplyOnLoad`, `UnlockAllEnchantments`)
- Modify: `playground/mock.js` (the same eight nodes; `check.mjs` compares zone, x, y, cost, scale)
- Modify: `lang/template.txt` (by `node tools/extract-strings.mjs`)
- Modify: `CMakeLists.txt` (add `src/Enchantments.cpp`)

**Interfaces:**
- Consumes: Task 2's FormID table.
- Produces (namespace `Isekai::Enchantments`):
  - `void Resolve();` — at kDataLoaded, look up all forms by local FormID in `Plugin::kFileName`; log each miss.
  - `bool Teach(std::uint32_t a_nodeKey);` — set `kKnown` on that node's ENCH; false if unresolved.
  - `void SyncKnown(std::span<const std::uint32_t> a_unlockedKeys);` — known flag set for bought keys, cleared for the rest.
  - `bool IsSystemEnchantment(const RE::EnchantmentItem* a_ench);`
  - `std::size_t Resolved();` — how many of the eight resolved (self-test).

Node table additions (zone `kEnchant`, prereq `{ 7, 0 }`, effect `kTeachEnchantment`, size 1.0, not repeatable; coordinates are the builder's, keeping the layout rule):

| key | name | cost | gate |
|---|---|---|---|
| 24 | Divine Judgment | 30 | kH |
| 25 | Stormchain | 35 | kH |
| 26 | Soul Rend | 30 | kH |
| 27 | System Breach | 40 | kA |
| 28 | Bulwark | 30 | kH |
| 29 | Thorncrown | 30 | kH |
| 30 | Phoenix Heart | 40 | kA |
| 31 | Voidstep | 30 | kH |

Descriptions follow the existing style: one line of flavour, one line of effect ("The System judges with light.\nSun damage, doubled against the undead.").

- [ ] **Step 1: Add the eight nodes to `playground/mock.js` only**, run `node tools/check.mjs`
Expected: FAIL naming keys 24–31 as present in the mock but missing in `SkillTree.cpp`.
- [ ] **Step 2: Add zone, effect and the eight nodes in C++**; `ZoneName(kEnchant)` returns `L("zone.enchant", "ENCHANT")`; `ApplyEffect` case `kTeachEnchantment` calls `Enchantments::Teach(a_node.key)`; `ApplyOnLoad` ends with `Enchantments::SyncKnown(unlocked)`; `UnlockAllEnchantments` skips `IsSystemEnchantment(base)` in both loops; `IsRefundable` stays false for the new effect (default branch).
- [ ] **Step 3: Strings and checks**

Run: `node tools/extract-strings.mjs` then `node tools/check.mjs`
Expected: `template.txt` gains `zone.enchant` and the node 24–31 names and descriptions; check prints `N/N checks passed`.
- [ ] **Step 4: Look at the tree** in the playground (`node playground/serve.mjs`, open the tree screen)
Expected: an ENCHANT band, no node sitting on a link between two others, no link crossing a tile. Screenshot into the ledger.
- [ ] **Step 5: Build**

Run: `build.bat`
Expected: ends in `BUILD_OK`.
- [ ] **Step 6: Commit** — "feat(tree): an Enchant band that teaches the System enchantments".

---

### Task 4: Phoenix cooldowns in the co-save (v17)

**Files:**
- Modify: `src/Enchantments.h/.cpp` (cooldown store and play clock)
- Modify: `src/System.cpp` (`kVersion = 17`; write after the v16 fields; read under `if (version >= 17)`; clear on revert/new game)

**Interfaces:**
- Produces:
  - `double PlaySeconds();` — seconds of unpaused play, advanced by a 1 Hz `FrameTick` listener (`TickThrottle`), saved.
  - `double PhoenixReadyAt(RE::FormID a_actor);` / `void PhoenixStartCooldown(RE::FormID a_actor);`
  - `void SaveCooldowns(SKSE::SerializationInterface*);` / `void LoadCooldowns(SKSE::SerializationInterface*);` / `void ClearCooldowns();`
- Layout appended to the state record: `double playSeconds; std::uint32_t count; count × { RE::FormID actor; double readyAt; }`. `count > kMaxSavedCooldowns` → log an error, read nothing more of it, start empty. Actor IDs go through `ResolveFormID`; unresolvable ones are dropped. Expired entries are not written.

- [ ] **Step 1: Write the code** and build (`build.bat` → `BUILD_OK`).
- [ ] **Step 2: Save/load in game** (Task 8 checklist item P1 covers it): a v16 save loads with `Co-save: … version 16` and no cooldowns; a v17 save after a rescue reloads with the cooldown still running.
- [ ] **Step 3: Commit** — "feat(enchant): phoenix cooldowns survive a reload (co-save v17)".

---

### Task 5: The plugin effects

**Files:**
- Modify: `src/Enchantments.cpp`
- Modify: `src/main.cpp` or the kDataLoaded hook where other sinks register (`Enchantments::Install()`)

**Interfaces:**
- Consumes: `EnchantLogic.h` (Task 1), forms (Task 3), cooldowns (Task 4).
- Produces: `void Install();` — registers a `BSTEventSink<RE::TESMagicEffectApplyEvent>` and a `BSTEventSink<RE::TESHitEvent>`.

Behaviour:
- **Trigger for weapon effects:** `TESMagicEffectApplyEvent` with `magicEffect` = Stormchain or System Breach carrier. The engine applies a carrier only while the weapon has charge. Defer one task (`SKSE::GetTaskInterface()->AddTask`), then read the carrier's magnitude from the target's active effects for that caster (already scaled by the table).
- **Stormchain:** candidates = actors in the high process list within 800 units of the target, mapped to `Logic::Candidate` (`hostileToAttacker = attacker->IsHostileToActor(candidate)`); for each picked index cast `IsekaiSpellStormchainJump` instantly from the target to it, magnitude override = carrier magnitude, blame = attacker. Max 3 jumps; no jump re-triggers a chain (the jump spell has no carrier).
- **System Breach:** damage the target's Health directly by the magnitude (`RestoreActorValue(kDamage, kHealth, -m)`), then `ApplyArtObject(IsekaiArtSystemBreach, 1.0)`.
- **Phoenix Heart:** on `TESHitEvent`, for the hit actor: collect the magnitudes of all active Phoenix carriers on it, take `StrongestMagnitude`; if `PhoenixShouldFire(health/permanentHealth, PlaySeconds(), PhoenixReadyAt(id))` → restore that percent of permanent health, start the cooldown, play the ember shader.
- Every handler: no throw out of a sink; nothing done for a null or deleted actor.

- [ ] **Step 1: Write the code**, build (`build.bat` → `BUILD_OK`).
- [ ] **Step 2: Run the unit tests again** (`tools/run-unit-tests.ps1` → 100 %).
- [ ] **Step 3: Commit** — "feat(enchant): stormchain, system breach and the phoenix rescue".

---

### Task 6: Self-test line

**Files:** Modify `src/SelfTest.cpp` (new `CheckEnchantments`, called with the others).

- [ ] **Step 1:** Add `isekai.enchantments`: PASS when `Enchantments::Resolved() == 8` and every ENCH's known flag equals "its node is unlocked"; detail lists any mismatch by node key.
- [ ] **Step 2:** `build.bat` → `BUILD_OK`; `node tools/check.mjs` passes (it checks self-test wiring).
- [ ] **Step 3: Commit** — "feat(selftest): the System enchantments resolve and match the tree".

---

### Task 7: The System Breach mesh (approach C)

**Files:** in the asset pipeline repository; here only `meshes/isekai/effects/systembreach_hit.nif` and the ARTO path in `plugin/IsekaiHero.esp`.

- [ ] **Step 1:** Build the cyan glitch-shard hit effect in the asset pipeline (particle NIF, additive, the projection cyan of the System UI), committed there first.
- [ ] **Step 2:** Copy the NIF here, point `IsekaiArtSystemBreach` at it, verify with `housecarl_nif_inspect` and `housecarl_records`.
- [ ] **Step 3:** `node tools/check.mjs` passes (packaging includes `meshes/`).
- [ ] **Step 4: Commit** — "feat(art): system breach's own glitch effect".

---

### Task 8: Test run in the modlist

**Files:** Create `docs/superpowers/plans/2026-11-20-system-enchantments-testlist.md`.

- [ ] **Step 1:** Write the checklist; each point measurable through the test bridge:
  - E1 the eight nodes show in the ENCHANT band, gates and prices as in Task 3
  - E2 after buying, each enchantment appears at the table; after Arcane Omniscience alone, none of the eight
  - E3 loading an older save without the nodes: none of the eight at the table
  - E4 each look on blade/target or wearer (photos)
  - E5 Stormchain jumps to 3 hostiles, never to a follower; a follower's Stormchain never hits the player
  - E6 System Breach damage lands in full against a high-resist target
  - E7 Phoenix Heart rescues once below 25 %, then not again for 60 s; survives save and reload (P1)
  - E8 Thorncrown reflects, Voidstep speed, stamina and fall damage. Per the spec, whatever the ESP does not achieve here moves into the plugin as a follow-up task with its own test.
  - E9 an empty weapon: no chain, no breach
  - E10 self-test `isekai.enchantments PASS`
- [ ] **Step 2:** Hand the checklist to the SotN project lead for the next collected run; the deploy into the modlist goes through the project lead as usual.
- [ ] **Step 3:** Record the results; anything that failed becomes a ruling or a fix with its own test.
