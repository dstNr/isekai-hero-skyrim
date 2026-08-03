# In-game test plan

What still has to be tested by hand, and nothing else.

Two layers already run automatically — **don't spend time re-testing what they prove**:

- `node tools/check.mjs` — data tables, geometry, contracts, save-format wiring.
- The in-game self-test (`SelfTestKey` in the ini) — that forms resolve, keywords exist,
  sweeps find materials, hooks installed.

What neither can do is **anything that changes state**. A self-test that bought a node,
spent points and reverted them would corrupt the save it was meant to check. So this list
is exactly the set of "does the thing actually happen" questions.

## Before you start

1. Set `SelfTestKey = 0x57` (F11) in `IsekaiHero.ini`.
2. Load your save, press F11, and read the report in
   `Documents\My Games\Skyrim Special Edition\SKSE\IsekaiHeroSKSE.log`.
3. **If anything there says FAIL, stop and send me the log.** Everything below assumes
   the preconditions hold; testing features whose forms did not resolve just produces
   confusing results.

Log lines to keep an eye on throughout: the mod writes what it does, so a test that
"seems" to work but logged nothing usually did not work.

---

## Priority 1 — changed most recently, least proven

### 1.1 System Quests (new, and it touched the save format)

| Step | Expected |
|---|---|
| Load a save. Open the status panel (`RShift+S`). | An **Objective** row: a target, a progress bar, `0 / N`, and `+X SP`. |
| Kill one matching creature (the objective names which). | Counter goes up by exactly **1**. |
| Let a follower land the killing blow on one. | Counter still goes up — teammates count on purpose. |
| Kill something that does *not* match. | Counter does **not** move. |
| Finish the objective. | Notification, System Points increase by the advertised amount, and a **different** objective appears immediately. |
| Save, quit to desktop, reload. | Same objective, same progress. |

**This last row is the important one** — it is the first change to the co-save format
(v11). If progress resets to 0 on reload, the state is not persisting.

> A counter that never moves at all, on every quarry, points at the kill detection.
> A counter that never moves for **one specific** quarry points at that keyword — the
> self-test would already have flagged it.

### 1.2 The storage no longer arrives stocked

This inverted how the Dimensional Storage works, so it is worth confirming end to end.

| Step | Expected |
|---|---|
| Start a **new character**, take HERO or ASCENDED, Full. | Storage opens and is **empty**. (It used to arrive with thousands of everything.) |
| Open the Shop, buy **Smithing Materials** (5 SP). | Points drop by 5; storage now holds ingots, leather, gems. |
| Buy it again. | Quantities roughly double — packs stack. |
| Buy **Alchemy Crate** (15 SP). | Ingredients appear, far more of each than the small pack. |
| Try to buy something you cannot afford. | Card is dimmed, clicking does nothing, **no points are lost**. |
| Load an **old save** whose chest was already full. | Contents unchanged — old saves keep what they had. |

### 1.3 Mastery tiers (the cost maths changed, and so did respec)

| Step | Expected |
|---|---|
| Hover a mastery node (Beast of Burden, Storm Ward, …) at rank 0. | Shows its base cost. |
| Buy ranks and watch the price. | Cost **rises every 2 ranks** — the tier changes (Novice → Adept → …). |
| Reach rank 10. | Level-up flourish fires, the node reads **Grandmaster**, and it can no longer be bought. |
| Check the actual stat in the magic menu / character sheet. | The bonus matches what the node advertised, times the rank. |
| Press **Respec**, confirm. | Refund equals **everything you actually paid**, not `base × rank` — the tiers made later ranks dearer. Stats revert. |

> The refund figure is the one to watch. Getting it wrong would quietly hand out or eat
> System Points, and nothing else would tell you.

---

## Priority 2 — long-standing features, but nothing automated proves they work

### 2.1 Skill tree effects actually land

| Node kind | How to confirm |
|---|---|
| Attribute nodes (Vital Surge, Enduring Vigor) | The stat rises, and the `System:` ability under **Active Effects** shows the new total. |
| `kDirectStat` nodes (Fleet of Foot, Storm Ward, Iron Skin) | The value changes on the character sheet. These write the actor value directly and do **not** appear as an ability. |
| Knowledge unlocks (the four Omniscience nodes) | Shouts appear and are usable; enchantments are listed at a table without disenchanting; every ingredient effect is known; spells are in your book. |
| Perk Synthesis | Perk points go up by 5 per purchase, and it stops paying at 255. |
| System Analysis | After buying it, the Analyze key (`V`) reports on whatever you look at. Before buying it, it does nothing. |

**Reload after buying** and confirm everything above still holds — knowledge unlocks and
direct stats are re-derived on load rather than stored, so a load is a real test of them.

### 2.2 Crafting reads the storage in place

The subtlest feature in the mod, and entirely untested by automation.

| Step | Expected |
|---|---|
| Put materials **only** in the Dimensional Storage, carry none. | |
| Stand at a forge. | Recipes you can only afford from storage are listed and craftable. |
| Craft one. | Materials disappear **from the storage**, item appears in your inventory. |
| Same at an alchemy table and an enchanting table. | Stored ingredients and filled soul gems are offered. |
| With a full overhaul (CCOR etc.) installed. | Recipes gated behind "do you carry this?" still appear. |

### 2.3 Milestones

| Step | Expected |
|---|---|
| Complete any quest from the table (e.g. *Before the Storm*). | Flourish, a title, a passive, System Points. |
| Install the mod on a save that already finished several. | On load, a single **SYNCHRONISED** summary that recognises them all retroactively — not one popup per quest. |

### 2.4 Blessings and reboot

| Step | Expected |
|---|---|
| New game → all five choices (Normal, Hero, Ascended, Shattered, Dormant, Custom). | Each grants what the README describes. |
| Dormant: level to the threshold. | It awakens on its own, with a notification. |
| Status panel → **Reboot**, confirm. | Blessing choice re-opens; milestones, tree and points are kept. |

### 2.5 The storage codex

Use the **Dimensional Storage** item from your inventory → the chest opens, and the item
is **still there** afterwards.

---

## Priority 3 — visual, and only you can judge it

Nothing here can fail automatically; the checks only prove nothing overlaps.

- Skill tree in **both** renderers (with and without the PrismaUI patch): are the zone
  frames sensible, the icons legible, the names readable, the mastery rail usable?
- Shop cards: do the icons read as what they sell?
- The status panel with a long title list (79 milestones) — does it still scroll and fit?
- The level-up flourish.

---

## Not testable yet

- **The System potions** — no ESP records exist, so their shop cards are hidden by design.
  See `CREATION_KIT_ESP.md` Part H.
- **VR** — no test environment on this side. See `VR.md`.

---

## When something fails

Send the log (`IsekaiHeroSKSE.log`) plus the F11 self-test report, and say which row of
which table above. The log records what the mod believed it was doing, which is usually
enough to tell "it did not run" from "it ran and did the wrong thing" — and those two
have completely different causes.
