> ⚠️ **Legacy / archived.** This documents the old Papyrus implementation of the mod
> (tag `papyrus-v1.0`), which has been superseded by the native SKSE C++ version. Kept
> for reference only.

# Isekai Hero - Creation Kit guide (v2.0)

**For Skyrim Special Edition 1.6.x / Anniversary Edition & Creation Kit v1.6+**

This guide walks you step by step through building the Isekai Hero mod in the Creation Kit.

⚠️ **IMPORTANT:** the Creation Kit has some bugs. If something doesn't work, check the
troubleshooting section!

---

## 📋 Preparation

### Required tools
1. **Skyrim Special Edition** (Steam) — must be installed
2. **Creation Kit** (Steam → Library → Tools → Creation Kit)
3. **Scripts from this repository** — copy the `Scripts/Source` folder into your Skyrim
   Data directory

### First-time CK setup
1. Start the Creation Kit **as administrator** (right-click → Run as administrator)
2. On first start: **File → Select Paths...**
3. Make sure the paths are correct:
   - **Skyrim:** `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`
   - **Scripts:** `Data\Scripts` (important for compilation!)

---

## 🚀 Step 1: Create the plugin

### 1.1 Start the CK & load masters

1. Open the Creation Kit (wait until it has loaded — can take 1-2 minutes)
2. **File → Data...** (top left in the menu)
3. The "Data" window opens
4. Tick **Skyrim.esm**
5. Optional: tick **Update.esm** (if present)
6. Click **OK** (not "Set as Active File!" yet)
7. **Wait** until the CK has finished loading (status bar at the bottom)
   - You see "Loading..." in the bottom bar
   - Wait until "Done" or similar appears

### 1.2 Create a new plugin

1. **File → Save** (or Ctrl+S)
2. The "Save Plugin" dialog opens
3. **IMPORTANT:** pick the right folder!
   - Navigate to: `Steam\steamapps\common\Skyrim Special Edition\Data`
4. Enter the filename: `IsekaiHero`
5. Make sure the type is **.esp**
6. Click **Save**

✅ **Success:** you now see "IsekaiHero.esp" as the active file at the top of the window

---

## 🏷️ Step 2: ESL flag (recommended)

So the mod doesn't count towards the 255-plugin limit:

### 2.1 Compact the form IDs

1. **File → Compact Active File Form IDs**
2. A warning appears: "This will change all Form IDs..."
3. Click **YES**
4. Wait a moment

### 2.2 Convert to ESL

1. **File → Convert Active File to Light Master**
2. Click **YES** in the dialog

### 2.3 Save

1. **File → Save**
2. Click **YES** to overwrite

✅ **Check:** the file should now be only ~100-200 KB

---

## 🎯 Step 3: Create the main quest

### 3.1 Create the quest

1. In the **Object Window** (the big window with many categories):
   - Click the **+** next to "Character"
   - Click the **+** next to "Quest"
   - Or: type "Quest" in the filter field at the top

2. **Right-click** in the empty list on the right
3. Choose **New** (or press Insert)
4. The "Quest" window opens

### 3.2 Quest settings

Fill in these fields (top to bottom):

| Field | Value | Description |
|------|------|--------------|
| **ID** | `IsekaiIntroQuest` | Unique identifier |
| **Name** | `Isekai Hero - System Awakening` | Display name |
| **Type** | `None` | Quest type |
| **Priority** | `50` | Importance (higher = more important) |
| ☑ **Start Game Enabled** | ON | Starts automatically |
| ☑ **Run Once** | ON | Runs only once |

**IMPORTANT:** do NOT click OK yet!

### 3.3 Add the script

1. In the quest window: click the **Scripts** tab (top)
2. Click the **Add** button (right)
3. "Add Script" opens
4. Choose from the list: `IsekaiIntroQuest`
   - If it's not in the list: scripts must be compiled first (see step 9)
5. Click **OK**

