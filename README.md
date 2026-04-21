# Isekai Hero for Skyrim Special Edition

[![Skyrim SE](https://img.shields.io/badge/Skyrim%20SE-Compatible-brightgreen)]()
[![ESL](https://img.shields.io/badge/ESL%20Flag-Compatible-blue)]()
[![Version](https://img.shields.io/badge/Version-2.0-orange)]()

*"Welcome, Reincarnated One. The System has recognized your soul."*

Start your Skyrim adventure as a hero from another world! Inspired by Isekai anime, this mod transforms your character creation into a full System Interface experience with dramatic flair.

> **📝 Development:** See [WORKFLOW.md](WORKFLOW.md) for contribution guidelines.

---

## ✨ Features

### 🌍 Dimensional Origin Selection
Choose your previous world:
- **Earth** - A world of technology without magic
- **Japan** - *"Truck-kun sent you on your next adventure..."*
- **Korea** - Dungeons and hunters
- **Fantasy World** - Swords and sorcery
- **Sci-Fi Future** - Advanced technology and space travel
- **Apocalyptic** - Survival in a wasteland

Each origin grants unique flavor text and minor bonuses!

### ⚡ Three Power Levels

| Level | Stats | Description |
|-------|-------|-------------|
| **NORMAL** | Vanilla | Start as a native of Nirn |
| **HERO** | Level 1, Skills 100, 50 perks | Retain heroic memories |
| **ASCENDED** | Level 255, Max Skills, 500 perks | Transcend mortal limits |

### 🎯 Skill Focus Distribution
- **Balanced** - Equal mastery in all arts
- **Warrior** - One/Two-Handed, Block, Heavy Armor, Smithing
- **Mage** - All magic schools
- **Thief** - Sneak, Lockpicking, Pickpocket, Light Armor
- **Custom** - Configure later via MCM

### 🎒 Equipment Tiers
- **Humble** - Iron gear, basic supplies, 100 gold
- **Adventurer** - Steel gear, potions, 500 gold
- **Hero** - Daedric items, ultimate potions, 2000 gold
- **None** - Pure skill only (hard mode)

### 💰 Wealth Levels
- **Modest** - 1,000 gold — A humble merchant's savings
- **Wealthy** - 10,000 gold — A successful adventurer's hoard
- **Noble** - 50,000 gold — A minor lord's fortune
- **Merchant Prince** - 100,000 gold + rare gems — Wealth beyond measure!

### 🎮 System Interface
- Dramatic boot sequence with soul detection
- ASCII-style System headers (`╔═══ 「 SYSTEM 」 ═══╗`)
- World-specific flavor text and bonuses
- Status Window to check your System stats
- Ascended mode visual and audio effects

### 🏆 NEW in v2.0: Progression System
Instead of just getting everything at once, earn perks through milestones:
- **First Steps** - Begin your journey (+5 perks)
- **Voice of the Dragonborn** - Learn your first Word (+10 perks)
- **Dragon Slayer** - Defeat your first Dragon (+15 perks)
- **Rising Power** - Reach Level 25 (+10 perks)
- **Dungeon Delver** - Clear 5 dungeons (+10 perks)
- **Adept** - Reach Level 50 (+20 perks)
- **Dragonborn** - Learn 10 Words (+25 perks)
- **Master** - Reach Level 100 (+50 perks)
- **Legend** - Defeat 10 Dragons (+100 perks)

### 🌟 NEW in v2.0: Isekai Perk Tree
Unique perks that fit the Isekai theme:

**Reincarnated Soul Branch:**
- Past Life Memories (+10% XP)
- Quick Learner (+20% skill speed)
- Prodigy (+50% XP, skills faster)
- Transcendent Being (+100% XP, legendary skills)

**Dimensional Knowledge Branch:**
- Otherworldly Insight (+20 Magicka)
- Arcane Understanding (+50 Magicka, +20% duration)
- Dimensional Storage (+100 carry weight)
- Reality Manipulation (+150 Magicka, -25% cost)

**System Protection Branch:**
- System Shield (+20 Health, 5% resistance)
- Pain Suppression (+50 Health, 10% resistance)
- Regeneration (+100% health regen)
- Immortal Vessel (+200 Health, 25% resistance)

### 🔧 NEW in v2.0: MCM Menu
Full Mod Configuration Menu support:
- View System status and current bonuses
- Re-Spec your character anytime
- Toggle notifications
- Force-trigger the System
- Debug mode

---

## 📋 Requirements

- **Skyrim Special Edition** (1.5.x or 1.6.x)
- **SKSE** (optional, for enhanced features)
- **[Optional]** SkyUI for MCM menu
- **[Optional]** UIExtensions for enhanced menus

---

## 🚀 Installation

### Mod Manager (Recommended)
1. Install with Vortex/Mod Organizer 2
2. Enable `IsekaiHero.esp`
3. **Important:** Create the Message Forms in Creation Kit (see guide below)

### Manual
1. Download and extract
2. Copy `IsekaiHero.esp` and `Scripts/` folder to your Data directory
3. Enable in your mod manager

---

## 🎮 How It Works

### 1. Character Creation
Create your character as normal.

### 2. The Awakening
After you spawn in the world (compatible with **all** start mods):
```
[SYSTEM] Detecting soul signature...
[SYSTEM] Analyzing dimensional origin...
[SYSTEM] Welcome to NIRN, Reincarnated One.
```

### 3. System Interface
The System will guide you through:
1. **Dimensional Origin** - Where did you come from?
2. **Power Level** - How much strength do you retain?
3. **Skill Focus** - What were you in your past life?
4. **Equipment** - What gear manifests from the void?
5. **Wealth** - What fortune accompanies you?

### 4. Progression
As you play, complete milestones to earn additional perks!

### 5. Begin Your Adventure
Your choices are applied and you begin as a true Isekai hero!

---

## 🔧 Compatibility

| Mod | Status | Notes |
|-----|--------|-------|
| Vanilla Start | ✅ Compatible | Works out of the box |
| Alternate Start | ✅ Compatible | Auto-detected |
| Live Another Life | ✅ Compatible | Auto-detected |
| **Skyrim Unbound** | ✅ Compatible | **N.Y.A Modlist support! Auto-detected** |
| SkyUI | ✅ Compatible | Required for MCM |
| UIExtensions | ✅ Compatible | Optional, enhances menus |

- ✅ **ESL flagged** - Doesn't count towards 255 plugin limit
- ✅ **Minimal script load** - No constant OnUpdate loops
- ✅ **Heavy modlist safe** - Form IDs resolved dynamically

---

## 📖 Creation Kit Setup

To complete the mod, you need to create Message Forms in the Creation Kit:

See **[CREATION_KIT_GUIDE.md](CREATION_KIT_GUIDE.md)** for step-by-step instructions.

### Required Message Forms:
- `IsekaiMsg_WorldSelect` - Dimensional origin
- `IsekaiMsg_PowerChoice` - Power level selection
- `IsekaiMsg_SkillFocus` - Skill distribution
- `IsekaiMsg_EquipmentChoice` - Starting gear
- `IsekaiMsg_WealthChoice` - Starting wealth
- `IsekaiMsg_SystemComplete` - Completion summary

### Required FormLists (for v2.0):
- `Isekai_Weapons_Humble`
- `Isekai_Armor_Humble`
- `Isekai_Weapons_Adventurer`
- `Isekai_Armor_Adventurer`
- `Isekai_Weapons_Hero`
- `Isekai_Armor_Hero`
- `Isekai_Potions_Health`
- `Isekai_Potions_Magicka`
- `Isekai_Potions_Stamina`
- `Isekai_Gems_Rare`

### Required Perks (for v2.0):
- `Isekai_PastLifeMemories`
- `Isekai_QuickLearner`
- `Isekai_Prodigy`
- `Isekai_Transcendent`
- `Isekai_OtherworldlyInsight`
- `Isekai_ArcaneUnderstanding`
- `Isekai_DimensionalStorage`
- `Isekai_RealityManipulation`
- `Isekai_SystemShield`
- `Isekai_PainSuppression`
- `Isekai_Regeneration`
- `Isekai_ImmortalVessel`
- `Isekai_SystemAdmin` (Ascended exclusive)

---

## 🛠️ Troubleshooting

### Quest doesn't start
- ✅ "Start Game Enabled" must be checked in the Quest properties
- ✅ Check that Message Forms are properly created
- ✅ Wait 10-30 seconds after spawning

### Scripts not found
- ✅ Ensure `.pex` files are in the `Scripts` folder
- ✅ Recompile scripts in Creation Kit if needed

### Compatibility issues
- ✅ The mod auto-detects start mods and waits appropriately
- ✅ If issues persist, use the MCM menu to manually trigger

---

## 📝 Changelog

### v2.0 - Major Update
- ✅ **Heavy Modlist Compatibility** - Dynamic form resolution
- ✅ **Progression System** - Milestones with perk rewards
- ✅ **Isekai Perk Tree** - 13 unique perks across 3 branches
- ✅ **MCM Menu** - Full SkyUI configuration
- ✅ **Re-Spec Function** - Reset character anytime
- ✅ **Better Form Safety** - No more hardcoded IDs

---

## 🎨 Future Plans

- [ ] Custom visual effects for Ascended mode
- [ ] More origin worlds
- [ ] Dimensional Storage (access inventory anywhere)
- [ ] System achievements
- [ ] Save/load System settings
- [ ] Custom spell effects

---

## 📝 Credits

Created for the Isekai experience in Skyrim.

Special thanks to:
- SkyUI team for the MCM framework
- UIExtensions team for enhanced UI
- The Skyrim Scripting community

---

## 📄 License

MIT License - See LICENSE.md
