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

**Setting up.** Objectives now arrive on a **timer** — 12 game hours for the first of a
character's life, a day after each completed one. That is right for playing and useless
for testing, so put this in `IsekaiHero.ini` before you start:

```ini
[Quests]
FirstTaskHours = 0
TaskIntervalHours = 0

[Diagnostics]
QuestRerollButton = 1
```

With the first two at 0 the next objective is handed over within a second of the last one
ending, which is the only way to walk this table in one sitting. The third adds a **NEW
TASK** button to the status panel that re-rolls the objective on the spot — you cannot
choose which quarry you draw, so without it testing a specific one means hunting whatever
the dice picked.

**Put all three back afterwards.** They exist for this table and they undo the feature
they test: with them on you will never see the behaviour the rows below are about.

The seven quarries and where to find one quickly:

| Objective | Kill | Where |
|---|---|---|
| Draugr | `ActorTypeUndead` | any barrow — Bleak Falls Barrow is next to Riverwood. Skeletons and vampires count too. |
| wild beasts | `ActorTypeAnimal` | wolves on any road |
| Dwarven automata | `ActorTypeDwarven` | any Dwemer ruin |
| Daedra | `ActorTypeDaedra` | **fastest of all: conjure an atronach and kill it.** It is your summon, so it is a teammate, and it is Daedra. |
| giants | `ActorTypeGiant` | the camps around Whiterun |
| trolls | `ActorTypeTroll` | mountain passes; the road to High Hrothgar |
| dragons | `ActorTypeDragon` | the slowest one to test; leave it |

To spawn one instead, open the console, click nothing, and use a leveled list — e.g.
`player.placeatme 0003BCBC 1` for a draugr. `help "troll" 4` lists what your load order
has if you want a different one.

| Step | Expected |
|---|---|
| **Leave the ini at its defaults.** Start a new character and open the status panel straight after the blessing. | **No objective**, and `none — the System is quiet for another 12h`. Nothing is handed over at rebirth. |
| Sleep 12 game hours. | The first objective arrives on its own — a banner, no menu involved — and it is **wild beasts**, not a random draw. |
| Now set both ini values to 0 and reload for the rest of this table. | The next objective appears within a second of the last one finishing. |
| Load a save. Open the status panel (`RShift+S`). | An **Objective** row: a target, a progress bar, `0 / N`, and `+X SP`. |
| Press **NEW TASK**. | Panel comes straight back with a **different** objective at `0 / N`, and a **banner appears top-centre** naming it. |
| Kill one matching creature (the objective names which). | Counter goes up by exactly **1**, and a line fades in top-centre: `Draugr   1 / 25`. |
| Kill several in a row. | **One** line counting up, not one per kill — progress toasts replace each other. |
| Finish it, then check the status panel. | No objective. Instead: `none — the System is quiet for another Nh`. |
| Wait or sleep out that countdown. | The next objective arrives **on its own**, with a banner, without opening any menu. |
| Compare the target count at level 5 and at level 50. | The higher-level character is sent after noticeably more, and paid proportionally more. |
| At a low level, press NEW TASK repeatedly. | Never dragons or giants — those need level 25 / 20. |
| Let a follower land the killing blow on one. | Counter still goes up — teammates count on purpose. |
| Kill it with a poison, a rune or a summon instead of in melee. | Counter still goes up. Anything the player or a teammate causes counts; that is deliberate, since a mage or a sneak lands few direct blows. |
| Kill something that does *not* match. | Counter does **not** move. |
| Finish the objective. | Level-up sting, System Points increase by the advertised amount, and the next objective follows once the interval is up. |
| Save, quit to desktop, reload. | Same objective, same progress — **and no second objective handed out on the way in**. |

**This last row is the important one.** The objective, its snapshotted target and payout,
the clock and how many objectives the character has had all live in the co-save (v13). If
progress resets to 0 on reload, none of it is persisting; if a **fresh** objective appears
every time you load, the clock is not.

> A counter that never moves at all, on every quarry, points at the kill detection.
> A counter that never moves for **one specific** quarry points at that keyword. That is
> not hypothetical: "Falmer" hunted `ActorTypeFalmer`, which does not exist, and the
> objective could never be finished. `node tools/check.mjs` now fails on an invented
> keyword, and the in-game self-test reports one that is missing from your load order.

