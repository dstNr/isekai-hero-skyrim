# 💡 Feature ideas (C++ version)

A collection point for ideas about the current native-SKSE version before they become
real tasks. Not sorted by priority — simply appended chronologically.

---

## Cross-save legacy / "New Game+"

**Status:** 🧠 Idea, not yet specified
**Origin:** The actual trigger for the whole Isekai mod: on the umpteenth restart of a
save you don't feel like unlocking everything from zero again — and end up cheating it in
anyway. The isekai premise ("you are reincarnated with bonuses") is meant to replace
exactly that, but honestly earned across previous playthroughs instead of via a console
command.

### Core idea

Progress should count not only *within* a save (as now: milestones, blessing, perk points
via `SKSE::SerializationInterface` in the co-save) but also be inheritable **between**
different playthroughs. Whoever finishes run 1 should get back up to speed noticeably
faster in run 2 — not by raw cheating, but through the isekai reincarnation menu that
already exists.

### Why "pick a save → copy items over" does not work that way

- The current progress lives in the co-save of the respective `.ess` file (`System.cpp`:
  `SaveCallback` / `LoadCallback`). That is isolated per playthrough; `RevertCallback` even
  hard-resets `g_state` on every New Game.
- Two saves cannot read each other — completely separate game worlds, separate object
  references.
- Physical items (especially individually enchanted/named ones) are bound to references
  *in exactly that save*. There is no "export item from save A, import into save B" in the
  Skyrim save format — the code would have to recreate the item from scratch in save B.

### Technical approach that works

A **global progress store outside any savegame** — a simple file (e.g. JSON) under
`Documents/My Games/Skyrim Special Edition/SKSE/`, bound to the mod installation, not to a
single savegame. Trivial in C++ via `fstream`, no StorageUtil/Papyrus detour as in the old
version.

**Proposed flow:**
1. At the end of a playthrough (or continuously), progress is paid into this global
   "legacy" file: milestones reached, highest blessing ever chosen, dragon-soul balance,
   possibly unlocked titles.
2. On `kNewGame` (the hook already exists in `System.cpp`) the code reads the legacy file
   and offers an extra entry in the existing reincarnation menu (`ShowPowerSelection()`):
   *"Inherit from previous life"*.
3. Redeeming it uses exactly the mechanism `ApplyReincarnation()` already has for the
   blessings today (setting skills/level/gold/perks) — just fed from real prior progress
   instead of fixed blessing-table values.

**What is easy (numeric progression):**
- Skills, character level, gold, perk points, dragon souls — all already simple values in
  the state, banking/inheriting is straightforward.

**What is harder (item inheritance) — a deliberate split:**
- *Easy:* a curated list of fixed "legacy items" (forms from the ESP, analogous to the 8
  passive abilities). Unlocked globally on first receipt and collectable in the new life,
  e.g. from a kind of Dimensional Storage chest (see old backlog).
- *Hard/later:* carrying arbitrary self-enchanted/crafted items over 1:1 — deliberately
  left out of the first pass.

### Open questions (to clarify before this becomes a task)
- What exactly is inherited: only numbers, or the legacy-item list from the start too?
- Does it inherit per character-save or globally across all saves (including different
  characters)?
- Should paying in happen automatically (e.g. on completing the main quest) or be a
  deliberate menu trigger?
- Balance: how much may run 2 be sped up without making the isekai premise itself trivial?

---

## System skill tree in the interface

**Status:** 🧠 Idea, not yet specified
**Origin:** Two things already sit half-finished in the code and want to be brought
together: `System.h` defines a `SkillFocus` enum (`Balanced/Warrior/Mage/Thief/Custom`)
that is currently **used nowhere** — a pure placeholder. And `Passives.cpp`/`Progression.cpp`
already have a clean mechanism to *derive* actor-value bonuses from a list of "earned"
passives (never to accumulate). A skill tree in the System interface would actually use
both instead of leaving them idle.

### Core idea

A dedicated menu ("System: Skill Tree"), reachable from the existing System interface, in
which the player invests a new currency — **System Points** — into freely chosen nodes.
Each node grants a passive stat bonus (exactly like the existing 8 passives), but the
player decides *which*, instead of it falling automatically out of a milestone.

**Structure:** not a linear path but a branching web with prerequisites — similar to
Skyrim's own perk tree. Organised into three regions that finally give `SkillFocus` a
purpose:

