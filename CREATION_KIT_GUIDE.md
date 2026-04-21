# Isekai Hero - Creation Kit Guide

This guide walks you through setting up the Isekai Hero mod in the Creation Kit.

## Prerequisites

- Skyrim Special Edition Creation Kit (Steam: Tools > Creation Kit)
- Basic familiarity with the Creation Kit interface

---

## Step 1: Create the Plugin

1. Open Creation Kit
2. **File > Data...**
3. Check **Skyrim.esm** (and Update.esm if you have it)
4. Click **OK** and wait for loading
5. **File > Save**
6. Name: `IsekaiHero.esp`
7. Click **Save**

---

## Step 2: ESL Flag (Optional but Recommended)

To make the mod ESL-flagged (doesn't count towards 255 plugin limit):

1. **File > Compact Active File Form IDs**
2. **File > Convert Active File to Light Master**
3. **File > Save**

---

## Step 3: Create the Main Quest

1. In the Object Window, filter for `Quest`
2. Right-click in the list > **New**
3. Set the following:
   - **ID**: `IsekaiIntroQuest`
   - **Name**: Isekai Hero - System Awakening
   - **Type**: None
   - **Priority**: 50
   - ☑ **Start Game Enabled**
   - ☑ **Run Once**

4. Go to the **Scripts** tab
5. Click **Add**
6. Select `IsekaiIntroQuest` (you may need to compile scripts first)
7. Click **Properties**
8. Add the following properties:
   - `DialogScript` → ObjectReference → `IsekaiDialogScript`
   - `PowerScript` → ObjectReference → `IsekaiPowerScript`
   - `ProgressionScript` → ObjectReference → `IsekaiProgressionScript`
   - `PerkDefs` → ObjectReference → `IsekaiPerkDefinitions`

9. Click **OK**

---

## Step 4: Create Quest Stages

In the Quest window, go to **Quest Stages** tab:

| Stage | Log Entry |
|-------|-----------|
| 10 | (No log entry - waiting for spawn) |
| 20 | [SYSTEM] Boot sequence initiated |
| 25 | [SYSTEM] Dimensional origin confirmed |
| 30 | [SYSTEM] Power level selected |
| 40 | [SYSTEM] Skill focus assigned |
| 50 | [SYSTEM] Equipment summoned |
| 55 | [SYSTEM] Wealth allocated |
| 60 | [SYSTEM] Applying changes... |
| 100 | [SYSTEM] Reincarnation complete |

---

## Step 5: Create Message Forms

These are the dialog boxes that appear during the Isekai sequence.

### 1. IsekaiMsg_WorldSelect
1. Object Window > filter `Message`
2. Right-click > **New**
3. **ID**: `IsekaiMsg_WorldSelect`
4. **Name**: SYSTEM: Dimensional Origin
5. **Message**: 
   ```
   From which world do you hail, Reincarnated One?
   
   [1] EARTH - Modern world, no magic
   [2] JAPAN - Land of truck-kun incidents
   [3] KOREA - Dungeons and hunters
   [4] FANTASY WORLD - Swords and sorcery
   [5] SCI-FI FUTURE - Advanced technology
   [6] APOCALYPTIC - Survival and mutations
   ```
6. **Buttons** (one per line):
   ```
   Earth
   Japan
   Korea
   Fantasy World
   Sci-Fi Future
   Apocalyptic
   ```

### 2. IsekaiMsg_PowerChoice
- **ID**: `IsekaiMsg_PowerChoice`
- **Name**: SYSTEM: Status Allocation
- **Message**:
  ```
  Choose your reincarnation blessing:
  
  [1] NORMAL - No memories, native start
  [2] HERO - Level 1 | Skills 100 | 50 Perks
  [3] ASCENDED - Level 255 | Max Skills | 500 Perks
  [4] DECLINE - Refuse the blessing
  ```
- **Buttons**:
  ```
  NORMAL
  HERO
  ASCENDED
  DECLINE
  ```

### 3. IsekaiMsg_SkillFocus
- **ID**: `IsekaiMsg_SkillFocus`
- **Name**: SYSTEM: Skill Allocation
- **Message**:
  ```
  Select your past life's expertise:
  
  [1] BALANCED - Equal mastery
  [2] WARRIOR - Combat mastery
  [3] MAGE - Arcane mastery
  [4] THIEF - Shadow mastery
  [5] CUSTOM - Configure later
  [6] BACK
  ```
- **Buttons**:
  ```
  BALANCED
  WARRIOR
  MAGE
  THIEF
  CUSTOM
  BACK
  ```

### 4. IsekaiMsg_EquipmentChoice
- **ID**: `IsekaiMsg_EquipmentChoice`
- **Name**: SYSTEM: Equipment Summoning
- **Message**:
  ```
  Choose your starting gear:
  
  [1] HUMBLE - Iron gear, basic supplies
  [2] ADVENTURER - Steel gear, potions
  [3] HERO - Daedric items, ultimate potions
  [4] NONE - Pure skill only
  [5] BACK
  ```
- **Buttons**:
  ```
  HUMBLE
  ADVENTURER
  HERO
  NONE
  BACK
  ```

### 5. IsekaiMsg_WealthChoice
- **ID**: `IsekaiMsg_WealthChoice`
- **Name**: SYSTEM: Wealth Allocation
- **Message**:
  ```
  Choose your starting fortune:
  
  [1] MODEST - 1,000 gold
  [2] WEALTHY - 10,000 gold
  [3] NOBLE - 50,000 gold
  [4] MERCHANT PRINCE - 100,000 gold + gems
  [5] BACK
  ```
- **Buttons**:
  ```
  MODEST
  WEALTHY
  NOBLE
  MERCHANT PRINCE
  BACK
  ```

### 6. IsekaiMsg_SystemComplete
- **ID**: `IsekaiMsg_SystemComplete`
- **Name**: SYSTEM: Reincarnation Complete
- **Message**:
  ```
  ╔══════════════════════════════════════╗
  ║     REINCARNATION COMPLETE!          ║
  ╠══════════════════════════════════════╣
  
  Welcome to Nirn, Reincarnated One.
  Your journey begins now.
  
  May your legend be written in the stars.
  
  ╚══════════════════════════════════════╝
  ```
- **Buttons**: `Continue`

---

## Step 6: Create FormLists (v2.0)

These allow modders to customize equipment without editing scripts.

1. Object Window > filter `FormList`
2. Right-click > **New**
3. Create the following FormLists:

### Weapon FormLists
- **Isekai_Weapons_Humble** - Add: IronSword (00012E4E)
- **Isekai_Weapons_Adventurer** - Add: SteelSword (0001398C)
- **Isekai_Weapons_Hero** - Add: DaedricSword (000139B8)

### Armor FormLists
- **Isekai_Armor_Humble** - Add: IronArmor (00012E4D)
- **Isekai_Armor_Adventurer** - Add: SteelArmor (00013958)
- **Isekai_Armor_Hero** - Add: DaedricArmor (0001396A)

### Potion FormLists
- **Isekai_Potions_Health** - Add: PotionOfHealth (0003EADE), PotionOfUltimateHealth (00039BE5)
- **Isekai_Potions_Magicka** - Add: PotionOfMagicka (0003EADA), PotionOfUltimateMagicka (00039BE7)
- **Isekai_Potions_Stamina** - Add: PotionOfUltimateStamina (00039BE6)

### Misc FormLists
- **Isekai_Gems_Rare** - Add: FlawlessDiamond (0006851E), FlawlessRuby (0006851F), FlawlessSapphire (00068520)

---

## Step 7: Create Perks (v2.0)

1. Object Window > filter `Perk`
2. Right-click > **New**
3. Create the following perks:

### Reincarnated Soul Branch
| Perk ID | Name | Description |
|---------|------|-------------|
| Isekai_PastLifeMemories | Past Life Memories | +10% XP gain |
| Isekai_QuickLearner | Quick Learner | +20% skill speed |
| Isekai_Prodigy | Prodigy | +50% XP, skills faster |
| Isekai_Transcendent | Transcendent Being | +100% XP, legendary |

### Dimensional Knowledge Branch
| Perk ID | Name | Description |
|---------|------|-------------|
| Isekai_OtherworldlyInsight | Otherworldly Insight | +20 Magicka |
| Isekai_ArcaneUnderstanding | Arcane Understanding | +50 Magicka, +20% duration |
| Isekai_DimensionalStorage | Dimensional Storage | +100 carry weight |
| Isekai_RealityManipulation | Reality Manipulation | +150 Magicka, -25% cost |

### System Protection Branch
| Perk ID | Name | Description |
|---------|------|-------------|
| Isekai_SystemShield | System Shield | +20 Health, 5% resistance |
| Isekai_PainSuppression | Pain Suppression | +50 Health, 10% resistance |
| Isekai_Regeneration | Regeneration | +100% health regen |
| Isekai_ImmortalVessel | Immortal Vessel | +200 Health, 25% resistance |

### Ascended Exclusive
| Perk ID | Name | Description |
|---------|------|-------------|
| Isekai_SystemAdmin | System Administrator | Ultimate power (requires Level 100) |

For each perk:
1. Set the **ID** and **Name**
2. Add the description in the **Description** field
3. Set **Level** requirements as needed
4. Click **OK**

---

## Step 8: Create Additional Quests (v2.0)

### IsekaiProgression Quest
1. Create new Quest: `IsekaiProgression`
2. ☑ **Start Game Enabled**
3. Add Script: `IsekaiProgressionScript`
4. Set priority: 45

### IsekaiPerks Quest
1. Create new Quest: `IsekaiPerks`
2. ☑ **Start Game Enabled**
3. Add Script: `IsekaiPerkDefinitions`
4. Set priority: 45

### IsekaiMCM Quest (for SkyUI)
1. Create new Quest: `IsekaiMCM`
2. ☑ **Start Game Enabled**
3. Add Script: `IsekaiMCMScript`
4. Set priority: 40

---

## Step 9: Compile Scripts

1. **Gameplay > Compile Scripts** (or press F6)
2. Select all Isekai scripts:
   - IsekaiIntroQuest.psc
   - IsekaiDialogScript.psc
   - IsekaiPowerScript.psc
   - IsekaiProgressionScript.psc
   - IsekaiPerkDefinitions.psc
   - IsekaiMCMScript.psc
3. Click **Compile**
4. Check for errors in the message window

---

## Step 10: Save and Test

1. **File > Save**
2. Copy `IsekaiHero.esp` to your Skyrim Data folder
3. Copy compiled `.pex` files from `Data/Scripts/` to your mod folder
4. Launch Skyrim and test!

---

## Troubleshooting

### "Script not found" error
- Make sure scripts are compiled
- Check that script names match exactly

### Message boxes don't appear
- Verify Message Form IDs match script properties
- Check button indices (0, 1, 2...)

### Quest doesn't start
- Ensure "Start Game Enabled" is checked
- Check for script compilation errors

---

## Distribution

When releasing the mod, include:
- `IsekaiHero.esp`
- `Scripts/Isekai*.pex` (compiled scripts)
- `Scripts/Source/Isekai*.psc` (source scripts)
- `Interface/IsekaiMCMConfig.json` (MCM config)
- `README.md`
- This guide