### 1.1b Mob danger level (threat labels)

No longer a purchase and no longer a panel — the verdict floats over the actor itself. The
`System Analysis` skill-tree node and the `AnalyzeKey` setting are gone; a save that bought
the node gets its **10 SP refunded on the next load** (the log says so). The old `V`
binding is gone entirely — the display now has an off/on key of its own on **`F10`**
(`ThreatLabelKey`), which is a different job from analysing one target.

| Step | Expected |
|---|---|
| Load a save that had bought System Analysis. | Log: `'System Analysis' was removed from the tree — refunding 10 SP`, and the points are there. |
| Point the crosshair at a wolf. | A coloured verdict floats above it. |
| Look away from it, while it is still calm. | The label goes. The default labels what you aim at, nothing else — until something attacks. |
| Let it attack you. | It keeps its label without being aimed at, and so does anything else fighting you. |
| Walk past a sleeping bandit camp without waking it. | **No labels.** This is the row the default exists for — `hostile` would mark every one of them from across the valley. |
| Back away from something that is fighting you. | The label shrinks and dims with distance, then disappears past the range — but stays **readable** the whole way. Dimming past legible is the bug this row is for. |
| Look at one against snow, and again against a dark cave wall. | Readable on both. The text is outlined, so no glyph edge ever lies directly on the scene. |
| Stand in a market square. | No labels on townspeople. |
| Press **`F10`** (`ThreatLabelKey`). | `Threat display OFF` top-centre, and every label goes. Press again for `ON`. |
| Save, quit, reload after switching them off. | They are back **on** — the toggle is a display switch, not part of the character. |
| Set `ThreatLabelTargets = hostile`, reload. | Every enemy in range is marked again, noticed or not (the pre-0.6.2 behaviour). |
| Set `ThreatLabelTargets = all`, reload. | Now everyone has one, shopkeepers included. |
| Set `ThreatLabelTargets = crosshair`, reload. | Only what you are looking at, even mid-fight. |
| Set `ThreatLabels = 0`, reload. | None at all — but `F10` still brings them up for the session. |
| Open the inventory or a menu. | Labels vanish while it is up — they are world-anchored and the camera is elsewhere. |

The four verdicts come from **target level minus your level**, nothing else:

| Difference | Verdict |
|---|---|
| +20 or more | `LETHAL` |
| +5 to +19 | `DANGEROUS` |
| −14 to +4 | `MANAGEABLE` |
| −15 or less | `TRIVIAL` |

> Most Skyrim enemies are **level-scaled**, so they track your level and land in
> `MANAGEABLE` almost every time. That is correct behaviour, not a bug — but it means you
> have to seek out the other three deliberately. Reliable checks: a chicken or a rabbit
> for `TRIVIAL`; a Giant (level 32) or a Dragon Priest for `DANGEROUS`/`LETHAL` at a low
> level. `player.setlevel 5` before looking at a giant makes both extremes reachable in
> one go.
>
> Analysis is **switched off in Skyrim VR** — the crosshair lookup it needs resolves
> through an SE/AE-only address. It says so rather than guessing.

### 1.1c Manual start and text speed (new settings)

Both live in `[System]` in the ini, and the deployed ini is **kept** by `build.bat` — add
the keys to the one under `Data\SKSE\Plugins` or delete it, or you will be testing the
defaults.

| Step | Expected |
|---|---|
| `AutoStart = 0`, start a **new character**. | Nothing happens when you take control — no messages, no blessing menu. The log says *Reincarnation held: AutoStart = 0*. |
| Walk somewhere else, wait, open a container. | Still nothing. |
| Press the System hotkey (Right Shift + S). | The boot sequence runs from there: the three messages, then the blessing menu. |
| Press the hotkey again after choosing. | Normal status panel — the boot runs once, as before. |
| `AutoStart = 1` (or delete the key), new character. | Unchanged from 0.7.0: it fires by itself once you are in control. |
| `TextSpeed = 3`, open any panel. | Text types out about three times as fast. |
| `TextSpeed = 0`. | No typing at all — the full text is there the moment the panel opens. |
| `TextSpeed = 1`, open a long panel and **click it** while it types. | Jumps to the full text, buttons appear. The click must **not** press a button. |
| Same click test with the **PrismaUI patch** installed. | Identical behaviour. |

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