- **Warrior** — Health/Stamina-heavy nodes, armor/melee resistances
- **Mage** — Magicka-heavy nodes, elemental resistances
- **Thief** — Carry Weight/Stamina, evasion/theft-adjacent stats

Plus a small neutral **hub** in the middle from which all three branches diverge —
prerequisite chains can also run across branches (a Warrior node can require a Mage node as
a prerequisite), the web is not strictly split into three silos.

The `SkillFocus` direction chosen at reincarnation could discount or directly unlock the
hub node of its own branch — without locking the other two branches. That finally gives the
so-far consequence-free choice in the reincarnation menu a noticeable effect.

### Currency: System Points

Deliberately **not** the same perk points that flow into Skyrim's own perk system
(`GrantPerkPoints`, capped at 127 by the engine) — a separate, uncapped counter, analogous
to dragon souls.

**Source, two ideas that combine:**
1. Every milestone pays out a small chunk of System Points on top of passive/souls/perk
   points (endpoints more accordingly) — scaled with `RewardScale()` like everything else,
   so the blessing choice keeps counting here too.
2. Dragon souls are fairly useless after the main quest once all word walls are empty. An
   exchange rate (e.g. 1 dragon soul → X System Points) would give the currency a purpose
   beyond shouts — exactly the kind of dragon-soul sink already noted as an open idea
   ("System Shop") in `papyrus/FEATURES.md`.

### Technical approach

**Data model:** a `SkillTree` module analogous to `Progression`/`Passives` — a static node
table (`key`, prerequisites as a list of keys, `Passive` payload, cost in System Points,
branch membership). Unlocked nodes land in `State` (a new field next to
`grantedMilestones`) and are saved in the co-save.

**Passives integration:** `Progression::EarnedPassives()` currently returns only milestone
passives. It's natural to extend that to a shared source (milestones **+** unlocked tree
nodes), so `Passives::Refresh()` keeps deriving everything from one list unchanged, instead
of building a second, parallel application path.

**A limit to clarify early:** `Passives::Refresh()` currently drives only 8 fixed ability
spells (`kAbilityFormIDs`), one per actor value. A skill tree with more nodes than actor
values would need either several nodes per actor value (multiple nodes add to the same
bonus — fits the existing "sum of all passives per AV" model) or additional ability spells
in the ESP for new actor values. The former is doable without an ESP change, the latter
needs Creation Kit work.

**UI:** The existing `SystemWindow` is a pure text+button dialog (title, body, choices) —
no graph layout. A skill tree with nodes, connection lines and prerequisite highlighting is
a new ImGui component (e.g. `UI/SkillTreeWindow.cpp`) that takes the visual style from
`Style.h` (glow border, corner brackets, accent colour) but needs its own layout —
probably with fixed node coordinates per branch rather than automatic graph layout, to keep
render load and complexity small.

**Tie-in with cross-save legacy:** A pool of System Points + unlocked nodes is exactly the
kind of simple, bankable numeric progression the legacy idea above classifies as "easily
inheritable" — a junction point for later if both features happen.

### Open questions (to clarify before this becomes a task)
- Exact exchange rate dragon souls → System Points, if idea 2 happens — and whether it's
  needed at all or milestones alone provide enough flow.
- How big does the web get (rough node count per branch) before it can be designed for
  implementation?
- Respec: are set nodes permanent, or is there (as noted as an idea in
  `papyrus/FEATURES.md`) a respec mechanism that would then also have to refund System
  Points?
- Does the `SkillFocus` direction chosen at reincarnation change only the hub node of its
  own branch, or the cost/availability in the other two branches too?

---

## User feedback backlog (Nexus, v0.4.x)

A collection from a detailed user report. Implemented in v0.5.0: the "Shattered" awakening
(blessing tier without the flat starting grant), repeatable utility nodes (Fleet of Foot /
Beast of Burden / Enduring Vigor) and the optional ini `HideSealedNodes`. The hotkey no
longer fires over an open game menu. Open / on the roadmap:

- **Storage codex as a physical fallback (item 4).** A book/item in the inventory (present
  by default, non-droppable) that opens the Dimensional Storage — as a backup in case the
  ImGui overlay ever sticks, and as access without a crafting station. Implementation: a
  small MISC/BOOK in the ESP + an activation hook (or Papyrus fragment) that calls
  `Storage::Open()`. Small–medium.
  *Note:* Storage is already reachable at any time via the RShift+S panel (not just at a
  table); the cell-reset repair fix (v0.4.0) already softens the "menu sticks" case
  considerably — the codex is belt-and-braces.

