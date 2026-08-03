# 💡 Feature roadmap

Open ideas for the native-SKSE version, clustered by theme. Shipped ideas are folded into
the one-liner list below — the reasoning behind them lives in git history and
`CHANGELOG.md`, not here.

**Shipped, not detailed here:** repeatable utility nodes (including the Tier 1
resistance/regen batch — Storm Ward, Warded Mind, Arcane Absorption, Iron Skin, Rapid
Recovery), mastery tiers on those nodes (10 ranks / 5 named tiers, escalating cost),
Shattered/Dormant/Custom blessings, skill-tree respec, the Reboot button, remappable
hotkey + `HideSealedNodes` ini, the PrismaUI status dashboard, orphaned-chest cleanup,
System Rank (E–S) in the status panel, System Analysis (the Analyze hotkey), the storage
codex (physical fallback access to Dimensional Storage), the skill tree's zone redesign
(named branches, node labels, size hierarchy), and the System Shop — now its own screen of
item cards selling material packs, gold and (pending their ESP records) System potions,
which is also how the Dimensional Storage gets filled at all since it stopped arriving
pre-stocked. See `CHANGELOG.md` for versions.

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

---

## B · What to spend System Points on

The tree's economy — where points come from and where they go.

### Damage nodes (harder, no clean global actor value)

- **Spell Damage** — no global spell-damage multiplier AV; needs a perk or magic-effect
  route.
- **Melee Damage** — same problem; weapon-type-specific (sword/mace/dagger/**bound**) is
  much harder (perks per type — a reporter plays a bound-weapon build via Biggie Traits).
- **Multiple standing-stone bonuses at once** — managing ability spells; complex.

### System Quests — ✅ shipped

A standing kill objective paying System Points, matched by ActorType keyword
(`src/Quests.cpp`). Shipped without the timer the original note assumed: expiry needs
game-time tracking, a failed state and its UX, and in a mod about feeling OP a running-out
timer is mostly friction. The state fields are in the co-save (v11), so adding one later
would not need another format bump.

**Possible follow-ups:** more objective *kinds* than kills (explore N dungeons, craft N
items, sell N septims' worth) — each needs its own event source, which is why the first
pass is kills only. Also: several objectives at once, which needs a list layout in the
status panel rather than the single row it has now.

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
  now that **System Rank** is shipped (`Progression::SystemRank()`), this is where it pays
  off: NPC chatter can scale with rank rather than only the coarser blessing tier.
- **Tier 3 (marquee, big/risky):** the "[SYSTEM]" itself as an LLM voice generating
  context-aware messages instead of fixed strings — the core of the isekai fantasy, but
  real tone-control and LLM latency/cost risk.

**Deliberately not:** System actions replacing real quests (overlaps with dedicated quest
mods; isekai lives more from the world *reacting* than from generated busywork).

Verification checklist and API details: `src/SkyrimNet.cpp`, `README.md`.

---

## D · Access, platforms & QoL

### Gamepad support for the UI

**Status:** 🅿️ Roadmap
PrismaUI 1.5 added gamepad input; the built-in ImGui path would need its own controller
mapping. Its own effort; ties into the VR work below (both are "the built-in UI doesn't
cover every input method" problems).

### Skyrim VR — Phase 2 (in-HMD UI)

**Status:** 🅿️ Blocked on a VR test environment (or a responsive tester). Phase 1
(VR-loadable, no crash, forms resolve, PrismaUI as the VR UI) is shipped.
**The three routes are now costed out in `docs/VR.md`** — including why the ImGui VR
Helper's "four integration steps" do not cover our case (it exposes no per-frame render
callback, and our own frame tick is precisely what is broken in VR). Read that before
picking this up; the analysis is done, only the decision is open.
