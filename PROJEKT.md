# Isekai Hero Skyrim Mod — Projektplan

## 🎯 Ziel
Einen vollständigen Isekai-Mod für Skyrim Special Edition bauen, der:
- Den Spieler beim Start als "Reincarnated Hero" willkommen heißt
- System-Dialoge mit Weltwahl, Power-Level, Skills, Equipment, Wealth bietet
- **N.Y.A Modlist kompatibel ist** (Skyrim Unbound als Start-Mod)
- Mit allen gängigen Start-Mods (Skyrim Unbound, Alternate Start, LAL) kompatibel ist

---

## 📁 Projektstruktur

```
isekai-hero-skyrim/
├── PROJEKT.md              # Dieser Fahrplan
├── README.md               # Projektübersicht
├── WORKFLOW.md             # Git-Workflow
├── CREATION_KIT_GUIDE.md   # CK-Anleitung
├── FEATURES.md             # Geplante Features
├── compile.ps1             # Papyrus Compile-Check (Build-Skript)
│
├── Scripts/
│   └── Source/
│       ├── IsekaiIntroQuest.psc        # Haupt-Quest-Logik
│       ├── IsekaiDialogScript.psc      # System-Interface / Menüs (Vanilla + UIExtensions)
│       ├── IsekaiPowerScript.psc       # Skills / Equipment / Wealth
│       ├── IsekaiProgressionScript.psc # Milestones / Perk-Belohnungen
│       ├── IsekaiPerkDefinitions.psc   # Isekai-Perks
│       └── IsekaiMCMScript.psc         # MCM (SKI_ConfigBase / SkyUI)
│
└── Interface/              # (IsekaiMCMConfig.json entfernt — MCM läuft über SKI_ConfigBase-Script)
```

