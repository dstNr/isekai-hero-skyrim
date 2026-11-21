# Creation Kit guide: `IsekaiHero.esp`

Everything the mod needs from a plugin file. Parts A–G built the original ESP; the later
parts were added as features needed them.

## What is outstanding right now

Parts H and I are **done** — the ten potions and the Shock Resist ability are in the
plugin and wired up. What is left:

| Part | What | Why now |
|---|---|---|
| **G fix** | Rename `IsekaiStorageToken` | One field. It currently carries the container's name, so it reads as a stray copy of the storage in the inventory. ~1 min |
| **J** | Damage abilities | *Optional.* Unblocks a roadmap item, but nothing references them yet. |

**K is done** — the System Blade's two records are in the plugin, wired into the shop, and
verified in game on 2026-09-11.
**L is done too**, and without the Creation Kit — see Part L for why and for what to test.
**M is done the same way** — the eight System enchantments; see Part M for the FormIDs.
**N as well** — the twenty voice descriptors of the System and their sound category; see Part N.
The **G fix** is a single `FULL - Name` change — see the warning box in Part G. **J** only
if you want to go further.

Afterwards send me the FormIDs (the last three hex digits of each) and I wire them in;
see *Afterwards — the FormIDs* under Part H for the three ways to read them off.

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
| **Model** | `isekai\storage\empty.nif` — replaces the inherited chest mesh |

> ⚠️ **`Respawns` is the critical checkbox.** If it stays set, **Skyrim empties your chest
> automatically every few in-game days.** Your items would be gone.

