# The power curve: stop moving the world

**Date:** 2026-09-13
**Tickets:** #29 (rewards need a shape), #17 (configurable node values), and the
pace half of #32 that the MCM did not already cover.
**Status:** design approved, ready for an implementation plan.

## The complaint, and why it is really one complaint

A player, on Nexus:

> Currently I find with the mod's "power curve," there's no good way to set the difficulty
> of the game. You either start out with enemies insanely hard before quickly catching up,
> then overtaking by a 'healthy' amount. Or, you start with reasonably powerful enemies,
> and they quickly get trivialized.

The ticket reads this as a *reward shape* problem and proposes nine orthogonal toys —
magicka cost reduction, poison refunds, stagger on power attacks, an infinite quiver. That
reading is wrong, and the tree itself is the evidence: `SkillTree::Effect` already carries
`kShoutCooldown`, `kAllSpells`, `kAllEnchantments` and `kAllIngredients`. Orthogonal power
is not missing. It is simply not where the mod's weight sits.

The weight sits here, from a test log:

```
Reincarnation applied: power=ASCENDED skills=100 level=150 perks=+255 attr=+497 gold=25000
```

| Blessing | Skills | Level | Perks | System Points |
|---|---|---|---|---|
| NORMAL | — | — | — | — |
| HERO | 50 | 25 | 10 | 5 |
| ASCENDED | 100 | **150** | max | 500 (marked a test value in the code) |

`ApplyBlessing` derives the attribute grant *from the level it is about to set*
(`src/System.cpp`, `AttributeBonusPerStat`): `(150 − 1) × 10 / 3 = 497` per stat. And
**Skyrim's levelled lists key off the player's character level.** Setting it to 150 pulls
every scaling enemy to its ceiling in one step.

That single fact produces both halves of the report:

- the level is set → the world jumps to its ceiling → *"insanely hard"*
- the world is already at its ceiling → the player keeps growing → *"quickly get trivialized"*

The tree's attribute nodes are noise against `+1490` total. Replacing them would not have
touched the problem. Removing the level grant does.

The reporter names DORMANT as the worst case, and the model explains that too: DORMANT
jumps twice, at character level 25 and again at 80, and the second jump is the whole 150.

## Decisions taken

| Question | Answer |
|---|---|
| What is #29 actually for? | Making difficulty controllable |
| May the design touch the blessing, or only the tree? | Blessing and tree, the level at the centre |
| How should ASCENDED feel against the world? | Stronger than the world — Skyrim stays Skyrim |
| Which approach? | Drop the level grant, plus #17 |
| Existing level-150 characters? | Offer REBOOT |
| ASCENDED's body, now that enemies no longer follow? | Unchanged (the level-150 equivalent) |
| ASCENDED's skills at 100, which freezes the character level? | Accepted |

Rejected: an orthogonal-effect framework for the ticket's nine examples. Magicka cost,
poison refunds, stagger and quiver refresh each need their own hook into combat, casting or
crafting paths. That is not one spec, it is several, and it is the class of change that
breaks on every runtime bump.

## The design

### 1. The blessing grants a body, not a level

`Blessing::playerLevel` goes to zero for every tier and is replaced by an **attribute
target**:

```cpp
struct Blessing {
    std::uint16_t skillLevel;    // set all 18 skills to at least this
    std::uint16_t attrTarget;    // raise Health/Magicka/Stamina to at least this
    std::int32_t  perkPoints;
    std::int32_t  gold;
    std::int32_t  dragonSouls;
    std::int32_t  systemPoints;
};
```

A target rather than a flat amount, for one concrete reason. Today
`AttributeBonusPerStat(currentLevel, playerLevel)` pays out only the *difference*: a DORMANT
character awakening to ASCENDED at character level 80 receives 233 per stat, not 497. A flat
`+497` field would silently hand them the full amount. A target preserves the behaviour, and
it is the idiom the same function already uses for skills — raise only what is below.

`AttributeBonusPerStat` is deleted.

### 2. The numbers

| | Skills | Attribute target | System Points | Perks | Gold | Souls |
|---|---|---|---|---|---|---|
| HERO | 50 | **180** | 5 | 10 | 2000 | 3 |
| ASCENDED | 100 | **600** | **10** | 255 | 25000 | 20 |

The targets reproduce today's outcome for a fresh character, whose Health, Magicka and
Stamina all start at 100: HERO's 180 is exact (100 + 80), ASCENDED's 600 rounds 597 up by
three points. Nothing meaningful about the body changes; only its coupling to enemy scaling
does. That keeps the balance conversation separate and measurable.

ASCENDED's 500 System Points were marked in the source as a test value — *"Rebalance before
any release"* — and they buy the whole 260-point tree twice over. The replacement is derived,
not invented: HERO seeds 5 at a `RewardScale()` of 2×, so ASCENDED's equivalent at 4× is
**10**. A tier's advantage lives in its reward scale, which is what the design always said;
ASCENDED was the one place it did not hold.

### 3. What this deletes

The granted level never survived a save — it lives in `actorData.level` on the player's
ActorBase, which a load does not restore. An entire mechanism exists only to hold the number
up, and all of it goes:

| Removed | What it did |
|---|---|
| `SetPlayerLevelAtLeast` | wrote the level, never demoted |
| `ReapplyLevelOnLoad` | re-asserted it after every load |
| `RestoreLevelBeforeGrant` | reverted it before every load |
| `g_levelBeforeGrant` | the process-side memory for that revert |
| `AttributeBonusPerStat` | derived attributes from the level |

