# 💡 Feature roadmap

Open ideas for the native-SKSE version, clustered by theme. Shipped ideas are folded into
the one-liner list below — the reasoning behind them lives in git history and
`CHANGELOG.md`, not here.

**Shipped, not detailed here:** repeatable utility nodes (including the Tier 1
resistance/regen batch — Storm Ward, Warded Mind, Arcane Absorption, Iron Skin, Rapid
Recovery), Shattered/Dormant/Custom blessings, skill-tree respec, the Reboot button,
remappable hotkey + `HideSealedNodes` ini, the PrismaUI status dashboard, orphaned-chest
cleanup. See `CHANGELOG.md` for versions.

---

## A · Progression & identity

What the System *is* to the player across a whole playthrough — and across playthroughs.

### Cross-save legacy / "New Game+"

**Status:** 🧠 Idea, not yet specified
**Origin:** the actual trigger for the whole isekai mod — restarting a save from zero
gets old fast, and the isekai premise ("reincarnated with bonuses") should replace
console-cheating it back in, earned across previous playthroughs instead.

Progress currently lives only in one save's co-save (`System.cpp`: `SaveCallback`/
`LoadCallback`; `RevertCallback` hard-resets `g_state` on every New Game). The proposed
approach: a **global progress file** outside any save (e.g. JSON under
`Documents/My Games/Skyrim Special Edition/SKSE/`), paid into at the end of a playthrough
and offered as "Inherit from previous life" in `ShowPowerSelection()` on `kNewGame`,
redeemed through the same mechanism `ApplyReincarnation()` already uses for blessings.

Numeric progression (skills, level, gold, perks, souls) is easy — all already simple
values. Item inheritance is a deliberate split: a curated "legacy items" list (easy,
analogous to the 8 passive abilities) vs. arbitrary self-enchanted items 1:1 (hard,
deliberately out of scope for a first pass).

**Open questions:** numbers only or legacy items too; per-character or global across
saves; automatic payout (e.g. on main-quest completion) or a deliberate menu trigger;
how much may run 2 be sped up without trivialising the premise.

### System Rank (E → S)

**Status:** 🧠 New idea (this session)
**Why:** the cheapest flavor win on the list — no new state, a derived label the status
header and SkyrimNet can both use. Every isekai/tower-climbing story ranks its
protagonist (E/D/C/B/A/S); we track everything needed to compute one (milestones earned,
character level, skill-tree depth/points spent) but never surface it as a single number.

Pure function over existing `State` fields → a rank string, shown in the status header
next to the tier badge. Feeds directly into the SkyrimNet "legend scales" idea below
(NPC chatter reacts to rank, not just tier).

---

## B · What to spend System Points on

The tree's economy — where points come from and where they go.

### Damage nodes (harder, no clean global actor value)

- **Spell Damage** — no global spell-damage multiplier AV; needs a perk or magic-effect
  route.
- **Melee Damage** — same problem; weapon-type-specific (sword/mace/dagger/**bound**) is
  much harder (perks per type — a reporter plays a bound-weapon build via Biggie Traits).
- **Multiple standing-stone bonuses at once** — managing ability spells; complex.

### Analyze / Appraisal

**Status:** 🧠 New idea (this session)
**Why:** the strongest missing isekai signature move. Solo Leveling's "Observation" /
Overlord's "Appraisal" — target something, get a System readout. Low effort: read the
crosshair target, render level/health/resistances/a threat tag through the panel UI we
already have. A natural skill-tree node to gate it behind (System Points well spent), and
a strong SkyrimNet hook ("the System recognises a genuine threat").

### System Quests + System Shop

**Status:** 🧠 New idea (this session), expands on the "System Shop" note from the old
Papyrus backlog (`papyrus/FEATURES.md`) — never built there either.
**Why these two belong together:** System Points currently have exactly one source
(milestones) and one sink (the tree). Once the tree is bought out, points stop mattering.
Quests give a second *source* (points from moment-to-moment play, not just quest
completions); the Shop gives a second *sink* (points still worth earning after the tree is
full) — together they close the loop instead of leaving it dead-ended.

- **System Quests:** the System hands out lightweight objectives ("Slay 12 Draugr") via
  a kill/event tracker + a short timer, paying System Points on completion — read as
  System narration (fits the "[SYSTEM] as a voice" territory SkyrimNet Tier 3 explores).
  Medium effort (kill-tracking + timer state).
- **System Shop:** a dedicated screen (own entry point next to Skill Tree / Storage in
  the status header), a fixed catalog of crafting materials/consumables priced in System
  Points, delivered straight into the Dimensional Storage via the same
  `AddObjectToContainer` path `GrantStartingMaterials` already uses — no new delivery
  mechanism needed. A rotating catalog is a later stretch, not v1.

### Modularity / configurable node values

**Status:** 🅿️ Roadmap
Expose node magnitudes and costs via the ini so players can tune them. Medium effort;
pairs naturally with the Tier 1 batch above (more values worth tuning once they exist).

---

## C · The System as a presence in the world

### SkyrimNet — expanding the AI-NPC integration

**Status:** 🅿️ Parked — the MVP ships (pushing blessing/milestones as world-knowledge,
`src/SkyrimNet.cpp`), untested with a real SkyrimNet install. Continue only after tester
verification.

- **Tier 1 (cheap, stays script-free, builds on the push MVP):** an otherworlder persona
  pushed on reincarnation (fish-out-of-water dialogue both ways); past-life memories
  seeded as memories; the level-up flourish surfaced as a short-lived event so nearby NPCs
  react to the moment; Dormant awakenings additionally high-salience.
- **Tier 2 (needs a decorator, breaks script-freeness):** a live-aura decorator so NPCs
  know the *current* tier/rank/latest title in every conversation, not just past events —
  this is where **System Rank** (cluster A) pays off, letting NPC chatter scale with rank
  rather than only the coarse blessing tier.
- **Tier 3 (marquee, big/risky):** the "[SYSTEM]" itself as an LLM voice generating
  context-aware messages instead of fixed strings — the core of the isekai fantasy, but
  real tone-control and LLM latency/cost risk.

**Deliberately not:** System actions replacing real quests (overlaps with dedicated quest
mods; isekai lives more from the world *reacting* than from generated busywork).

Verification checklist and API details: `src/SkyrimNet.cpp`, `README.md`.

---

## D · Access, platforms & QoL

### Storage codex (physical fallback access)

**Status:** 🅿️ Roadmap — half-built already
A book/item that opens the Dimensional Storage without the System panel, as a belt-and-
braces backup and a table-free access point. **The item form already exists in the ESP**
(`IsekaiStorageToken`, local ID `0x000D7F`, "Dimensional Storage") but the code never
wires it up — only an activation hook calling `Storage::Open()` is missing. Small.

### Gamepad support for the UI

**Status:** 🅿️ Roadmap
PrismaUI 1.5 added gamepad input; the built-in ImGui path would need its own controller
mapping. Its own effort; ties into the VR work below (both are "the built-in UI doesn't
cover every input method" problems).

### Skyrim VR — Phase 2 (in-HMD UI)

**Status:** 🅿️ Blocked on a VR test environment. Phase 1 (VR-loadable, no crash, forms
resolve, PrismaUI as the VR UI) is shipped; see `docs/VR.md` for the full status.
Two routes for getting UI into the headset without depending on PrismaUI's own VR alpha:
(a) in-HMD rendering via an OpenVR overlay / stereo targets (heavy), or (b) a fallback to
the game's own `MessageBox` menus for panels, which the headset renders natively (the
skill tree stays the hard case either way). Only worth designing in detail once there is
a VR install to test against.
