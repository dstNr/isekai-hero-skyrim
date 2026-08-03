# Creation Kit guide: `IsekaiHero.esp`

Everything the mod needs from a plugin file. Parts A–G built the original ESP; the later
parts were added as features needed them.

## What is outstanding right now

| Part | What | Why now |
|---|---|---|
| **I** | Shock Resist magic effect + ability | **Fixes a live bug** — two milestones have never granted anything. ~3 min |
| **H** | The ten System potions | Their shop cards, icons and code are all done and waiting; only the records are missing. ~30 min |
| **J** | Damage abilities | *Optional.* Unblocks a roadmap item, but nothing references them yet. |

Do **I** first — it is three minutes and repairs something broken. Then **H** if you have
the time. **J** only if you want to go further.

Afterwards send me the FormIDs (the last three hex digits of each) and I wire them in;
see *Afterwards — the FormIDs* under Part H for the three ways to read them off.

> `node tools/check.mjs` currently **fails** on the Part I arity error. That is deliberate:
> it stays red until the ESP has that ability, so the bug cannot be forgotten again.

---

## Why an ESP at all?

Two things are technically impossible without an ESP:

1. **Passives in the magic menu.** An entry under *Active Effects* must be an ability
   spell with a MagicEffect — both are forms, and forms only come from a plugin file.
   (Skyrim does not reliably persist forms created at runtime; on the next load the save
   points at nothing.)
2. **Interdimensional Storage.** A chest is a form plus a placed reference. Same problem —
   except here your *items* would be gone.

## Why only 8 passives and not 61?

The magnitude of an ability effect is **set in stone** in the plugin file. "+25 Health"
is a different spell from "+100 Health". One passive per quest, times three blessing
tiers, would be **183 spells** by hand.

Instead: **one passive per stat value**, whose magnitude the code sets at runtime. The
values visibly add up as you complete quests — and your effects menu is not cluttered
with 61 separate entries.

---

## Preparation

1. Start the Creation Kit.
2. **File → Data**
3. Tick **`Skyrim.esm`** and **`Update.esm`**. Nothing else.
   *(No DLC masters: we reference no DLC content. That keeps the ESP dependency-free —
   it also runs for people without the DLCs.)*
4. Do **NOT** click "Set as Active File" — there is no ESP yet.
5. **OK**, wait for it to load (takes a while, and throws warnings — dismiss them all with
   *Yes to All*).
6. **File → Save**, filename: **`IsekaiHero.esp`**

From now on `IsekaiHero.esp` is the active file; everything you create lands in it.

---

## Part A — Magic Effects (8 of them)

**Object Window** → tree on the left: **Magic → Magic Effect**
→ right-click in the list → **New**

Fill in identically for **each** of the eight entries below:

| Field | Value |
|---|---|
| **ID** | see table |
| **Name** | see table *(this is the text you later see in the effects menu)* |
| **Effect Archetype** | **`Peak Value Modifier`** |
| **Casting Type** | `Constant Effect` |
| **Delivery** | `Self` |
| **Assoc. Item 1** | the actor value — see table |
| **Flags** | **`Recover`** ✅ and `No Duration` ✅ |
| | ❌ **NOT** `Detrimental` |
| | ❌ **NOT** `Hide in UI` — that is exactly what we want to see |
| Magnitude/Duration/Area | leave empty (0) |

> ### ⚠️ `Recover` is the hook everything hangs on
>
> Without `Recover`, Skyrim treats a value modifier **not as a Fortify** but as a one-off
> heal/damage effect: it changes your *current* value once and your *maximum* never. The
> effect then sits cleanly in the menu, has a magnitude — and still does nothing.
>
> Read out of the real game data, all with `Recover` and archetype `34`
> (= `Peak Value Modifier`):
>
> ```
> Fortify Magicka        archetype=34  Recover
> Fortify Stamina        archetype=34  Recover | NoDuration
> Fortify Carry Weight   archetype=34  Recover
> The Steed Stone        archetype=34  Recover
> Resist Magic           archetype=34  Recover | NoDuration
> ```