> **Status der Skripte:** Logik-Review + bekannte Compile-Fehler behoben
> (`GodModeAuraActive`, `EndEvent`/`EndFunction`, `Game.AddPerkPoints`,
> Tracked-Stat-Namen). **Echter Compiler-Gegencheck steht noch aus** — dafür
> wird die Skyrim-SE-Creation-Kit benötigt (siehe „Build-Toolchain" unten).

### 🔧 Build-Toolchain (für Compile-Check)

`compile.ps1` kompiliert alle `.psc` in `Scripts/Source` und meldet Fehler,
bevor das CK geöffnet wird. Voraussetzungen (einmalig zu installieren):

| Komponente | Liefert | Quelle |
|---|---|---|
| Skyrim SE **Creation Kit** | `PapyrusCompiler.exe`, Vanilla-Quellen, `TESV_Papyrus_Flags.flg` | Steam (kostenlos) |
| **SkyUI SDK** | `SKI_ConfigBase.psc` (für MCM) | SkyUI Modder-Resource |
| **UIExtensions** Quellen | `UIListMenu.psc` (für UIExt-Menüs) | UIExtensions Modder-Resource |

Aufruf: `./compile.ps1` (auto-detect) oder `./compile.ps1 -Only IsekaiPowerScript`.

---

## ✅ Checkliste: Was noch fehlt

### Phase 1: Creation Kit Setup (DONE-Anleitung)

- [ ] **CK öffnen** → Skyrim.esm als Master laden
- [ ] **Neues Plugin erstellen** → `IsekaiHero.esp`
- [ ] **ESL flaggen** (optional):
  1. **File → Compact Active File Form IDs**
  2. **File → Convert Active File to Light Master**
  3. **File → Save**
- [ ] **Quest erstellen** → `IsekaiIntroQuest` (ID: `IsekaiIntroQuest`)
  - [ ] Start Game Enabled ✓
  - [ ] Alle Stages (10, 20, 25, 30, 40, 50, 55, 60, 100)
  - [ ] Script: `IsekaiIntroQuest`

### Phase 1b: Skyrim Unbound Detection (FÜR N.Y.A MODLIST!)

> ⚠️ **WICHTIG:** N.Y.A Modlist nutzt Skyrim Unbound statt Alternate Start/LAL!

In der Quest `IsekaiIntroQuest` Properties:
- [ ] SkyrimUnboundInstalled → `SkyrimUnbound.esp` (prüft automatisch)
- [ ] AlternateStartInstalled → `AlternateStart.esp`
- [ ] LALInstalled → `Alternate Start - Live Another Life.esp`

Der Mod erkennt automatisch welches Start-System aktiv ist.

### Phase 2: Message Forms (DIALOGE)

> Diese liefern die Button-Rückgabewerte. **Müssen im CK erstellt werden!**

- [ ] `IsekaiMsg_SystemWelcome` — "Continue" → 1 Button
- [ ] `IsekaiMsg_WorldSelect` — 6 Buttons (0-5)
- [ ] `IsekaiMsg_PowerChoice` — 4 Buttons (0-3)
- [ ] `IsekaiMsg_SkillFocus` — 6 Buttons (0-5)
- [ ] `IsekaiMsg_EquipmentChoice` — 5 Buttons (0-4)
- [ ] `IsekaiMsg_WealthChoice` — 5 Buttons (0-4)
- [ ] `IsekaiMsg_SystemComplete` — 1 Button

### Phase 3: Scripts Kompilieren

- [ ] `IsekaiIntroQuest.psc` → `IsekaiIntroQuest.pex`
- [ ] `IsekaiDialogScript.psc` → `IsekaiDialogScript.pex`
- [ ] `IsekaiPowerScript.psc` → `IsekaiPowerScript.pex`

### Phase 4: Properties Verknüpfen

In der Quest `IsekaiIntroQuest`:
- [ ] DialogScript → `IsekaiDialogScript`
- [ ] PowerScript → `IsekaiPowerScript`

In `IsekaiDialogScript`:
- [ ] Alle Message Properties → jeweilige Message Forms
- [ ] Alle Sound Properties → Sounds (optional, kann "None" sein)
- [ ] MainQuest → `IsekaiIntroQuest`

In `IsekaiPowerScript`:
- [ ] Alle ActorValue Properties → `NONE` (oder direkt nutzen)
- [ ] FormList Properties →leer lassen vorerst

### Phase 5: Speichern & Testen

- [ ] **File → Save** in CK
- [ ] `IsekaiHero.esp` in Skyrim Data kopieren
- [ ] Scripts.7z entpacken nach `Data/Scripts/`
- [ ] Neues Spiel starten
- [ ] Quest startet nach Character Creation

---

## 🔧 Script-Fixes (bereits im Code)

### IsekaiDialogScript.psc — Gefixte Zeile:
```papyrus
; ALTE ZEILE (fehlerhaft):
Explosion Property FXDragonDeath seq Auto

; NEUE ZEILE (korrigiert):
Explosion Property FXDragonDeath Auto
```

---

## 🎮 Spielerlebnis-Flow

```
1. Charakter erstellen (beliebig)
2. Spiel startet
3. [SYSTEM] "Detecting soul signature..." (5-30 Sekunden)
4. [SYSTEM] "Analyzing dimensional residue..."
5. [SYSTEM] "Dimensional Origin" Menü
   → Earth / Japan / Korea / Fantasy / Sci-Fi / Apocalyptic
6. [SYSTEM] "Status Allocation" Menü
   → Normal / Hero / Ascended / Decline
7. (Falls nicht "Decline") [SYSTEM] "Skill Allocation"
   → Balanced / Warrior / Mage / Thief / Custom / Back
8. (Falls nicht "Decline") [SYSTEM] "Equipment Summoning"
   → Humble / Adventurer / Hero / None / Back
9. (Falls nicht "Decline") [SYSTEM] "Wealth Allocation"
   → Modest / Wealthy / Noble / Merchant Prince / Back
10. [SYSTEM] "REINCARNATION COMPLETE" Zusammenfassung
11. Spiel beginnt mit gewählten Boni!
```

---

## 📋 Bekannte Probleme & Lösungen

### Quest startet nicht
- ✅ "Start Game Enabled" muss angehakt sein
- ✅ Warten (kann bis 30 Sekunden dauern)
- ✅ Kompatibilität mit Start-Mods prüfen

### Scripts nicht gefunden
- ✅ .pex Dateien müssen in `Data/Scripts/` liegen
- ✅ Kompilieren im CK ohne Fehler

### Message Forms funktionieren nicht
- ✅ Müssen exakt die richtigen Button-IDs haben (0, 1, 2, ...)
- ✅ Script-Namen müssen mit Property-Namen übereinstimmen

---

## 🚀 Zukünftige Features (Nice-to-Have)

- [ ] MCM-Menü für Re-Spec
- [ ] Custom Status Window via SkyUI
- [ ] Ascended Mode visuelle Effekte
- [ ] Mehr Origin-Welten
- [ ] Achievements
- [ ] Konfiguration speichern/laden

---

## 📦 Abhängigkeiten

| Mod | Status | Bemerkung |
|-----|--------|-----------|
| Skyrim SE 1.5.x/1.6.x | ✅ Erforderlich | Base Game |
| **SKSE64** | ✅ **Erforderlich** | `GetName()`, `UI`, `StringUtil` werden im Code genutzt |
| **SkyUI** | ✅ **Erforderlich** | MCM-Menü (`SKI_ConfigBase`) |
| **UIExtensions** | ✅ **Erforderlich** | Scroll-Menüs (`UIListMenu`) |
| Alternate Start | ✅ Kompatibel | Auto-detected |
| Live Another Life | ✅ Kompatibel | Auto-detected |

> ⚠️ SKSE/SkyUI/UIExtensions sind **keine** optionalen Extras — der Code
> referenziert ihre Funktionen direkt und kompiliert ohne sie nicht.

---

## 🛠️ Installation (Endnutzer)

1. SKSE64, SkyUI und UIExtensions installieren
2. IsekaiHero.esp aktivieren
3. Scripts.7z nach Data/ entpacken
4. Über den SKSE-Loader starten

---

_Letzte Aktualisierung: 2026-06-28_
