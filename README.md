# Isekai Hero for Skyrim Special Edition

[![Skyrim SE](https://img.shields.io/badge/Skyrim%20SE-Compatible-brightgreen)]()
[![ESL](https://img.shields.io/badge/ESL%20Flag-Compatible-blue)]()

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

### 🎮 System Interface
- Dramatic boot sequence with soul detection
- ASCII-style System headers (`╔═══ 「 SYSTEM 」 ═══╗`)
- World-specific flavor text and bonuses
- Status Window to check your System stats
- Ascended mode visual and audio effects

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

### 4. Begin Your Adventure
Your choices are applied and you begin as a true Isekai hero!

---

## 🔧 Compatibility

| Mod | Status | Notes |
|-----|--------|-------|
| Vanilla Start | ✅ Compatible | Works out of the box |
| Alternate Start | ✅ Compatible | Auto-detected, triggers after start scenario |
| Live Another Life | ✅ Compatible | Auto-detected, triggers after start scenario |
| SkyUI | ✅ Compatible | Required for MCM |
| UIExtensions | ✅ Compatible | Optional, enhances menus |

- ✅ **ESL flagged** - Doesn't count towards 255 plugin limit
- ✅ **Minimal script load** - No constant OnUpdate loops

---

## 📖 Creation Kit Setup

To complete the mod, you need to create Message Forms in the Creation Kit:

See **[CREATION_KIT_GUIDE.md](CREATION_KIT_GUIDE.md)** for step-by-step instructions.

### Required Message Forms:
- `IsekaiMsg_WorldSelect` - Dimensional origin
- `IsekaiMsg_PowerChoice` - Power level selection
- `IsekaiMsg_SkillFocus` - Skill distribution
- `IsekaiMsg_EquipmentChoice` - Starting gear
- `IsekaiMsg_SystemComplete` - Completion summary

---

## 🛠️ Troubleshooting

### Quest doesn't start
- Ensure "Start Game Enabled" is checked in the Quest properties
- Check that Message Forms are properly created
- Wait 10-30 seconds after spawning

### Scripts not found
- Ensure `.pex` files are in the `Scripts` folder
- Recompile scripts in Creation Kit if needed

### Compatibility issues
- The mod auto-detects start mods and waits appropriately
- If issues persist, use the MCM menu to manually trigger

---

## 🎨 Future Plans

- [ ] Custom visual effects for Ascended mode
- [ ] More origin worlds
- [ ] Perk tree integration
- [ ] System achievements
- [ ] Save/load System settings

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