> **The model is deliberately empty.** The chest is a real placed reference, parked 3000
> units below the player, and players still saw it standing beside them now and then
> (#38). `meshes/isekai/storage/empty.nif` holds a single `BSFadeNode` with no geometry
> and no collision, so wherever the reference ends up there is nothing to see or bump
> into. Anyone rebuilding this record
> in the CK must set the model back to `isekai\storage\empty.nif` — the duplicated chest
> mesh is exactly what this replaces. `node tools/check.mjs` fails if it is anything else.

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

1. **Start `SSEEdit.exe` directly** from wherever it is installed —
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

> ⚠️ **Do not name it after the container.** An earlier version of this guide said
> `Dimensional Storage` — the same name the CONT record in Part F carries. The result is
> an inventory item that looks like a stray copy of the chest rather than the key to it,
> and it is the first thing a player asks about. **If your plugin already has it under
> the old name, open `IsekaiStorageToken` and change `FULL - Name` to `Storage Codex`** —
> nothing in the code reads the name, so that one field is the whole change.
>
> Players who want no extra item at all can set `StorageCodex = 0` in `IsekaiHero.ini`;
> the panel button remains the normal way in. The codex only exists as a fallback for
> anyone whose hotkey or panel does not work — Skyrim VR above all.

In SSEEdit (as in Part E):

1. `Skyrim.esm` → category **`Ingestible`** → select a simple potion
   (e.g. a *Potion of Minor Healing*)
   *(xEdit calls the ALCH record type "Ingestible" — a "Potion" category exists only in
   the Creation Kit.)*
2. Right-click → **Copy as new record into…** → `IsekaiHero.esp`
   → editor ID: **`IsekaiStorageToken`**
3. On the new record:
   - **`FULL - Name`** → `Storage Codex`
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
| `IsekaiPotionVigor` | System Restorative: Vigor | `250` | Restore Stamina `10000` / `0` |
| `IsekaiPotionFocus` | System Restorative: Focus | `250` | Restore Magicka `10000` / `0` |
| `IsekaiPotionVitality` | System Restorative: Vitality | `250` | Restore Health `10000` / `0` |
| `IsekaiPotionPanacea` | Panacea | `150` | Cure Disease `0` / `0` **+** Cure Poison `0` / `0` |

> ⚠️ **Vigor is Stamina and Vitality is Health**, not the other way round. An earlier
> version of this table had them swapped, and the mod then disagreed with itself: the
> passive granted for Health is called `System: Vitality` while the shop sold
> `Restorative: Vigor` for the same stat (#24). **If your plugin already carries the old
> pairing, open the two records and swap only the magic EFFECT** — `IsekaiPotionVigor`
> takes Restore Stamina, `IsekaiPotionVitality` takes Restore Health. Leave the editor
> IDs, the names and the FormIDs alone: the code keys off the FormID and the shop card
> reads the effect off the record, so that one field per potion is the whole fix.

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

## Part K — The System Blade (a weapon *and* a first-person static)

Two records, not one. Everything else is inherited by duplicating a vanilla sword.

The asset side is finished: `meshes/isekai/weapons/systemblade.nif` with collision, BSX
flags and an attachment point, plus two 2048² BC7 textures. How they were built is in
the [asset pipeline repository](https://github.com/dstNr/isekai-asset-pipeline); none of it matters here.

### K.0 — Before you start the Creation Kit

**The CK reads meshes from the game's own `Data\` folder, and ours are not there.**
MO2 is not involved: the CK is started outside it, `build.bat` copies only the DLL, and
nothing has ever put a mesh into the base game. Skip this step and the model does not
appear in the file picker at all — there is nothing to select.

From the repository root, in PowerShell:

```powershell
robocopy meshes   "$env:SKYRIM_DATA\meshes"   /E
robocopy textures "$env:SKYRIM_DATA\textures" /E
```

Then confirm this file exists before going further:

```
<Skyrim>\Data\meshes\isekai\weapons\systemblade.nif      432,233 bytes
```

These copies exist only for the Creation Kit's benefit. The mod ships its meshes through
the FOMOD; the base-game folder is not the test setup and never was.

### K.1 — Setup

As in Part H's *Setup* — the ESP already exists:

1. **File → Data**
2. Tick `Skyrim.esm`, `Update.esm` **and** `IsekaiHero.esp`
3. Select `IsekaiHero.esp`, click **Set as Active File**
4. **OK**, dismiss the warnings with *Yes to All*

### K.2 — The first-person static, first

Do this one **before** the weapon: the weapon's dropdown can only choose a static that
already exists, and there is no "create one from here" button.

**Object Window** → left tree: **WorldObjects → Static** → right-click the list → **New**

| Field | Value |
|---|---|
| **ID** | `IsekaiSystemBlade1st` |
| **Model** | click the **`...`** button and pick `meshes\isekai\weapons\systemblade.nif` |

Everything else stays at its default. **OK**.

A static is a genuinely small record — vanilla's `1stPersonIronSword` is nothing but an
editor ID, bounds and a model path.

> **Why the same mesh as third person?** Vanilla ships a separate, denser first-person
> version — 1,019 triangles against 425 for the world model. Ours is already 10,159, so a
> second one would gain nothing. Most weapon mods do exactly this.

### K.3 — Duplicate the iron sword

**Object Window** → **Items → Weapon**. Click the **Editor ID** column header to sort,
find **`IronSword`** (the plain one — there are several `FavorAmren…` variants, ignore
them), right-click → **Duplicate**.

You get `IronSwordDUPLICATE000` in the list. Double-click it and change:

| Field | Value |
|---|---|
| **ID** | `IsekaiSystemBlade` |

Duplicating rather than starting from **New** inherits about a dozen fields that are easy
to get wrong and that raise no error when they are: the equip type, the one-handed
animation set, the attack and impact sounds, the block-bash impact, the keyword list. A
short sword is still a sword to the game — only the mesh is smaller.

### K.4 — The two model fields

| Field | Value |
|---|---|
| **Model** | `isekai\weapons\systemblade.nif` — via the **`...`** button |
| **1st Person Model Object** | `IsekaiSystemBlade1st` — the dropdown, from K.2 |

The path is relative to `Data\meshes\`, so **no leading `meshes\`**. Confirmed against
`Skyrim.esm`, where the vanilla record reads `Weapons\Iron\LongSword.nif`.

> ⚠️ **The second field is the whole reason this part has a warning box.** A weapon's
> first-person appearance does not come from its own model. It comes from `WNAM`, which
> points at a **separate STAT record**. Vanilla:
>
> ```
> WEAP  MODL: Weapons\Iron\LongSword.nif
>       WNAM: 0x00036BB0  ->  STAT "1stPersonIronSword"
> ```
>
> **A duplicate inherits `WNAM` → `1stPersonIronSword`.** Change only the Model and you get
> a weapon that is correct in third person and a vanilla iron sword the moment you look
> down at your hand — with nothing anywhere to warn you.

### K.5 — Name, damage, value

| Field | Value | Note |
|---|---|---|
| **Name** | `Flameforged Oathblade` | Must match the shop card exactly — the code carries the same string. |
| **Damage** | `20` | Vanilla's ceiling is Daedric 14, Dragonbone 15. |
| **Critical Damage** | `10` | The iron sword is 3. |
| **Value** | `2500` | The iron sword is 25. |
| **Weight** | `6.0` | The iron sword is 9.0; ours is a shorter blade. |

**These five are balance, not correctness.** Retune them at any time without touching the
mesh, the code or anything else — nothing reads them.

Optional, if you want the shorter blade to *feel* shorter: **Speed** `1.1` (the iron sword
is `1.0`, a Daedric dagger `1.3`). Leave **Reach** at `1.0`; cutting it makes a weapon feel
worse far more than it makes it feel small.

Leave the keyword list alone. It carries `WeapTypeSword` (the animation set),
`WeapMaterialIron` and `VendorItemWeapon` (merchants will buy it).

> `WeapMaterialIron` only decides which perk a **tempering recipe** would require — and
> there is no recipe, so the blade cannot be improved at a grindstone at all. For a weapon
> the System hands over that is a decision, not an oversight. A `COBJ` record can add it
> later if it ever matters.

**OK** to close the dialog.

### K.6 — Save, then four checks

**File → Save.** Then, in order:

1. **The title bar names the active file.** If it does not say `IsekaiHero.esp`, both
   records went somewhere else and are gone.

2. **Expect the FormIDs to be out of range.** ⚠️ This is not a hypothetical: on the first
   run the Creation Kit assigned `0x2311`, `0x2312` and `0x2313`. The plugin is
   ESL-flagged, so a local FormID above **`0xFFF`** is out of range, and the CK does not
   know or care. The existing records sit at `0xD69`–`0xD8C`, nowhere near where it put
   the new ones.

   So this is a step, not a check: **the new records have to be renumbered afterwards.**
   Either right-click the plugin in SSEEdit → *Compact FormIDs for ESL*, or send it to me.
   `tools/check.mjs` refuses the plugin until they are in range, which is what caught it.

3. **The bounds will be zero, and should not stay that way.** The Creation Kit blanked
   `OBND` on both records — `0,0,0 / 0,0,0`. It does *not* recompute bounds from the mesh
   on save, whatever an earlier draft of this section claimed. Vanilla's iron sword reads
   `-6,-11,-1 / 6,63,1` against a mesh spanning Y −11.72 … +63.15, so the numbers do exist
   in a finished record.

   Ours were written from the mesh's measured extents (Y −13.59 … +46.40) as
   `-6,-14,-2 / 6,47,2`. Zero bounds are not fatal, but they are what the game uses for a
   dropped item's pick-up box.

4. **A duplicate you delete does not go away.** The CK left `0x2312` behind as a deleted,
   empty `WEAP`. Harmless, and worth knowing before you go looking for a third record you
   did not make.

5. Copy the saved `IsekaiHero.esp` back into the repository over `plugin/IsekaiHero.esp`,
   then run:

   ```
   node tools/check.mjs
   ```

   Expected: PASS, including `ESP: every FormID is valid for a light plugin`.

> **After a repair, the game folder's copy is the stale one.** The repository holds the
> corrected plugin; copy it back over `<Skyrim>\Data\IsekaiHero.esp`, and do not re-save
> from a Creation Kit session that still has the old one loaded.

### K.7 — Send me the two IDs

The last three hex digits of each; the weapon's goes into the shop catalog. Or start the
game once and send the log — the plugin dumps every form the ESP contributes with its
FormID, which needs no tool at all (see Part F).

For the record, the first pass came out as:

| Record | Editor ID | As saved | After renumbering |
|---|---|---|---|
| STAT | `IsekaiSystemBlade1st` | `0x2311` | **`0xD8D`** |
| WEAP | `IsekaiSystemBlade` | `0x2313` | **`0xD8F`** |

### If something looks wrong

| Symptom | Cause |
|---|---|
| The model picker does not list `systemblade.nif` | K.0 was skipped — the file is not under the game's `Data\meshes\`. |
| Our entry is missing from the **1st Person Model Object** dropdown | K.2 was not done, or the static landed in a different active file. |
| Correct sword in third person, iron sword in first | `WNAM` still points at `1stPersonIronSword`. See the box in K.4. |
| Bounds read `0,0,0 / 0,0,0` | Expected — the CK blanks them and does not recompute. See K.6 step 3. |
| A third, nameless weapon record appeared | A duplicate that was deleted. The CK keeps the empty shell. |
| The blade falls through the floor when dropped | Not a Creation Kit problem — that is the mesh's collision, and it is in there. Report it and I will re-run the collision script from the [asset pipeline](https://github.com/dstNr/isekai-asset-pipeline). |
| Nothing on the hip when sheathed | Expected. The mesh has no scabbard, by decision — most weapon mods ship without one and the blade simply appears whole on the body. |

---

## Part L — The MCM quest (issue #27) — **done, and not in the Creation Kit**

Nothing to do here. The quest was written into the plugin directly, because the Creation
Kit could not do it: Steam's 2026-09-04 update replaced it with **CK 1.7.99.0**, and it
crashes with an access violation when a quest is created. CKPE, which is what normally
patches the editor's crashes, supports up to CK 1.6.1378.1 and has no build for 1.7.99.

So the record was built from the format itself and verified against plugins that already
ship a working MCM Helper menu.

### What is in the plugin

| | |
|---|---|
| `QUST 0xD90` | `IsekaiHeroMCM`, start game enabled, priority 0 |
| Script | `IsekaiHeroMCM`, an empty subclass of `MCM_ConfigBase` |
| Alias 0 | `PlayerAlias`, forced reference `PlayerRef` (`0x14`), script `SKI_PlayerLoadGameAlias` |

**The subclass is required.** MCM Helper's own documentation says to write a script
extending `MCM_ConfigBase` rather than attaching that script itself, and of 3525 plugins
in the test modlist, 127 attach a subclass and **none** attach `MCM_ConfigBase` directly.
Its C++ side would accept the base class, but there is no reason to be the first to try it.

The script is nevertheless empty, and should stay that way: the menu's contents come from
`config.json` and the player's answers are read back from the ini by the plugin. Source in
`mcm-patch/Source/Scripts/IsekaiHeroMCM.psc`, compiled with the Papyrus compiler that ships
with the game, output committed as `mcm-patch/Scripts/IsekaiHeroMCM.pex`.

> **If you ever recompile it, run `node tools\pex-scrub.mjs mcm-patch\Scripts\IsekaiHeroMCM.pex`
> afterwards.** The compiler writes the Windows account name into the `.pex` header, and
> packaging stops dead until it is gone — which is the privacy gate working, not a bug in it.

### How it was verified

Every field was compared against `OCPA.esl`, a shipping mod whose MCM works in the test
modlist, and the record matches it exactly in structure: same subrecord sequence
(`EDID VMAD DNAM NEXT ANAM ALST ALID FNAM ALFR VTCK ALED`), same `DNAM` (flags `0x0101`,
priority 0, type 0), same alias fill, same VMAD layout down to the fragment block. The
VMAD parses to exactly its own length, so there are no trailing bytes. `node tools\check.mjs`
passes, including the light-plugin ceiling: `0xD90` is well under `0xFFF`.

> **One trade-off, stated plainly.** The quest lives in the main `IsekaiHero.esp`, so
> players who do not tick the MCM component get a quest whose script is not installed.
> Papyrus writes one "cannot open store" line for that and carries on; logging is off by
> default, and nothing else changes. The alternative was a second plugin file, which costs
> every player a load-order slot to save a log line that almost nobody will see.

### What is left

Only playing it: install the archive in MO2 with **Mod Configuration Menu** ticked, and check
that **Isekai Hero** appears under Mod Configuration, that all five pages show the shipped
values, that changing the interface scale takes effect without a restart, and — this is the
one the `PlayerAlias` exists for — that the entry is **still there after a save and reload**.

---

## Part M — System enchantments — **done, and not in the Creation Kit**

The eight enchantments of the skill tree's Enchant band
([design](superpowers/specs/2026-11-20-system-enchantments-design.md)), written into the
plugin directly for the same reason as Part L: CK 1.7.99 crashes. 38 new records, local
FormIDs `0xD91`–`0xDB6`, no existing record changed, masters still only `Skyrim.esm` and
`Update.esm` (no DLC master: the Dawnguard sun look was copied into an own shader).

### The FormIDs the code uses

| Node key | Enchantment (`ENCH`) | | Carrier `MGEF` |
|---|---|---|---|
| 24 | `0xDAC` | `IsekaiEnchDivineJudgment` | — |
| 25 | `0xDAD` | `IsekaiEnchStormchain` | `0xDA1` `IsekaiCarrierStormchain` |
| 26 | `0xDAE` | `IsekaiEnchSoulRend` | — |
| 27 | `0xDAF` | `IsekaiEnchSystemBreach` | `0xDA5` `IsekaiCarrierSystemBreach` |
| 28 | `0xDB0` | `IsekaiEnchBulwark` | — |
| 29 | `0xDB1` | `IsekaiEnchThorncrown` | — |
| 30 | `0xDB2` | `IsekaiEnchPhoenixHeart` | `0xDA9` `IsekaiCarrierPhoenixHeart` |
| 31 | `0xDB3` | `IsekaiEnchVoidstep` | — |

Plus `0xDB4` `IsekaiSpellStormchainJump` (SPEL) and `0xD99` `IsekaiArtSystemBreach` (ARTO).

All eight are **base enchantments**: `baseEnchantment` is empty and nothing in the plugin
names them as one. Cast type, target and worn restrictions follow vanilla's own base
enchantments (`EnchWeaponFireDamageBase`, `EnchArmorFortifyHealthBase`).

### What each one holds (first-test values)

Magnitudes are the enchantment's base values; the enchanting table scales them like any
vanilla enchantment (vanilla's strongest variants reach about 3–3.5× their base).

| Enchantment | Effects (magnitude / duration) | Look | Worn on |
|---|---|---|---|
| Divine Judgment | sun damage 30 (no resist) · burn 5/s for 3 s · undead-only 30 more (`HasKeyword ActorTypeUndead`) | gold sun shader `0xD91` on the target, gold blade `0xD92` | weapons |
| Stormchain | shock damage 25 (1 s, vanilla dual value: half to magicka, Resist Shock) · carrier 15 = damage per jump | violet shader `0xD93`, vanilla purple blade | weapons |
| Soul Rend | absorb health 20 · absorb magicka 20 (1 s each, vanilla absorb FX script) | dark-red shader `0xD94`, vanilla red blade | weapons |
| System Breach | carrier 30 = true damage (1 s) | ARTO `0xD99` (applied by the plugin), vanilla blue blade | weapons |
| Bulwark | armor rating (`DamageResist`) +100 · perk `0xDB5`: all damage taken ×0.75 (`ModIncomingDamage`) | blue shield shimmer `0xD95` | body, shield |
| Thorncrown | `ReflectDamage` 13 % | red thorn shader `0xD96` | body, shield |
| Phoenix Heart | `HealRateMult` +40 % · carrier 20 = rescue heal in % of max health | ember shader `0xD97` | body, necklace |
| Voidstep | `SpeedMult` +15 % · `StaminaRateMult` +50 % · perk `0xDB6`: falling damage ×0 (`ModFallingDamage`) | dark violet shader `0xD98` | boots |

Thorncrown's base is 13, not 20: Body and shield both allow it and `ReflectDamage` stacks, so with the table's
top scaling (about 3.5x) two pieces give 2 x 13 x 3.5 = 91 %, below 100 %. A base of 14 would be 98 %, too close;
15 would pass 100 %. Whether the engine itself caps `ReflectDamage` is untested (E8).

Vanilla for comparison: fire damage base 10 (Divine Judgment is 3×), absorb health base 8.
Charge cost lands at 110 for every weapon enchantment (vanilla's dearest base: Banish 113,
Paralyze 102), and 502–507 for the armor ones (vanilla: Fortify Health Regen 503, Sneak 672).

Bulwark's damage reduction and Voidstep's fall-damage immunity are perks, applied through
`PerkToApply` on `IsekaiME_Bulwark` and `IsekaiME_VoidstepSpeed` while the item is worn —
the way vanilla's `ghostHalfDamageEffect` applies its `ModIncomingDamage` perk. Their values
are fixed (the table scales only magnitudes) and **in-game test pending**.

The three carriers are Value Modifiers on `Fame`, an actor value the game does not use, with
`Recover` set: they only hold a magnitude the plugin reads off the active effect. The
Stormchain carrier is hidden; System Breach's and Phoenix Heart's show their description,
since that is the only text that explains them. The two weapon carriers (`0xDA1`, `0xDA5`) also
have `No Death Dispel` (`0x10000000`), as vanilla's soul-trap enchantments do: without it the
engine dispels the carrier on a killing blow, and Stormchain would have no magnitude to chain
with. Worn restrictions are three new keyword lists:
`0xD9A` (`ArmorCuirass`, `ArmorShield`), `0xD9B` (`ArmorCuirass`, `ClothingNecklace`),
`0xD9C` (`ArmorBoots`).

All new records:

| Local id | Type | EditorID |
|---|---|---|
| `0xD91`–`0xD98` | EFSH | `IsekaiShaderDivineJudgment`, `…DivineJudgmentBlade`, `…Stormchain`, `…SoulRend`, `…Bulwark`, `…Thorncrown`, `…PhoenixHeart`, `…Voidstep` |
| `0xD99` | ARTO | `IsekaiArtSystemBreach` |
| `0xD9A`–`0xD9C` | FLST | `IsekaiEnchWornBodyShield`, `IsekaiEnchWornBodyNecklace`, `IsekaiEnchWornBoots` |
| `0xD9D`–`0xD9F` | MGEF | `IsekaiME_DivineJudgment`, `…Burn`, `…Undead` |
| `0xDA0`–`0xDA2` | MGEF | `IsekaiME_Stormchain`, `IsekaiCarrierStormchain`, `IsekaiME_StormchainJump` |
| `0xDA3`–`0xDA4` | MGEF | `IsekaiME_SoulRendHealth`, `IsekaiME_SoulRendMagicka` |
| `0xDA5` | MGEF | `IsekaiCarrierSystemBreach` |
| `0xDA6`–`0xDAB` | MGEF | `IsekaiME_Bulwark`, `IsekaiME_Thorncrown`, `IsekaiME_PhoenixHeart`, `IsekaiCarrierPhoenixHeart`, `IsekaiME_VoidstepSpeed`, `IsekaiME_VoidstepStamina` |
| `0xDAC`–`0xDB3` | ENCH | the eight above |
| `0xDB4` | SPEL | `IsekaiSpellStormchainJump` (Fire and Forget, Target Actor, manual cost 0) |
| `0xDB5`–`0xDB6` | PERK | `IsekaiPerkBulwark`, `IsekaiPerkVoidstep` |

### How it was verified

Every record was **built from a vanilla record's own subrecords**, read from `Skyrim.esm`
(Dawnguard's sun damage and sun shader from `Dawnguard.esm`), with only the named fields
changed: effects from `DLC1EnchSunDamage[Undead]`, `EnchShockDamageFFContact`, `ShockDamageFFAimed`, `EnchAbsorbHealth/MagickaFFContact`,
`PerkBleedingDamage`, `EnchFortifyHealth/HealRate/StaminaRateConstantSelf`; shaders from
`DLC1SunFireFXShader`, `EnchFireFXShader`, `ShockFXShader`, `AbsorbHealthFXS`,
`ShieldSpellFXS`, `FireFXShader`; the ARTO from `FXShockCloak01`; the spell from
`DA16SkullOfCorruptionDamageSpell`; the perks from `ghostHalfDamagePerk` (`ModIncomingDamage`)
and `Cushioned` (`ModFallingDamage`, its conditions dropped). A byte diff of each shader against its reference shows
only colour fields and the palette texture changed. No form of a DLC is referenced: every
FormID inside the new records resolves to a `Skyrim.esm` record or to one of our own (333
references checked).

Read back twice: by an independent parser (sizes parse exactly, old records byte-identical,
header count 89 = 76 records + 13 groups, ESL flag, every field against the table above) and
by houseCARL's record reader on the written file, which named every actor value, flag, link
and magnitude as intended — and caught that index 58 is `WaterWalking`, not `Fame`, before
it shipped. `node tools\check.mjs` passes its ESP checks, including the light-plugin ceiling
(76 records, all ≤ `0xFFF`).

### What is left

Playing it (Task 8 of the plan): whether the constant-effect auras show permanently on the
wearer, how bright the colours read, whether `SpeedMult` takes effect without a carry-weight
nudge, whether the two perks apply while the item is worn and leave with it (Bulwark on body
and shield at once included). One thing the ESP cannot do: a shader "at the feet" only (an
effect shader covers the whole body). `IsekaiArtSystemBreach` now points at its own mesh,
`isekai\effects\systembreach_hit.nif` (cyan glitch shards, built by the asset pipeline's
`tools/build-systembreach-hit.py`, texture `textures\isekai\effects\systembreach_glitch.dds`);
only its `MODL` changed (OBND and the empty `MODT` are still the shock-cloak's). Whether it
plays in game is the same test.

---

## Part N — Voice descriptors of the System — **done, and not in the Creation Kit**

The System's spoken lines ([design](superpowers/specs/2026-11-20-system-voice-design.md)).
One sound descriptor per moment, and one per blessing and rank; each lists every recorded
variant of its line, and the engine picks one of them at random each time it plays. 20 new
records, local FormIDs `0xDB7`–`0xDCA`, plus their own sound category `0xDCB` (added
afterwards, see *Category* below); no older record changed, masters still only
`Skyrim.esm` and `Update.esm`.

The WAV files are `sounds/voice/<sndr>_<nn>.wav` in the repo (the list with every line's
text is `voice/lines.tsv`); packaging puts them in `Data\Sound\fx\isekai\voice\`.

| FormID | Editor ID | Variants | Sound files |
|---|---|---|---|
| `0xDB7` | `IsekaiVOX_reincarnation` | 2 | `fx\isekai\voice\reincarnation_01.wav` … `_02` |
| `0xDB8` | `IsekaiVOX_blessing_normal` | 1 | `fx\isekai\voice\blessing_normal_01.wav` |
| `0xDB9` | `IsekaiVOX_blessing_hero` | 1 | `fx\isekai\voice\blessing_hero_01.wav` |
| `0xDBA` | `IsekaiVOX_blessing_ascended` | 1 | `fx\isekai\voice\blessing_ascended_01.wav` |
| `0xDBB` | `IsekaiVOX_blessing_dormant` | 1 | `fx\isekai\voice\blessing_dormant_01.wav` |
| `0xDBC` | `IsekaiVOX_blessing_custom` | 1 | `fx\isekai\voice\blessing_custom_01.wav` |
| `0xDBD` | `IsekaiVOX_levelup` | 12 | `fx\isekai\voice\levelup_01.wav` … `_12` |
| `0xDBE` | `IsekaiVOX_milestone` | 9 | `fx\isekai\voice\milestone_01.wav` … `_09` |
| `0xDBF` | `IsekaiVOX_rank_d` | 1 | `fx\isekai\voice\rank_d_01.wav` |
| `0xDC0` | `IsekaiVOX_rank_c` | 1 | `fx\isekai\voice\rank_c_01.wav` |
| `0xDC1` | `IsekaiVOX_rank_b` | 1 | `fx\isekai\voice\rank_b_01.wav` |
| `0xDC2` | `IsekaiVOX_rank_a` | 1 | `fx\isekai\voice\rank_a_01.wav` |
| `0xDC3` | `IsekaiVOX_rank_s` | 1 | `fx\isekai\voice\rank_s_01.wav` |
| `0xDC4` | `IsekaiVOX_quest_new` | 9 | `fx\isekai\voice\quest_new_01.wav` … `_09` |
| `0xDC5` | `IsekaiVOX_quest_done` | 9 | `fx\isekai\voice\quest_done_01.wav` … `_09` |
| `0xDC6` | `IsekaiVOX_dormant` | 2 | `fx\isekai\voice\dormant_01.wav` … `_02` |
| `0xDC7` | `IsekaiVOX_reboot` | 2 | `fx\isekai\voice\reboot_01.wav` … `_02` |
| `0xDC8` | `IsekaiVOX_lethal` | 9 | `fx\isekai\voice\lethal_01.wav` … `_09` |
| `0xDC9` | `IsekaiVOX_guild` | 2 | `fx\isekai\voice\guild_01.wav` … `_02` |
| `0xDCA` | `IsekaiVOX_cosave_lost` | 1 | `fx\isekai\voice\cosave_lost_01.wav` |
| `0xDCB` | `IsekaiAudioCategorySystemVoice` (`SNCT`) | — | the category of all twenty, see below |

67 sound files in all. The order matters for the same reason as in Part E: the code maps the
descriptors by FormID, in exactly this order.

**Category.** All twenty use the plugin's own sound category
**`IsekaiAudioCategorySystemVoice`** (`SNCT`, `0xDCB`), which shows in the game's
**Settings → Audio** as its own slider, **System Voice**. Its fields:

| Field | Value |
|---|---|
| `EDID` | `IsekaiAudioCategorySystemVoice` |
| `FULL` | `System Voice` (plain string; the plugin is not localized) |
| `FNAM` | `2` = *Should Appear On Menu* (no *Mute When Submerged*) |
| `PNAM` | `_AudioCategoryMaster` (`0x0EB803`, Skyrim.esm) |
| `VNAM` | `0xFFFF` = static volume multiplier 1.0 (u16 / 65535) |
| `UNAM` | `0xFFFF` = default menu value 1.0 (u16 / 65535) |

The parent is the master category directly, *not* any category under
`AudioCategoryPausedDuringMenu` (`0x0DDDC5`). The lines first used `AudioCategoryVOC`
(`0x0876BB`, the *Voice* slider), whose chain is VOC → `AudioCategoryVOCGeneral` →
`AudioCategoryPausedDuringMenu` → `_AudioCategoryMaster`; since every System window pauses
the game, a line then waited until the window closed (seen in game). The *Voice* and
*Effects* sliders no longer affect the System's voice; the *Master* slider still does. The
game saves the volume of an added category only with SSE Engine Fixes'
`bSaveAddedSoundCategories`; without it the System Voice slider is back at 100 % after a
restart.

**Output model.** The output model is
**`SOMDialogue2D`** (`0x0B5183`), *not* the draugr taunt's `SOMMono03000_verb`
(`0x0F1B12`): that one is a positioned 3D model that fades out between 150 and 3000 units,
while the System's lines are played without a position (like the Part E sounds). `SOMDialogue2D`
is the engine's default output model for unpositioned dialogue; vanilla uses it for the
voices that speak inside the player's head (Azura, Dagon, Namira, Hircine's beast).

> ⚠️ **Lines keep playing while a menu is open.** The category is outside
> `AudioCategoryPausedDuringMenu`, so a line runs on under an open System window (or any
> other pausing menu). The Voice code still starts each line no sooner than
> `Sounds::kResumeGraceMs` after it is asked for: that delay is about the UI's resume pass
> discarding a sound started in the frame a view loses focus, not about the category.

### How it was built

Written byte-wise into the plugin by a script, for the same reason as Parts L and M. Each
record is `IsekaiSND_LevelUp` (`0xD7B`, Part E) with four changes: its own `EDID`, `GNAM`
and `ONAM` as above, and one `ANAM` per variant instead of one. `CNAM` (standard
descriptor), `LNAM` (no loop) and `BNAM` (priority 128, static attenuation 10.57 dB) are
`0xD7B`'s bytes unchanged. The new records sit at the end of the existing `SNDR` group.

The category came second, by another script: one `SNCT` record (fields above, record
version 44, flags 0) in a new top-level `SNCT` group placed right after the `SNDR` group
(the Creation Kit's order is `SNDR`, `DUAL`, `SNCT`; the engine does not care), and the
twenty `GNAM`s changed in place from `0x000876BB` to `0x02000DCB` (`02` = the plugin's own
index, as in its other self-references). The subrecord layout and the u16 encoding of
`VNAM`/`UNAM` were taken from real records (`SexLabAudioVoice` in SexLab.esm,
`_AudioCategoryCustomVoice` in *PC Head Tracking - MCM.esp*: `VNAM` `0xFFFF` = 1.0, `UNAM`
`0xE666` = 0.9) and the Mutagen schema (`Flags`: *MuteWhenSubmerged* = 1,
*ShouldAppearOnMenu* = 2).

### How it was verified

Read back by an independent parser: every record parses to its exact length, 20 new
`SNDR`s with the editor IDs, FormIDs and subrecord order above, every `ANAM` path matching
`voice/lines.tsv` and naming a file that exists in `sounds/voice/` (67 of 67), header count
109 = 96 records + 13 groups, ESL flag and masters unchanged. A byte diff against the
previous plugin shows only three changes: the header's record count, the `SNDR` group's
size, and the inserted records; everything after them is identical. The header's
next-object ID stays `0x2314`: it is already above `0xDCA`, and lowering it would let an
editor hand out IDs that are taken. `node tools\check.mjs` passes.

The category change was read back the same way: every record parses to its exact length,
the `SNCT` at `0xDCB` has exactly the six subrecords and values in the table, all twenty
`IsekaiVOX_*` `GNAM`s are `0x02000DCB` with every other subrecord unchanged, every other
record is byte-identical, header count 111 = 97 records + 14 groups, ESL flag, masters and
next-object ID (`0x2314`, still above `0xDCB`) unchanged. The byte diff shows only the
header's record count, the twenty `GNAM`s and the inserted 140-byte `SNCT` group; everything
after it is identical. `node tools\check.mjs` now also checks that every `IsekaiVOX_*` uses
`0xDCB` and that `0xDCB` is an `SNCT` under `0x0EB803` with the menu flag.

### What is left

Hearing it (in game): that the System Voice slider moves the lines (and the Voice and
Effects sliders do not), that a line keeps playing while a System window is open, that they
sound centred and unfaded wherever the player stands, and whether 10.57 dB of static
attenuation (copied from the UI sounds) leaves them loud enough next to dialogue.

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
