> ⚠️ **Legacy / archived.** This documents the old Papyrus implementation of the mod
> (tag `papyrus-v1.0`), which has been superseded by the native SKSE C++ version. Kept
> for reference only.

# Isekai Hero Skyrim Mod — project plan

## 🎯 Goal
Build a complete isekai mod for Skyrim Special Edition that:
- Welcomes the player at the start as a "Reincarnated Hero"
- Offers System dialogs with world choice, power level, skills, equipment, wealth
- **Is N.Y.A Modlist compatible** (Skyrim Unbound as the start mod)
- Is compatible with all common start mods (Skyrim Unbound, Alternate Start, LAL)

---

## 📁 Project structure

```
isekai-hero-skyrim/
├── PROJEKT.md              # This roadmap
├── README.md               # Project overview
├── WORKFLOW.md             # Git workflow
├── CREATION_KIT_GUIDE.md   # CK guide
├── FEATURES.md             # Planned features
├── compile.ps1             # Papyrus compile check (build script)
│
├── Scripts/
│   └── Source/
│       ├── IsekaiIntroQuest.psc        # Main quest logic
│       ├── IsekaiDialogScript.psc      # System interface / menus (vanilla + UIExtensions)
│       ├── IsekaiPowerScript.psc       # Skills / equipment / wealth
│       ├── IsekaiProgressionScript.psc # Milestones / perk rewards
│       ├── IsekaiPerkDefinitions.psc   # Isekai perks
│       ├── IsekaiQuestTracker.psc      # Main-quest rewards (Solo-Leveling style)
│       └── IsekaiMCMScript.psc         # MCM (SKI_ConfigBase / SkyUI)
│
└── Interface/              # (IsekaiMCMConfig.json removed — the MCM runs via the SKI_ConfigBase script)
```

> **Status of the scripts:** logic review + known compile errors fixed
> (`GodModeAuraActive`, `EndEvent`/`EndFunction`, `Game.AddPerkPoints`,
> tracked-stat names). **A real compiler cross-check is still pending** — that
> needs the Skyrim SE Creation Kit (see "Build toolchain" below).

### 🔧 Build toolchain (for the compile check)

`compile.ps1` compiles all `.psc` in `Scripts/Source` and reports errors before the CK is
opened. Prerequisites (to install once):

| Component | Provides | Source |
|---|---|---|
| Skyrim SE **Creation Kit** | `PapyrusCompiler.exe`, vanilla sources, `TESV_Papyrus_Flags.flg` | Steam (free) |
| **SkyUI SDK** | `SKI_ConfigBase.psc` (for MCM) | SkyUI modder resource |
| **UIExtensions** sources | `UIListMenu.psc` (for UIExt menus) | UIExtensions modder resource |

Call: `./compile.ps1` (auto-detect) or `./compile.ps1 -Only IsekaiPowerScript`.

---

## ✅ Checklist: what's still missing

### Phase 1: Creation Kit setup (how-to)

- [ ] **Open the CK** → load Skyrim.esm as master
- [ ] **Create a new plugin** → `IsekaiHero.esp`
- [ ] **ESL-flag** (optional):
  1. **File → Compact Active File Form IDs**
  2. **File → Convert Active File to Light Master**
  3. **File → Save**
- [ ] **Create the quest** → `IsekaiIntroQuest` (ID: `IsekaiIntroQuest`)
  - [ ] Start Game Enabled ✓
  - [ ] All stages (10, 20, 25, 30, 40, 50, 55, 60, 100)
  - [ ] Script: `IsekaiIntroQuest`

### Phase 1b: Skyrim Unbound detection (FOR THE N.Y.A MODLIST!)

> ⚠️ **IMPORTANT:** the N.Y.A Modlist uses Skyrim Unbound instead of Alternate Start/LAL!

In the quest `IsekaiIntroQuest` properties:
- [ ] SkyrimUnboundInstalled → `SkyrimUnbound.esp` (checks automatically)
- [ ] AlternateStartInstalled → `AlternateStart.esp`
- [ ] LALInstalled → `Alternate Start - Live Another Life.esp`

The mod automatically detects which start system is active.

### Phase 2: Message forms (DIALOGS)

> These provide the button return values. **Must be created in the CK!**