### 3.4 Link properties (CRITICAL!)

This step is often wrong in guides — here in detail:

1. The script `IsekaiIntroQuest` now appears in the list
2. **Click the script** (click once so it's highlighted blue)
3. Click the **Properties** button (next to the list)
4. The "Properties" window opens

The properties are already declared in the script as **auto-properties** — you do **not**
have to create them manually with "Add Property". They appear automatically once the
script is attached. You only set their **values** (Edit Value → Select Form):

| Property | Type (automatic) | Set value to |
|----------|-------------------|-----------------|
| `DialogScript` | `IsekaiDialogScript` | Quest with `IsekaiDialogScript` |
| `PowerScript` | `IsekaiPowerScript` | Quest with `IsekaiPowerScript` |
| `ProgressionScript` | `IsekaiProgressionScript` | Quest with `IsekaiProgressionScript` |
| `PerkDefs` | `IsekaiPerkDefinitions` | Quest with `IsekaiPerkDefinitions` |
| `MCM` | `IsekaiMCMScript` | Quest with `IsekaiMCMScript` |

> ⚠️ The property **type** is the respective script name (NOT `Quest`). It is taken
> automatically from the script declaration.

**IMPORTANT:** the target quests are only created in step 8. So leave the values on
**None** for now and link them later (easiest via **Auto-Fill** once all quests exist).

Then: **OK** in the Properties window, **OK** in the quest window.

✅ **Check:** the quest now appears in the Object Window list

---

## 📊 Step 4: Quest stages

### 4.1 Open the quest

1. Double-click `IsekaiIntroQuest` in the list
2. Click the **Quest Stages** tab

### 4.2 Add stages

For each stage:
1. Click **New** (right)
2. Enter the **stage number**
3. Optional: add a **Log Entry**

Create these stages:

| Stage | Log Entry | Description |
|-------|-----------|--------------|
| 10 | *(leave empty)* | Waiting for spawn |
| 20 | `[SYSTEM] Boot sequence initiated` | System starts |
| 25 | `[SYSTEM] Dimensional origin confirmed` | World chosen |
| 30 | `[SYSTEM] Power level selected` | Power level chosen |
| 40 | `[SYSTEM] Skill focus assigned` | Skills chosen |
| 50 | `[SYSTEM] Equipment summoned` | Equipment chosen |
| 55 | `[SYSTEM] Wealth allocated` | Gold chosen |
| 60 | `[SYSTEM] Applying changes...` | Values are set |
| 100 | `[SYSTEM] Reincarnation complete` | Done |

**How to add a stage:**
1. Click **New**
2. Enter the number (e.g. 10)
3. Click the stage (it turns blue)
4. In the "Log Entry" area at the bottom:
   - Click **New**
   - Enter the text
   - Click **OK**

5. Click **OK** in the quest window

---

## 💬 Step 5: Message forms (dialogs)

Message forms are the dialog boxes that appear in-game.

### 5.1 Create a message form

1. In the Object Window:
   - Open "Character"
   - Choose "Message"
2. **Right-click** in the list → **New**

### 5.2 IsekaiMsg_WorldSelect

Fill in:

| Field | Value |
|------|------|
| **ID** | `IsekaiMsg_WorldSelect` |
| **Name** | `SYSTEM: Dimensional Origin` |

**Message Text** (copy it completely):
```
From which world do you hail, Reincarnated One?

[1] EARTH - Modern world, no magic
[2] JAPAN - Land of truck-kun incidents  
[3] KOREA - Dungeons and hunters
[4] FANTASY WORLD - Swords and sorcery
[5] SCI-FI FUTURE - Advanced technology
[6] APOCALYPTIC - Survival and mutations
```

**Buttons** (very important!):
1. In the "Buttons" area (bottom):
2. Click **New** for each button
3. Enter the text (one per button):
   - Button 1: `Earth`
   - Button 2: `Japan`
   - Button 3: `Korea`
   - Button 4: `Fantasy World`
   - Button 5: `Sci-Fi Future`
   - Button 6: `Apocalyptic`

⚠️ **IMPORTANT:** the order is critical! Button 0 = Earth, Button 1 = Japan, etc.

4. Click **OK**

### 5.3 IsekaiMsg_PowerChoice

| Field | Value |
|------|------|
| **ID** | `IsekaiMsg_PowerChoice` |
| **Name** | `SYSTEM: Status Allocation` |

**Message:**
```
Choose your reincarnation blessing:

[1] NORMAL - No memories, native start
[2] HERO - Level 1 | Skills 100 | 50 Perks
[3] ASCENDED - Level 255 | Max Skills | 500 Perks
[4] DECLINE - Refuse the blessing
```

**Buttons:**
1. `NORMAL`
2. `HERO`
3. `ASCENDED`
4. `DECLINE`

### 5.4 IsekaiMsg_SkillFocus

| Field | Value |
|------|------|
| **ID** | `IsekaiMsg_SkillFocus` |
| **Name** | `SYSTEM: Skill Allocation` |

**Message:**
```
Select your past life's expertise:

[1] BALANCED - Equal mastery
[2] WARRIOR - Combat mastery
[3] MAGE - Arcane mastery
[4] THIEF - Shadow mastery
[5] CUSTOM - Configure later
[6] BACK
```

**Buttons:**
1. `BALANCED`
2. `WARRIOR`
3. `MAGE`
4. `THIEF`
5. `CUSTOM`
6. `BACK`

### 5.5 IsekaiMsg_EquipmentChoice

| Field | Value |
|------|------|
| **ID** | `IsekaiMsg_EquipmentChoice` |
| **Name** | `SYSTEM: Equipment Summoning` |

**Message:**
```
Choose your starting gear:

[1] HUMBLE - Iron gear, basic supplies
[2] ADVENTURER - Steel gear, potions
[3] HERO - Daedric items, ultimate potions
[4] NONE - Pure skill only
[5] BACK
```

**Buttons:**
1. `HUMBLE`
2. `ADVENTURER`
3. `HERO`
4. `NONE`
5. `BACK`

### 5.6 IsekaiMsg_WealthChoice

| Field | Value |
|------|------|
| **ID** | `IsekaiMsg_WealthChoice` |
| **Name** | `SYSTEM: Wealth Allocation` |

**Message:**
```
Choose your starting fortune:

[1] MODEST - 1,000 gold
[2] WEALTHY - 10,000 gold
[3] NOBLE - 50,000 gold
[4] MERCHANT PRINCE - 100,000 gold + gems
[5] BACK
```

**Buttons:**
1. `MODEST`
2. `WEALTHY`
3. `NOBLE`
4. `MERCHANT PRINCE`
5. `BACK`

### 5.7 IsekaiMsg_SystemComplete

| Field | Value |
|------|------|
| **ID** | `IsekaiMsg_SystemComplete` |
| **Name** | `SYSTEM: Reincarnation Complete` |

**Message:**
```
╔══════════════════════════════════════╗
║     REINCARNATION COMPLETE!          ║
╠══════════════════════════════════════╣

Welcome to Nirn, Reincarnated One.
Your journey begins now.

May your legend be written in the stars.

╚══════════════════════════════════════╝
```

**Buttons:**
1. `Continue`

---

## 📦 Step 6: Create FormLists

FormLists are containers for multiple items.

### 6.1 Create a FormList

1. Object Window → open "Items"
2. Choose "FormList"
3. **Right-click** → **New**

### 6.2 FormLists for Isekai Hero

Create these FormLists (one after another):

#### Isekai_Weapons_Humble
- **ID:** `Isekai_Weapons_Humble`
- **Add a FormID:**
  1. Click **Add** (right)
  2. "Select Form" opens
  3. Enter: `IronSword` (or form ID: `00012E4E`)
  4. Click **OK**
  5. The item appears in the list

#### Isekai_Weapons_Adventurer
- **ID:** `Isekai_Weapons_Adventurer`
- **Add:** `SteelSword` (0001398C)

#### Isekai_Weapons_Hero
- **ID:** `Isekai_Weapons_Hero`
- **Add:** `DaedricSword` (000139B8)

#### Isekai_Armor_Humble
- **ID:** `Isekai_Armor_Humble`
- **Add:** `IronArmor` (00012E4D)

#### Isekai_Armor_Adventurer
- **ID:** `Isekai_Armor_Adventurer`
- **Add:** `SteelArmor` (00013958)

#### Isekai_Armor_Hero
- **ID:** `Isekai_Armor_Hero`
- **Add:** `DaedricArmor` (0001396A)

#### Isekai_Potions_Health
- **ID:** `Isekai_Potions_Health`
- **Add:** `PotionOfHealth` (0003EADE)
- **Add:** `PotionOfUltimateHealth` (00039BE5)

#### Isekai_Potions_Magicka
- **ID:** `Isekai_Potions_Magicka`
- **Add:** `PotionOfMagicka` (0003EADA)
- **Add:** `PotionOfUltimateMagicka` (00039BE7)

#### Isekai_Potions_Stamina
- **ID:** `Isekai_Potions_Stamina`
- **Add:** `PotionOfUltimateStamina` (00039BE6)

#### Isekai_Gems_Rare
- **ID:** `Isekai_Gems_Rare`
- **Add:** `FlawlessDiamond` (0006851E)
- **Add:** `FlawlessRuby` (0006851F)
- **Add:** `FlawlessSapphire` (00068520)

---

## 🌟 Step 7: Create perks

Perks are passive bonuses for the player.

### 7.1 Create a perk

1. Object Window → open "Character"
2. Choose "Perk"
3. **Right-click** → **New**

### 7.2 Perks for Isekai Hero

Create these 13 perks:

#### Reincarnated Soul branch

**Isekai_PastLifeMemories**
- **ID:** `Isekai_PastLifeMemories`
- **Name:** `Past Life Memories`
- **Description:** `Memories from your past life accelerate learning. You gain experience 10% faster.`

**Isekai_QuickLearner**
- **ID:** `Isekai_QuickLearner`
- **Name:** `Quick Learner`
- **Description:** `Your soul adapts quickly to new skills. Skills increase 20% faster.`

**Isekai_Prodigy**
- **ID:** `Isekai_Prodigy`
- **Name:** `Prodigy`
- **Description:** `You are a prodigy among mortals. +50% XP gain, skills level 20% faster.`

**Isekai_Transcendent**
- **ID:** `Isekai_Transcendent`
- **Name:** `Transcendent Being`
- **Description:** `You have transcended mortal limits. +100% XP gain, can make skills legendary.`

#### Dimensional Knowledge branch

**Isekai_OtherworldlyInsight**
- **ID:** `Isekai_OtherworldlyInsight`
- **Name:** `Otherworldly Insight`
- **Description:** `Knowledge from another world enhances your magic. +20 Magicka.`

**Isekai_ArcaneUnderstanding**
- **ID:** `Isekai_ArcaneUnderstanding`
- **Name:** `Arcane Understanding`
- **Description:** `Arcane secrets from your past life. +50 Magicka, spells last 20% longer.`

**Isekai_DimensionalStorage**
- **ID:** `Isekai_DimensionalStorage`
- **Name:** `Dimensional Storage`
- **Description:** `Access to a pocket dimension. +100 carry weight.`

**Isekai_RealityManipulation**
- **ID:** `Isekai_RealityManipulation`
- **Name:** `Reality Manipulation`
- **Description:** `You can bend reality itself. +150 Magicka, spells cost 25% less.`

#### System Protection branch

**Isekai_SystemShield**
- **ID:** `Isekai_SystemShield`
- **Name:** `System Shield`
- **Description:** `The System protects its chosen. +20 Health, 5% damage resistance.`

**Isekai_PainSuppression**
- **ID:** `Isekai_PainSuppression`
- **Name:** `Pain Suppression`
- **Description:** `Pain is just data to be ignored. +50 Health, 10% damage resistance.`

**Isekai_Regeneration**
- **ID:** `Isekai_Regeneration`
- **Name:** `Regeneration`
- **Description:** `Your body regenerates at supernatural speeds. Health regenerates 100% faster.`

**Isekai_ImmortalVessel**
- **ID:** `Isekai_ImmortalVessel`
- **Name:** `Immortal Vessel`
- **Description:** `Your vessel is nearly immortal. +200 Health, 25% damage resistance.`

#### Ascended exclusive

**Isekai_SystemAdmin**
- **ID:** `Isekai_SystemAdmin`
- **Name:** `System Administrator`
- **Description:** `You are the Administrator of this world. Ultimate power unlocked.`

---

## 🎮 Step 8: Additional quests (v2.0)

For progression, perks and the MCM we need separate quests.

### 8.1 IsekaiProgression quest

1. Object Window → Quest → **New**
2. Settings:
   - **ID:** `IsekaiProgression`
   - **Name:** `Isekai Progression System`
   - ☑ **Start Game Enabled**
   - **Priority:** `45`
3. **Scripts** tab → **Add** → `IsekaiProgressionScript`
4. **OK**

### 8.2 IsekaiPerks quest

1. Quest → **New**
2. Settings:
   - **ID:** `IsekaiPerks`
   - **Name:** `Isekai Perk System`
   - ☑ **Start Game Enabled**
   - **Priority:** `45`
3. **Scripts** tab → **Add** → `IsekaiPerkDefinitions`
4. **OK**

### 8.3 IsekaiMCM quest

1. Quest → **New**
2. Settings:
   - **ID:** `IsekaiMCM`
   - **Name:** `Isekai MCM Menu`
   - ☑ **Start Game Enabled**
   - **Priority:** `40`
3. **Scripts** tab → **Add** → `IsekaiMCMScript`
4. Set properties (Auto-Fill or manually):
   `MainQuest`, `DialogScript`, `PowerScript`, `Progression`, `QuestTracker`
   → each to the quest with the corresponding script
5. **OK**

### 8.4 IsekaiQuestTracker quest

The main-quest reward tracker → see the dedicated section
**"🏆 Setting up the Main Quest Tracker"** further below (create the quest, fill the
`MainQuests` array). Then point the `QuestTracker` property in the MCM quest at it.

---

## 🔧 Step 9: Compile scripts

This step is often tricky — here in detail:

### 9.1 Preparation

1. Make sure all `.psc` files are in the folder:
   ```
   Skyrim Special Edition\Data\Source\Scripts\
   ├── IsekaiIntroQuest.psc
   ├── IsekaiDialogScript.psc
   ├── IsekaiPowerScript.psc
   ├── IsekaiProgressionScript.psc
   ├── IsekaiPerkDefinitions.psc
   ├── IsekaiQuestTracker.psc
   └── IsekaiMCMScript.psc
   ```
   > Note: Skyrim SE uses `Data\Source\Scripts` (older guides say `Data\Scripts\Source`).
   > Use the directory where your `Game.psc` lives.

2. **IMPORTANT:** SKSE, **SkyUI** (for the MCM) and **UIExtensions** (for the menus) must
   be installed — their script sources must be findable to compile. Alternatively use the
   repo script `compile.ps1` (see `Scripts/SourceDeps/`).

### 9.2 Compilation

1. In the Creation Kit: **Gameplay → Compile Scripts** (or press F6)
2. The "Compile Scripts" window opens
3. **IMPORTANT:** tick **"All"** (top left)
4. Or: select only the Isekai scripts
5. Click **Compile**

### 9.3 Fixing errors

If errors occur:

**"Script not found"**
- The script file is missing in the `Scripts\Source` folder
- The filename doesn't match (case-sensitive!)

**"Unknown type"**
- SKSE is not installed
- Wrong Skyrim version (Oldrim vs SE)

**"Missing master"**
- Skyrim.esm not loaded
- Other dependencies missing

**"Failed to compile" without details**
- CK bug! Try:
  1. Close the CK
  2. Delete `Data\Scripts\Source\temp`
  3. Restart the CK
  4. Try again

---

## 💾 Step 10: Save & test

### 10.1 Save

1. **File → Save**
2. Click **YES** to overwrite
3. Wait until it's saved

### 10.2 Copy files

Copy these files into your mod folder:

```
From: Skyrim Special Edition\Data\
├── IsekaiHero.esp
└── Scripts\
    ├── IsekaiIntroQuest.pex
    ├── IsekaiDialogScript.pex
    ├── IsekaiPowerScript.pex
    ├── IsekaiProgressionScript.pex
    ├── IsekaiPerkDefinitions.pex
    ├── IsekaiQuestTracker.pex
    └── IsekaiMCMScript.pex
```
> Note: the MCM runs via the `IsekaiMCMScript` (SKI_ConfigBase) — no `IsekaiMCMConfig.json`
> is needed anymore (it was removed).

### 10.3 In-game test

1. Start Skyrim SE via SKSE
2. Load an existing save or start a new one
3. After character creation the System should start:
   - You see: "[SYSTEM] Initializing..."
   - Then: "[SYSTEM] Detecting soul signature..."
   - Then the dialogs

---

## 🐛 Troubleshooting

### "Script not found" in-game

**Cause:** the script wasn't compiled or is in the wrong place

**Fix:**
1. Check: does `Scripts\IsekaiIntroQuest.pex` exist?
2. If no: compile again (`compile.ps1`)
3. If yes: make sure the .pex files are in the Data folder

### "Cannot open store for class ..." when attaching a script

**Example:** `SCRIPTS: Cannot open store for class "uilistmenu", missing file?`

**Cause:** when attaching, the CK loads the complete reference graph of the script.
`IsekaiDialogScript` references `UIListMenu` (UIExtensions), `IsekaiMCMScript` inherits from
`SKI_ConfigBase` (SkyUI). Their **compiled `.pex`** must be findable to the CK — either in
`Data\Scripts` (loose) or in a loaded BSA. Since we do **not** load SkyUI/UIExtensions as
masters, the `.pex` must be present loose.

**Fix (one-time):** extract the dependencies' `.pex` into `Data\Scripts`:
- **UIExtensions:** from `Data\UIExtensions.bsa` → `uiextensions.pex`, `uilistmenu.pex`,
  `uimenubase.pex` (that's enough for our menus)
- **SkyUI:** from `SkyUI_SE.bsa` → all `SKI_*.pex`

> ⚠️ Do **NOT** copy `cosmeticmenu.pex` / `uicosmeticmenu.pex` / `uidyemenu.pex` — they
> inherit from **RaceMenu** (a separate mod) and otherwise trigger the warning
> `Cannot open store for class "RaceMenu"`. We don't use them.

**BSArch** or **BSA Browser** work for extracting. Then **restart the CK** (the script store
is only read at startup). These `.pex` never change — so this is only needed once.

### Quest doesn't start

**Cause:** "Start Game Enabled" not set or wrong priority

**Fix:**
1. Open the quest in the CK
2. Check: ☑ **Start Game Enabled** is ticked
3. Check: **Priority** is at least 50
4. Save again

### Message boxes don't appear

**Cause:** wrong button indices or message forms not linked

**Fix:**
1. Check: the message-form IDs match the script properties
2. Check: buttons are in the right order (0, 1, 2...)
3. In the script: `IsekaiMsg_WorldSelect.Show()` returns the button index

### CTD (crash to desktop)

**Causes & fixes:**

| Symptom | Cause | Fix |
|---------|---------|--------|
| On start | ESL not correct | Compact the form IDs |
| On dialog | Wrong message form | Check button indices |
| On equipment | Wrong form ID | Check the FormLists |
| Random | Script loop | Increase the OnUpdate interval |

### CK crashes

**Common CK bugs:**

1. **"Out of memory"**
   - The CK is 32-bit and has a memory limit
   - Fix: save often, restart the CK

2. **"Failed to open file"**
   - The CK is not running as administrator
   - Fix: right-click → Run as administrator

3. **Scripts don't compile**
   - Delete the `Data\Scripts\Source\temp` folder
   - Restart the CK

---

## 🏆 Setting up the Main Quest Tracker (Solo-Leveling rewards)

The script [`IsekaiQuestTracker.psc`](Scripts/Source/IsekaiQuestTracker.psc) rewards main-quest
completions with perk points + titles. Wire it up like this:

### 1. Create the quest
1. **Object Window → Character → Quest → right-click → New**
2. **ID:** `IsekaiQuestTracker`
3. **Quest Data** tab: tick **Start Game Enabled** and **Run Once**
4. **Scripts** tab **→ Add → `IsekaiQuestTracker`**

### 2. Fill the MainQuests array (IMPORTANT: exact order!)
In the script properties window select `MainQuests` → **Edit Value** → size **12**, then
assign each index the matching vanilla quest:

| Index | Quest editor ID | Quest name | Reward |
|------:|-----------------|------------|-----------|
| 0 | `MQ101` | Unbound | +10 perks, "Survivor" |
| 1 | `MQ102` | Before the Storm | +5 perks |
| 2 | `MQ103` | Bleak Falls Barrow | +15 perks, "Tomb Raider" |
| 3 | `MQ104` | Dragon Rising | +50 perks, "Dragon Slayer" |
| 4 | `MQ105` | The Way of the Voice | +25 perks, "Voice Wielder" |
| 5 | `MQ106` | The Horn of Jurgen Windcaller | +30 perks |
| 6 | `MQ201` | A Blade in the Dark | +20 perks |
| 7 | `MQ202` | Diplomatic Immunity | +40 perks, "Spy" |
| 8 | `MQ203` | A Cornered Rat | +25 perks |
| 9 | `MQ204` | Alduin's Wall | +30 perks, "Time Reader" |
| 10 | `MQ205` | The Fallen | +50 perks, "Dragon Tamer" |
| 11 | `MQ206` | Dragonslayer | +500 perks, "World Savior" |

> ⚠️ The order must be right — the rewards are hard-coupled to the index in the script.
> Find the quests in the Object Window by ID (filter `MQ1`/`MQ2`).

### 3. Options (properties)
- `EnableQuestRewards` = `True` (default)
- `RetroactiveRewards` = `False` → already-completed quests at install are **not** rewarded
  retroactively (recommended for existing saves)

### 4. Link it in the MCM quest
In the quest with `IsekaiMCMScript`, set the new `QuestTracker` property to
`IsekaiQuestTracker` (for the status display + toggle in the MCM).

---

## 📤 Distribution

For release you need:

### Required:
- `IsekaiHero.esp` (ESL-flagged)
- `Scripts\*.pex` (compiled scripts)
- SKSE64, SkyUI, UIExtensions (as mod dependencies in the mod description text)

### Optional (recommended):
- `Scripts\Source\*.psc` (source code)
- `README.md`
- `CREATION_KIT_GUIDE.md`

### Don't ship:
- `*.bsa` (if you have no assets)
- `*.esp.bak` or `*.esp.save`
- The `temp` folder

---

**If you run into problems:** open an issue on the GitHub repository with:
1. CK version
2. Skyrim version
3. The exact error message
4. What you were trying to do

_Good luck, modder!_ 🐉