> **There is no field called "Actor Value".** The actor value sits in
> **`Assoc. Item 1`** (on the left, under *Minimum Skill Level*). The field is generically
> named because its contents depend on the archetype — for `Value Modifier` it lists
> actor values. If objects show up there instead of actor values, toggle
> `Effect Archetype` away and back once; that reloads the list.

The eight:

| ID | Name | Assoc. Item 1 |
|---|---|---|
| `IsekaiME_Health` | `System: Health` | `Health` |
| `IsekaiME_Magicka` | `System: Magicka` | `Magicka` |
| `IsekaiME_Stamina` | `System: Stamina` | `Stamina` |
| `IsekaiME_CarryWeight` | `System: Carry Weight` | `CarryWeight` |
| `IsekaiME_ResistMagic` | `System: Magic Resist` | `ResistMagic` |
| `IsekaiME_ResistFire` | `System: Fire Resist` | `ResistFire` |
| `IsekaiME_ResistFrost` | `System: Frost Resist` | `ResistFrost` |
| `IsekaiME_ResistDisease` | `System: Disease Resist` | `ResistDisease` |

> **Why the plain names?** Under *Active Effects* Skyrim shows the name of the magic
> effect plus the magnitude. With fantasy names it read `+100 System: Burden` — pretty,
> but unreadable. Now it reads `+100 System: Carry Weight`. A status effect you have to
> look up is a bad status effect.

> **Tip:** Create the first one, then **right-click → Duplicate** in the list and only
> change the ID, name and actor value. Saves a lot of clicking.

---

## Part B — Abilities (8 of them)

**Object Window** → **Magic → Spell** → right-click → **New**

For each:

| Field | Value |
|---|---|
| **ID** | see table |
| **Name** | the same as its matching magic effect |
| **Type** | `Ability` |
| **Casting** | `Constant Effect` |
| **Delivery** | `Self` |
| **Cost / Charge Time** | 0 |

Then in the **Effects** box at the bottom → right-click → **New**:
- Select the matching magic effect from Part A
- **Magnitude: `0`**, **Duration: `0`**, **Area: `0`**

> **The magnitude `0` is deliberate.** The code sets it to your real, accumulated value
> on load. A number here would just be a lie that gets overwritten.

| Ability ID | Magic effect from Part A |
|---|---|
| `IsekaiAB_Health` | `IsekaiME_Health` |
| `IsekaiAB_Magicka` | `IsekaiME_Magicka` |
| `IsekaiAB_Stamina` | `IsekaiME_Stamina` |
| `IsekaiAB_CarryWeight` | `IsekaiME_CarryWeight` |
| `IsekaiAB_ResistMagic` | `IsekaiME_ResistMagic` |
| `IsekaiAB_ResistFire` | `IsekaiME_ResistFire` |
| `IsekaiAB_ResistFrost` | `IsekaiME_ResistFrost` |
| `IsekaiAB_ResistDisease` | `IsekaiME_ResistDisease` |

---

## Part C — The chest

**Object Window** → **World Objects → Container**

Find an existing chest in the list, e.g. **`TreasChestSmall01`**.
→ right-click → **Duplicate** *(not "New" — that way you inherit the model and sound for
free)*

Change on the duplicate:

| Field | Value |
|---|---|
| **ID** | `IsekaiStorageContainer` |
| **Name** | `Dimensional Storage` |
| **Respawns** | ❌ **ABSOLUTELY UNTICK** |
| Contents (item list) | delete everything — the chest starts empty |

> ⚠️ **`Respawns` is the critical checkbox.** If it stays set, **Skyrim empties your chest
> automatically every few in-game days.** Your items would be gone.

---

## Part D — omitted

> **Earlier versions of this guide said here to create a cell, put the chest in it and
> mark it as a *Persistent Reference*. That was wrong: Skyrim's Creation Kit does not have
> that checkbox** (it comes from Oblivion/Fallout).
>
> It is needed anyway — a non-persistent reference only exists while its cell is loaded,
> and would be unfindable for the code.
>
> **Solution:** The code creates the reference itself, with
> `PlaceObjectAtMe(base, forcePersist = true)`. Skyrim persists it cleanly in the save.
> So from the CK we only need **the base object from Part C** — no cell, no placed chest.
>
> If you already created the cell and the chest in it: just leave them, the code ignores
> them. Deleting works too, but is not necessary.