`RestoreLevelBeforeGrant` exists against a reported bug: once ASCENDED had written 150 into
the shared ActorBase, loading a save the System had never touched left that character at 150,
firing level-gated quest mods in a game the mod had not run in. Removing the grant removes
the bug with it.

`RestoreBaseline` keeps its `preLevel` restore. It is a no-op for characters granted no
level, and it is what corrects the ones that were.

### 4. #17 — two multipliers, not eighty-eight keys

Read literally, #17 asks for every node's cost and every bonus amount in the ini: 22 nodes ×
4 values. This project has already rejected that shape once, when the alchemy ingredients
became a runtime enumeration rather than a hand-kept FormID list, and for the same reason —
a list that long is that many chances to be wrong.

```ini
[Progression]
NodeCostScale   = 1.0    ; every node price times this
NodeEffectScale = 1.0    ; every bonus amount times this

HeroAttributeTarget     = 180
HeroSkillLevel          = 50
HeroSystemPoints        = 5
AscendedAttributeTarget = 600
AscendedSkillLevel      = 100
AscendedSystemPoints    = 10
```

`NodeCostScale` is literally the "system point cost multiplier" #32's reporter asked for.
Gold, dragon souls and perk points stay hard-coded: they are flavour, not curve, and they can
be exposed when somebody asks.

**The two multipliers behave differently, and the code decides which is which.**

`NodeEffectScale` applies **live**. Node effects are re-derived on every load, and
`kDirectStat` is documented as idempotent — *"re-applying on load or after another purchase
never stacks or drifts"*. Turning this knob mid-save recomputes cleanly.

`NodeCostScale` must not. `RespecRefund` computes a refund from `Node::cost` as it reads
*now*, and the surrounding code works visibly hard to keep one promise:
`MasteryCostToRank` sums every individual tier purchase *"so a respec gives back exactly what
was paid"*. A scale changed mid-save breaks exactly that promise — buy at 1.0, refund at 2.0.
So it is **snapshotted into the co-save at reincarnation** and fixed for that character. This
is the pattern `State::questReward` already uses, for the same reason. Changing it for an
existing character is what REBOOT is for.

All eight values also appear in the MCM, on the existing *The System* page, as a
**Progression** section beside *Bounties and professions*.

### 5. Migration, and co-save v16

Co-save goes to **v16** with two new fields:

```cpp
float nodeCostScale = 1.0f;   // snapshotted at reincarnation
bool  levelGrantRetired = false;  // one-shot migration marker
```

On the first load after the update, a character that is reincarnated, holds a v14 baseline
and has `levelGrantRetired == false` gets `actorData.level` written once from
`State::preLevel`, and the flag set. After that the mod never touches the character level
again.

This is deliberately not conditional on measuring what the engine would otherwise do. The
source comment claims the level "reverted to 1 on load" without the re-assert; whether a
given save would have kept 150 or dropped to 1 no longer matters, because the migration
states the answer explicitly. Ten lines and a bool, against a wager.

| Character | Outcome |
|---|---|
| New, after the update | No level grant, attribute target instead of derivation. Nothing to migrate. |
| Existing, with a v14 baseline | Level corrected once on load. Attributes, skills, perks and points all kept. |
| Existing, no baseline (pre-v14, SHATTERED) | Left as-is. REBOOT already reports honestly that it cannot hand the body back. |
| DORMANT, not yet awakened | Awakens without the world jumping — the reported worst case. |

`RebootSystem` needs no change. It already restores the baseline and re-opens the blessing
choice while keeping milestones, the tree and System Points, and it is the documented route
for a player who wants the new numbers on an old character.

### 6. Consequences accepted, not discovered later

**ASCENDED freezes the character level.** All 18 skills at 100 means no skill can rise, and
Skyrim only grants levels for skill increases. An ASCENDED character stays at the level they
reincarnated at, permanently. For "stronger than the world" that is the honest consequence —
but level-gated content in other mods stays locked at that level, and this belongs in the
mod description, not in a bug report six months from now.

**The world stops moving with the player.** That is the point of the change, and it means a
character who reincarnates at level 5 lives in a level-5 world. The reward for playing on is
the tree and the milestones, not a rising floor.

## Verification

In game, on the modlist, from the packaged archive:

| Check | Expected |
|---|---|
| Fresh ASCENDED reincarnation, read the log line | `level=0`, and the attribute figure is the difference up to 600 — `+500` on a character still at the 100 baseline |
| `player.getlevel` in the console straight after | The character's own level, unchanged by the blessing |
| Kill a scaling enemy near the start | It is at its own level, not at a ceiling |
| Save, reload, `player.getlevel` again | The same number. Nothing re-asserts, nothing reverts. |
| Load an existing ASCENDED save made before the update | Level corrected once; the log says so; attributes and skills unchanged |
| Load that save a second time | No second correction — the flag held |
| Load a save the System has never touched, in the same session | Level untouched. This is the reported ActorBase bug, and it cannot recur. |
| DORMANT character crossing level 80 | Awakens; the world does not jump |
| Set `NodeEffectScale = 2.0`, reload | Node bonuses double, no drift and no stacking across loads |
| Set `NodeCostScale = 2.0`, load an existing character | Prices unchanged — the snapshot holds |
| REBOOT that character, choose ASCENDED again | Prices now doubled; a respec refunds exactly what was paid |

## Out of scope

The nine orthogonal effects from #29's body. They are a content pass on top of a curve that
works, and the curve has to work first. #16 (damage nodes) belongs with them, not here.