- [ ] `IsekaiMsg_SystemWelcome` — "Continue" → 1 button
- [ ] `IsekaiMsg_WorldSelect` — 6 buttons (0-5)
- [ ] `IsekaiMsg_PowerChoice` — 4 buttons (0-3)
- [ ] `IsekaiMsg_SkillFocus` — 6 buttons (0-5)
- [ ] `IsekaiMsg_EquipmentChoice` — 5 buttons (0-4)
- [ ] `IsekaiMsg_WealthChoice` — 5 buttons (0-4)
- [ ] `IsekaiMsg_SystemComplete` — 1 button

### Phase 3: Compile scripts

- [ ] `IsekaiIntroQuest.psc` → `IsekaiIntroQuest.pex`
- [ ] `IsekaiDialogScript.psc` → `IsekaiDialogScript.pex`
- [ ] `IsekaiPowerScript.psc` → `IsekaiPowerScript.pex`

### Phase 4: Link properties

In the quest `IsekaiIntroQuest`:
- [ ] DialogScript → `IsekaiDialogScript`
- [ ] PowerScript → `IsekaiPowerScript`

In `IsekaiDialogScript`:
- [ ] All Message properties → the respective message forms
- [ ] All Sound properties → sounds (optional, can be "None")
- [ ] MainQuest → `IsekaiIntroQuest`

In `IsekaiPowerScript`:
- [ ] All ActorValue properties → `NONE` (or use directly)
- [ ] FormList properties → leave empty for now

### Phase 5: Save & test

- [ ] **File → Save** in the CK
- [ ] Copy `IsekaiHero.esp` into Skyrim Data
- [ ] Extract Scripts.7z to `Data/Scripts/`
- [ ] Start a new game
- [ ] Quest starts after character creation

---

## 🔧 Script fixes (already in the code)

### IsekaiDialogScript.psc — fixed line:
```papyrus
; OLD LINE (broken):
Explosion Property FXDragonDeath seq Auto

; NEW LINE (corrected):
Explosion Property FXDragonDeath Auto
```

---

## 🎮 Player-experience flow

```
1. Create a character (any)
2. The game starts
3. [SYSTEM] "Detecting soul signature..." (5-30 seconds)
4. [SYSTEM] "Analyzing dimensional residue..."
5. [SYSTEM] "Dimensional Origin" menu
   → Earth / Japan / Korea / Fantasy / Sci-Fi / Apocalyptic
6. [SYSTEM] "Status Allocation" menu
   → Normal / Hero / Ascended / Decline
7. (If not "Decline") [SYSTEM] "Skill Allocation"
   → Balanced / Warrior / Mage / Thief / Custom / Back
8. (If not "Decline") [SYSTEM] "Equipment Summoning"
   → Humble / Adventurer / Hero / None / Back
9. (If not "Decline") [SYSTEM] "Wealth Allocation"
   → Modest / Wealthy / Noble / Merchant Prince / Back
10. [SYSTEM] "REINCARNATION COMPLETE" summary
11. The game begins with the chosen bonuses!
```

---

## 📋 Known problems & solutions

### Quest doesn't start
- ✅ "Start Game Enabled" must be ticked
- ✅ Wait (can take up to 30 seconds)
- ✅ Check compatibility with start mods

### Scripts not found
- ✅ .pex files must be in `Data/Scripts/`
- ✅ Compiles in the CK without errors

### Message forms don't work
- ✅ Must have exactly the right button IDs (0, 1, 2, ...)
- ✅ Script names must match the property names

---

## 🚀 Future features (nice-to-have)

- [ ] MCM menu for re-spec
- [ ] Custom status window via SkyUI
- [ ] Ascended-mode visual effects
- [ ] More origin worlds
- [ ] Achievements
- [ ] Save/load configuration

---

## 📦 Dependencies

| Mod | Status | Note |
|-----|--------|-----------|
| Skyrim SE 1.5.x/1.6.x | ✅ Required | Base game |
| **SKSE64** | ✅ **Required** | `GetName()`, `UI`, `StringUtil` are used in the code |
| **SkyUI** | ✅ **Required** | MCM menu (`SKI_ConfigBase`) |
| **UIExtensions** | ✅ **Required** | Scroll menus (`UIListMenu`) |
| Alternate Start | ✅ Compatible | Auto-detected |
| Live Another Life | ✅ Compatible | Auto-detected |

> ⚠️ SKSE/SkyUI/UIExtensions are **not** optional extras — the code references their
> functions directly and won't compile without them.

---

## 🛠️ Installation (end user)

1. Install SKSE64, SkyUI and UIExtensions
2. Enable IsekaiHero.esp
3. Extract Scripts.7z to Data/
4. Start via the SKSE loader

---

_Last updated: 2026-06-28_