---

## Part E — Sound Descriptors (4 of them, via SSEEdit)

The WAV files are already in `Data\Sound\fx\isekai\` (the build workflow does that).

> **Why SSEEdit instead of the Creation Kit?** The CK has a known bug: the sound
> descriptor dialog crashes on close/OK. SSEEdit is the better route here anyway — copying
> a vanilla descriptor brings the Category and Output Model (i.e. the correct
> volume-slider binding) along automatically, instead of having to type them out.

1. **Start `SSEEdit.exe` directly** (e.g. `E:\Modlists\NYA\tools\SSEEdit 4.1.5\`) —
   ⚠️ **not through MO2!** Started directly it sees the base game's load order, i.e.
   exactly our test setup.
2. In the module dialog: right-click → *Select None*, then tick only **`IsekaiHero.esp`**
   (it loads its masters itself) → OK. Wait until the bottom-right says
   "Background Loader: finished".
3. In the tree on the left: expand **`Skyrim.esm` → `Sound Descriptor`**.
4. In the search box **top-left** type `UIMenuOKSD` + Enter → the vanilla descriptor for
   Skyrim's menu-OK click is selected.
5. **Right-click `UIMenuOKSD` → "Copy as new record into..."** → tick `IsekaiHero.esp` →
   enter the new editor ID `IsekaiSND_LevelUp`.
6. Select the new record (now under `IsekaiHero.esp → Sound Descriptor`). On the right, in
   the data pane, find the entry **`ANAM - Sound File`** → double-click the path →
   replace with `fx\isekai\Cinematic_6_1.wav`.
   *(If the copied record lists several sound files: delete the extra rows via
   right-click → Remove, exactly one stays.)*
7. Repeat steps 5–6 **in exactly this order** for the other three:

| No. | Editor ID | Sound File |
|---|---|---|
| 1 | `IsekaiSND_LevelUp` | `fx\isekai\Cinematic_6_1.wav` |
| 2 | `IsekaiSND_WindowOpen` | `fx\isekai\Cinematic_7_2.wav` |
| 3 | `IsekaiSND_ButtonClick` | `fx\isekai\Modern_2_2.wav` |
| 4 | `IsekaiSND_WindowClose` | `fx\isekai\Modern_5_2.wav` |

8. Close SSEEdit → the save dialog appears → leave `IsekaiHero.esp` ticked → OK. (SSEEdit
   makes a backup automatically.)

> ⚠️ **The order matters.** Sound descriptors carry neither a name nor an editor ID at
> runtime, and their file paths are only stored as a hash — the code maps them by FormID
> order, and new records get ascending IDs in creation order. If something is swapped:
> immediately audible, easy to fix.

---

## Part G — The Storage token (via SSEEdit)

Access to the Dimensional Storage goes through an inventory item: "use" it like a potion
→ the chest opens, the item stays (the code intercepts the consumption).

> **Why an ALCH item and not a ring?** A ring would have to be equipped, and equipment
> slots are hotly contested between mods (cloaks, bandoliers, …). Consuming touches not a
> single slot — zero conflict surface. The item's look (model) and name are still free to
> choose; only the inventory category stays "Potions".

In SSEEdit (as in Part E):

1. `Skyrim.esm` → category **`Ingestible`** → select a simple potion
   (e.g. a *Potion of Minor Healing*)
   *(xEdit calls the ALCH record type "Ingestible" — a "Potion" category exists only in
   the Creation Kit.)*
2. Right-click → **Copy as new record into…** → `IsekaiHero.esp`
   → editor ID: **`IsekaiStorageToken`**
3. On the new record:
   - **`FULL - Name`** → `Dimensional Storage`
   - **`Effects`** block → right-click → **Remove** (entirely — no healing effect)
   - **`DATA - Weight`** → `0`
   - **`ENIT`**: `Value` → `0`, **clear `Sound - Consume`** (otherwise it glugs when opened)
4. Save on close.

---

## Part H — System potions (via the Creation Kit)

Ten consumables sold by the System Shop. **No new Magic Effects are needed** — every one
points existing vanilla effects at new magnitudes and durations, so this is data entry,
not record design.

> **Why the Creation Kit and not SSEEdit here?** Parts E and G are single-field copies,
> which xEdit does well. These potions need *several effects each*, and adding entries to
> a record's effect list in xEdit is awkward to impossible depending on the record. The CK
> has a purpose-built effect list with a proper dialog. Use the right tool.

### Setup

Same as **Preparation** at the top, with one difference: `IsekaiHero.esp` already exists,
so load it and make it active.

1. **File → Data**
2. Tick `Skyrim.esm`, `Update.esm` **and** `IsekaiHero.esp`
3. Select `IsekaiHero.esp` and click **Set as Active File** — everything you create now
   lands in it
4. **OK**, dismiss the warnings with *Yes to All*

### Creating one potion

**Object Window** → left tree: **Magic → Potion** → right-click the list → **New**

In the dialog that opens:

1. **ID** → the Editor ID from the tables below
2. **Name** → the display name
3. **Weight** → `0.5` (as vanilla potions),  **Value** → from the table
4. The **Effects** list (lower part of the dialog) → right-click inside it → **New**
   - Pick the effect by the name the player sees — `Restore Health`, `Fortify One-Handed`,
     `Cure Disease`. A second dialog takes **Magnitude**, **Area** and **Duration**.
   - Leave **Area** at `0` throughout — none of these are area effects.
   - Repeat for each line in that potion's row.
5. **OK**

> **Duration 0 is not "no effect".** For an instant effect (Restore, Cure) it means "apply
> once, now". Only the elixirs get a real duration.

> If a name appears twice in the effect picker, take the one whose description matches
> what a potion does — the alchemy variants are the ones potions use.

### Group 1 — Restoratives (instant)

Spammable in combat. These exist separately from the elixirs on purpose: drinking a
one-hour elixir just to top up health would burn its buff.

| Editor ID | Name | Value | Effects (magnitude / duration) |
|---|---|---|---|
| `IsekaiPotionVigor` | System Restorative: Vigor | `250` | Restore Health `10000` / `0` |
| `IsekaiPotionFocus` | System Restorative: Focus | `250` | Restore Magicka `10000` / `0` |
| `IsekaiPotionVitality` | System Restorative: Vitality | `250` | Restore Stamina `10000` / `0` |
| `IsekaiPotionPanacea` | Panacea | `150` | Cure Disease `0` / `0` **+** Cure Poison `0` / `0` |

> Cure Disease and Cure Poison ignore magnitude — they either fire or they don't.
> **Panacea cannot cure Lycanthropy or established Vampirism**; both are quest-locked in
> Skyrim. It does clear Sanguinare Vampiris while still in the three-day incubation.

### Group 2 — Elixirs (one hour)

Every duration below is **`3600`**.

| Editor ID | Name | Value | Effects (magnitude / duration) |
|---|---|---|---|
| `IsekaiElixirSystem` | Elixir of the System | `1200` | Fortify Health `500` / `3600`<br>Fortify Magicka `500` / `3600`<br>Fortify Stamina `500` / `3600` |
| `IsekaiElixirAscended` | Draught of the Ascended | `1200` | Fortify One-Handed `500` / `3600`<br>Fortify Two-Handed `500` / `3600`<br>Fortify Marksman `500` / `3600`<br>Fortify Destruction `500` / `3600` |
| `IsekaiElixirAegis` | Aegis Elixir | `1000` | Resist Magic `85` / `3600`<br>Resist Fire `85` / `3600`<br>Resist Frost `85` / `3600`<br>Resist Shock `85` / `3600` |
| `IsekaiElixirPhantom` | Phantom Draught | `1000` | Invisibility `0` / `3600`<br>Muffle `0` / `3600`<br>Fortify Sneak `500` / `3600` |
| `IsekaiElixirCasting` | Elixir of Endless Casting | `1000` | Fortify Magicka Regen `1000` / `3600`<br>Fortify Magicka `1000` / `3600` |
| `IsekaiElixirTitan` | Titan's Draught | `800` | Fortify Carry Weight `2000` / `3600`<br>Fortify Stamina `500` / `3600`<br>Waterbreathing `0` / `3600` |

> **85 is the ceiling for resistances**, not caution on my part: Skyrim's
> `fPlayerMaxResistance` caps them there, so a larger number changes nothing.
> **Invisibility still breaks on attacking or interacting** — that is vanilla behaviour
> and no potion can override it.

### A note on Value and Weight

These carry a real gold value on purpose, roughly one tier above vanilla's strongest
potions (Ultimate Healing is ~132). An earlier draft of this guide said `Value = 0`, which
was copied from the storage codex in Part G — correct there, because the codex is a UI
token rather than an item, and wrong here: a potion showing no value at all just looks
broken in the inventory.

It opens no exploit. Converting System Points to gold by selling potions would net roughly
2,500 gold nominal for 3 points, before the merchant's markdown and their limited purse —
while the shop's own gold pack pays 100,000 gold for 5 points. Selling potions is about a
hundred times worse than the direct route, so nobody will ever do it.

Weight `0.5` matches vanilla potions. They arrive in the Dimensional Storage rather than
your pockets, so weight only ever applies to what you deliberately carry.

### Afterwards — the FormIDs

The shop looks these up by their local FormID, and only shows a card once the form
resolves — so an incomplete ESP costs nothing, the missing potions simply don't appear.

Three ways to get me the IDs, whichever is least effort:

- **From the Creation Kit:** the potion list has a **Form ID** column. Because the ESP is
  ESL-flagged, the part I need is the **last three hex digits** (e.g. `FE012D80` → `D80`).
- **From SSEEdit:** open `IsekaiHero.esp`, the new records sit under `Ingestible`, and each
  header shows its FormID. Same last three digits.
- **From the game:** start once and send the log — the plugin dumps every form the ESP
  contributes with its ID (see Part F). This one needs no tool at all.

Do them all in one pass and send the list together; I wire them in one edit.

### If the CK will not save, or a record vanishes

- **"Set as Active File" greyed out** — you selected a master (`.esm`). Only `.esp` files
  can be active.
- **The new potion is not in `IsekaiHero.esp` afterwards** — it was created while a
  different file was active. Check the title bar; it names the active file.
- **The CK refuses to save with an error about a missing effect** — an effect row was left
  without a chosen Magic Effect. Delete the empty row.

---

## Part I — the missing Shock Resist ability (fixes a live bug)

**Do this one.** It is not a new feature; it repairs something that has never worked.

### What is wrong

The milestone table grants **nine** distinct actor values, but Parts A and B built only
**eight** ability spells. Each passive is delivered by one ability per actor value, so at
least one is unbacked — and an unbacked passive grants nothing, logs nothing, and looks
exactly like a milestone whose effect is simply small. By name, the missing one is Shock
Resist: the eight cover Health, Magicka, Stamina, Carry Weight, and the Magic/Fire/Frost/
Disease resistances.

Two milestones are affected, and have been from the start:

| Milestone | Quest | Grants |
|---|---|---|
| Scholar | *Hitting the Books* (College of Winterhold) | +5% Shock Resist |
| Nightingale | *Trinity Restored* (Thieves Guild) | +5% Shock Resist |

`node tools/check.mjs` fails on this until it is fixed, and the in-game self-test names
the exact actor value once you run it (F11, if `SelfTestKey` is set).

### The magic effect

**Object Window** → **Magic → Magic Effect** → right-click → **New**, or simply
**Duplicate** `IsekaiME_ResistFire` and change three fields. Everything else is identical
to Part A — archetype `Peak Value Modifier`, casting `Constant Effect`, delivery `Self`,
flags **`Recover`** ✅ and **`No Duration`** ✅, magnitude/duration/area empty.

| ID | Name | Assoc. Item 1 |
|---|---|---|
| `IsekaiME_ResistShock` | `System: Shock Resist` | `ResistShock` |

### The ability

**Magic → Spell** → right-click → **New**, or duplicate `IsekaiAB_ResistFire`. Same as
Part B: type `Ability`, casting `Constant Effect`, delivery `Self`, cost 0, and one effect
entry at **magnitude 0, duration 0, area 0**.

| Ability ID | Magic effect |
|---|---|
| `IsekaiAB_ResistShock` | `IsekaiME_ResistShock` |

Send me its FormID (last three hex digits) with the potions' — it goes into
`kAbilityFormIDs` in `src/Passives.cpp`, and the two milestones start paying immediately,
including retroactively on the next load.

---

## Part J — damage abilities (optional, and nothing uses them yet)

Only worth doing if you want to unblock the roadmap's damage nodes. **Creating these
changes nothing on its own** — no skill-tree node references them until I add one, and I
would rather design those (magnitudes, costs, which rebirth tier) than have you build
records blind.

`docs/IDEAS.md` records damage nodes as blocked because "there is no global spell-damage
actor value". That turned out to be wrong — several exist:

| Actor value | What it scales | Note |
|---|---|---|
| `AttackDamageMult` | **all** weapon damage | Its base is `1.0`, not `0`, so a magnitude of `0.5` means +50%, not +50 points |
| `DestructionPowerModifier` | Destruction spell damage | The actor value the Augmented Flames-style perks use |
| `CriticalChance` | critical hit chance | |
| `WeaponSpeedMult` | attack speed | Base `1.0` as well. **Known for animation conflicts** — the skill tree deliberately avoids attack speed elsewhere, and this is why |

If you make them, follow Part A/B exactly, with `Assoc. Item 1` set to the actor value:

| Magic effect | Name | Ability |
|---|---|---|
| `IsekaiME_AttackDamage` | `System: Attack Damage` | `IsekaiAB_AttackDamage` |
| `IsekaiME_SpellDamage` | `System: Spell Damage` | `IsekaiAB_SpellDamage` |

> The base-`1.0` values are the reason `Progression.cpp` limits passives to actor values
> that add onto a known base. Their magnitudes need testing in game before any node ships
> with a number — send me the FormIDs and I will build the nodes with values we can then
> tune together.

I would skip `WeaponSpeedMult` entirely. Attack speed is the single most conflict-prone
value in Skyrim modding, and the tree already avoids it on purpose.

---

## Part F — Save and start once

1. **File → Save** (overwrites `IsekaiHero.esp`).
2. Make sure the ESP is **enabled** in the game (Steam launcher, MO2, or `plugins.txt` —
   depending on how you start the base setup).
3. Start Skyrim, get into the game (`coc riverwood` is enough).
4. **Let me know.**

The plugin then writes **every form from `IsekaiHero.esp` with its FormID to the log**. I
read them out and wire them into the code — the same method we already used to verify the
quest IDs. So there is no guessed number in the code.

---

## Common pitfalls

| Symptom | Cause |
|---|---|
| Can't find a field "Actor Value" | It's called **`Assoc. Item 1`** (Part A) |
| No "Persistent Reference" checkbox | It doesn't exist in Skyrim's CK — Part D is omitted |
| `Detrimental` / `Hide in UI` not visible | The flags box has three columns; both should stay **empty** anyway |
| CK crashes on load | `Skyrim.esm` **and** `Update.esm` must both be ticked |
| Chest empties itself | Forgot the `Respawns` checkbox (Part C) |
| Passives don't take effect | Ability type is `Spell` instead of `Ability` (Part B) |
| Effect doesn't show in the menu | `Hide in UI` accidentally ticked (Part A) |
| Can't add an effect to a potion in SSEEdit | Use the Creation Kit for those — **Magic → Potion** has a real effect list. Part H was originally written for xEdit and that was the wrong tool. |