- **Fully remappable keybind + choosable modifier (Ctrl instead of RShift).** Needs the
  config (the ini has existed since v0.5.0 — add a `[Hotkey]` section there with scan code
  + modifier and feed `Progression.cpp`/`Input.cpp` from it). Medium.

- **Even more node variety.** Further repeatable stats over the already-supported actor
  values (resistances, regen rates). Move speed already runs via setting `kSpeedMult`
  directly; attack speed is deliberately left out (animation/mod conflicts, classified that
  way by the user themselves).

---

## SkyrimNet — expanding the AI-NPC integration

**Status:** 🅿️ Parked — the MVP is in (pushing blessing/milestones as world-knowledge,
commit `87ab76f`), continue only after tester verification. Order below.

**Tier 1 — cheap, stays script-free (builds on the push MVP):**
- **Otherworlder persona:** on reincarnation, push a player bio (from another world,
  remembers a modern world) → fish-out-of-water dialogue in both directions.
- **Past-life memories:** seed fragments of memory from the old life as memories.
- **Awakening as a moment:** on the level-up flourish, a short-lived event → NPCs react
  immediately to the light/power surge. Dormant awakening additionally high-salience +
  possibly a short voice effect.

**Tier 2 — needs a decorator (small Papyrus glue, breaks the script-freeness):**
- **Live aura decorator:** NPCs know the *current* state in every conversation (tier,
  dormant/awakened, latest titles), not just past events.
- **Legend scales:** NPC chatter grows more reverent with the milestone count / highest
  title.

**Tier 3 — marquee, big/risky:**
- **The "[SYSTEM]" as an LLM voice:** context-aware System messages instead of fixed
  strings, personalised System directives. The core of the isekai fantasy, but tone control
  + LLM latency/cost.

**Deliberately not:** System actions as a replacement for real quests (overlaps with quest
mods; isekai lives more from the world *reacting*).

Details + open verification points (DLL name, `SkyrimNetApi` signatures) see
`src/SkyrimNet.cpp` and `README`.

---

## Skill-tree expansion — "more to spend points on" (user feedback)

**Status:** 🅿️ Roadmap — evaluated, feasibility confirmed against actual actor values,
not yet built (deferred until the VR / perf / storage fixes are tested & released).
**Origin:** player feedback — loves the concept and the UI, wants many more nodes so you
can get "really, really OP"; noted there's fire/frost resist but no shock resist.
(Expands the earlier "even more node variety" note above with concrete effects.)

### Tier 1 — easy wins (mirror the existing repeatable utility nodes: new node + AV)

All backed by real actor values, applied exactly like Fleet of Foot / Enduring Vigor:
- **Shock Resistance** — `kResistShock` (42). Fills the obvious gap next to Emberskin /
  Frostskin.
- **General Magic Resistance** — `kResistMagic` (44). Currently only the Warding milestone
  passive; add a buyable node.
- **Magic Absorption** — `kAbsorbChance` (83). Percent; cap it (e.g. 50–80%).
- **Physical Resistance / Armor** — `kDamageResist` (39). Armor-rating points.
- **Health / Magicka / Stamina Regen** — `kHealRateMult` / `kMagickaRateMult` /
  `kStaminaRateMult` (155–157), base 100 like move speed.

Design: repeatable, NORMAL tier, small per-rank amounts with sane caps so "OP" is earned
over many System Points, not handed over. Same respec handling as the other stat nodes.

### Tier 2 — harder, no clean global actor value

- **Spell Damage** — Skyrim has no global spell-damage multiplier AV; needs a perk or a
  magic-effect route. Meaningful effort.
- **Melee Damage** — same problem; weapon-type-specific (sword / mace / dagger / **bound**)
  is much harder (perks per type). The reporter plays a bound-weapon build (Biggie Traits).
- **Carry multiple standing-stone bonuses at once** — managing ability spells; complex.

### Separate, larger features

- **Gamepad support for the UI.** PrismaUI 1.5 added gamepad input; the built-in ImGui path
  would need its own controller mapping. Its own effort; ties into the VR work.
- **Modularity / configurable values.** Expose node magnitudes and costs via the ini so
  players can tune them. Medium effort; pairs naturally with Tier 1.

### Recommended order

Tier 1 batch first (biggest value per effort, low risk, exactly the request), then
modularity, then the damage nodes / gamepad as their own tracks.
