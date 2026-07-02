# Isekai Hero - Creation Kit Guide (v2.0)

**Für Skyrim Special Edition 1.6.x / Anniversary Edition & Creation Kit v1.6+**

Dieses Guide führt dich Schritt für Schritt durch die Erstellung des Isekai Hero Mods im Creation Kit.

⚠️ **WICHTIG:** Das Creation Kit hat einige Bugs. Wenn etwas nicht funktioniert, schaue in den Troubleshooting-Abschnitt!

---

## 📋 Vorbereitung

### Benötigte Tools
1. **Skyrim Special Edition** (Steam) - muss installiert sein
2. **Creation Kit** (Steam → Bibliothek → Tools → Creation Kit)
3. **Scripts aus diesem Repository** - Kopiere den `Scripts/Source` Ordner in dein Skyrim Data-Verzeichnis

### Erstmaliges CK Setup
1. Starte das Creation Kit **als Administrator** (Rechtsklick → Als Administrator ausführen)
2. Beim ersten Start: **File → Select Paths...**
3. Stelle sicher, dass die Pfade korrekt sind:
   - **Skyrim:** `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`
   - **Scripts:** `Data\Scripts` (wichtig für Kompilierung!)

---

## 🚀 Schritt 1: Das Plugin Erstellen

### 1.1 CK Starten & Masters Laden

1. Öffne das Creation Kit (warte bis es geladen ist - kann 1-2 Minuten dauern)
2. **File → Data...** (oben links im Menü)
3. Es öffnet sich das "Data" Fenster
4. Setze einen Haken bei **Skyrim.esm**
5. Optional: Setze einen Haken bei **Update.esm** (falls vorhanden)
6. Klicke **OK** (nicht "Set as Active File!" noch nicht)
7. **Warte**, bis das CK fertig geladen hat (Statusleiste unten)
   - Du siehst "Loading..." in der unteren Leiste
   - Warte bis "Done" oder ähnlich erscheint

### 1.2 Neues Plugin Anlegen

1. **File → Save** (oder Strg+S)
2. Es öffnet sich der "Save Plugin" Dialog
3. **WICHTIG:** Wähle den richtigen Ordner!
   - Navigiere zu: `Steam\steamapps\common\Skyrim Special Edition\Data`
4. Gib als Dateiname ein: `IsekaiHero`
5. Stelle sicher, dass als Typ **.esp** ausgewählt ist
6. Klicke **Save**

✅ **Erfolg:** Du siehst jetzt oben im Fenster "IsekaiHero.esp" als aktive Datei

---

## 🏷️ Schritt 2: ESL Flag (Empfohlen)

Damit der Mod nicht zum 255-Plugin-Limit zählt:

### 2.1 Form IDs Kompaktifizieren

1. **File → Compact Active File Form IDs**
2. Es erscheint eine Warnung: "This will change all Form IDs..."
3. Klicke **YES** (Ja)
4. Warte kurz

### 2.2 Zu ESL Konvertieren

1. **File → Convert Active File to Light Master**
2. Klicke **YES** im Dialog

### 2.3 Speichern

1. **File → Save**
2. Klicke **YES** zum Überschreiben

✅ **Kontrolle:** Die Datei sollte jetzt nur noch ~100-200 KB groß sein

---

## 🎯 Schritt 3: Die Hauptquest Erstellen

### 3.1 Quest Anlegen

1. Im **Object Window** (das große Fenster mit vielen Kategorien):
   - Klicke auf das **+** neben "Character"
   - Klicke auf das **+** neben "Quest"
   - Oder: Gib oben im Filter-Feld "Quest" ein

2. **Rechtsklick** in die leere Liste rechts
3. Wähle **New** (oder drücke Insert)
4. Es öffnet sich das "Quest" Fenster

### 3.2 Quest Einstellungen

Fülle diese Felder aus (von oben nach unten):

| Feld | Wert | Beschreibung |
|------|------|--------------|
| **ID** | `IsekaiIntroQuest` | Eindeutige Identifikation |
| **Name** | `Isekai Hero - System Awakening` | Anzeigename |
| **Type** | `None` | Quest-Typ |
| **Priority** | `50` | Wichtigkeit (höher = wichtiger) |
| ☑ **Start Game Enabled** | AN | Startet automatisch |
| ☑ **Run Once** | AN | Läuft nur einmal |

**WICHTIG:** Klicke noch NICHT auf OK!

### 3.3 Script Hinzufügen

1. Im Quest-Fenster: Klicke auf den Tab **Scripts** (oben)
2. Klicke auf den Button **Add** (rechts)
3. Es öffnet sich "Add Script"
4. Wähle aus der Liste: `IsekaiIntroQuest`
   - Falls nicht in der Liste: Scripts müssen zuerst kompiliert werden (siehe Schritt 9)
5. Klicke **OK**

### 3.4 Properties Verknüpfen (KRITISCH!)

Dieser Schritt ist oft fehlerhaft in Anleitungen - hier detailliert:

1. Das Script `IsekaiIntroQuest` erscheint jetzt in der Liste
2. **Klicke auf das Script** (einmal anklicken, damit es blau markiert ist)
3. Klicke auf den Button **Properties** (rechts neben der Liste)
4. Es öffnet sich das "Properties" Fenster

Die Properties sind bereits im Skript als **Auto-Properties** deklariert – du musst
sie **nicht** manuell mit „Add Property" anlegen. Sie erscheinen automatisch, sobald
das Skript angehängt ist. Du setzt nur ihre **Werte** (Edit Value → Select Form):

| Property | Typ (automatisch) | Wert setzen auf |
|----------|-------------------|-----------------|
| `DialogScript` | `IsekaiDialogScript` | Quest mit `IsekaiDialogScript` |
| `PowerScript` | `IsekaiPowerScript` | Quest mit `IsekaiPowerScript` |
| `ProgressionScript` | `IsekaiProgressionScript` | Quest mit `IsekaiProgressionScript` |
| `PerkDefs` | `IsekaiPerkDefinitions` | Quest mit `IsekaiPerkDefinitions` |
| `MCM` | `IsekaiMCMScript` | Quest mit `IsekaiMCMScript` |

> ⚠️ Der Property-**Typ** ist der jeweilige Skript-Name (NICHT `Quest`). Er wird
> automatisch aus der Skript-Deklaration übernommen.

**WICHTIG:** Die Ziel-Quests werden erst in Schritt 8 erstellt. Lass die Werte
also zunächst auf **None** und verknüpfe sie später (am einfachsten per **Auto-Fill**,
sobald alle Quests existieren).

Danach: **OK** im Properties-Fenster, **OK** im Quest-Fenster.

✅ **Kontrolle:** Die Quest erscheint jetzt in der Object Window Liste

---

## 📊 Schritt 4: Quest Stages (Phasen)

### 4.1 Quest Öffnen

1. Doppelklicke auf `IsekaiIntroQuest` in der Liste
2. Klicke auf den Tab **Quest Stages**

### 4.2 Stages Hinzufügen

Für jede Stage:
1. Klicke **New** (rechts)
2. Gib die **Stage Nummer** ein
3. Optional: Füge einen **Log Entry** hinzu

Erstelle diese Stages:

| Stage | Log Entry | Beschreibung |
|-------|-----------|--------------|
| 10 | *(leer lassen)* | Warten auf Spawn |
| 20 | `[SYSTEM] Boot sequence initiated` | System startet |
| 25 | `[SYSTEM] Dimensional origin confirmed` | Welt gewählt |
| 30 | `[SYSTEM] Power level selected` | Power-Level gewählt |
| 40 | `[SYSTEM] Skill focus assigned` | Skills gewählt |
| 50 | `[SYSTEM] Equipment summoned` | Ausrüstung gewählt |
| 55 | `[SYSTEM] Wealth allocated` | Gold gewählt |
| 60 | `[SYSTEM] Applying changes...` | Werte werden gesetzt |
| 100 | `[SYSTEM] Reincarnation complete` | Abgeschlossen |

**So fügst du eine Stage hinzu:**
1. Klicke **New**
2. Gib die Nummer ein (z.B. 10)
3. Klicke auf die Stage (sie wird blau)
4. Unten im "Log Entry" Bereich:
   - Klicke **New**
   - Gib den Text ein
   - Klicke **OK**

5. Klicke **OK** im Quest-Fenster

---

## 💬 Schritt 5: Message Forms (Dialoge)

Message Forms sind die Dialogboxen, die im Spiel erscheinen.

### 5.1 Message Form Erstellen

1. Im Object Window:
   - Öffne "Character"
   - Wähle "Message"
2. **Rechtsklick** in die Liste → **New**

### 5.2 IsekaiMsg_WorldSelect

Fülle aus:

| Feld | Wert |
|------|------|
| **ID** | `IsekaiMsg_WorldSelect` |
| **Name** | `SYSTEM: Dimensional Origin` |

**Message Text** (kopiere das komplett):
```
From which world do you hail, Reincarnated One?

[1] EARTH - Modern world, no magic
[2] JAPAN - Land of truck-kun incidents  
[3] KOREA - Dungeons and hunters
[4] FANTASY WORLD - Swords and sorcery
[5] SCI-FI FUTURE - Advanced technology
[6] APOCALYPTIC - Survival and mutations
```

**Buttons** (sehr wichtig!):
1. Im "Buttons" Bereich (unten):
2. Klicke **New** für jeden Button
3. Gib den Text ein (einer pro Button):
   - Button 1: `Earth`
   - Button 2: `Japan`
   - Button 3: `Korea`
   - Button 4: `Fantasy World`
   - Button 5: `Sci-Fi Future`
   - Button 6: `Apocalyptic`

⚠️ **WICHTIG:** Die Reihenfolge ist kritisch! Button 0 = Earth, Button 1 = Japan, etc.

4. Klicke **OK**

### 5.3 IsekaiMsg_PowerChoice

| Feld | Wert |
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

| Feld | Wert |
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

| Feld | Wert |
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

| Feld | Wert |
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

| Feld | Wert |
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

## 📦 Schritt 6: FormLists Erstellen

FormLists sind Container für mehrere Items.

### 6.1 FormList Anlegen

1. Object Window → öffne "Items"
2. Wähle "FormList"
3. **Rechtsklick** → **New**

### 6.2 FormLists für Isekai Hero

Erstelle diese FormLists (eine nach der anderen):

#### Isekai_Weapons_Humble
- **ID:** `Isekai_Weapons_Humble`
- **FormID hinzufügen:**
  1. Klicke **Add** (rechts)
  2. Es öffnet sich "Select Form"
  3. Gib ein: `IronSword` (oder Form ID: `00012E4E`)
  4. Klicke **OK**
  5. Das Item erscheint in der Liste

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

## 🌟 Schritt 7: Perks Erstellen

Perks sind passive Boni für den Spieler.

### 7.1 Perk Anlegen

1. Object Window → öffne "Character"
2. Wähle "Perk"
3. **Rechtsklick** → **New**

### 7.2 Perks für Isekai Hero

Erstelle diese 13 Perks:

#### Reincarnated Soul Branch

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

#### Dimensional Knowledge Branch

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

#### System Protection Branch

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

#### Ascended Exclusive

**Isekai_SystemAdmin**
- **ID:** `Isekai_SystemAdmin`
- **Name:** `System Administrator`
- **Description:** `You are the Administrator of this world. Ultimate power unlocked.`

---

## 🎮 Schritt 8: Zusätzliche Quests (v2.0)

Für Progression, Perks und MCM brauchen wir separate Quests.

### 8.1 IsekaiProgression Quest

1. Object Window → Quest → **New**
2. Einstellungen:
   - **ID:** `IsekaiProgression`
   - **Name:** `Isekai Progression System`
   - ☑ **Start Game Enabled**
   - **Priority:** `45`
3. Tab **Scripts** → **Add** → `IsekaiProgressionScript`
4. **OK**

### 8.2 IsekaiPerks Quest

1. Quest → **New**
2. Einstellungen:
   - **ID:** `IsekaiPerks`
   - **Name:** `Isekai Perk System`
   - ☑ **Start Game Enabled**
   - **Priority:** `45`
3. Tab **Scripts** → **Add** → `IsekaiPerkDefinitions`
4. **OK**

### 8.3 IsekaiMCM Quest

1. Quest → **New**
2. Einstellungen:
   - **ID:** `IsekaiMCM`
   - **Name:** `Isekai MCM Menu`
   - ☑ **Start Game Enabled**
   - **Priority:** `40`
3. Tab **Scripts** → **Add** → `IsekaiMCMScript`
4. Properties setzen (Auto-Fill oder manuell):
   `MainQuest`, `DialogScript`, `PowerScript`, `Progression`, `QuestTracker`
   → jeweils auf die Quest mit dem entsprechenden Skript
5. **OK**

### 8.4 IsekaiQuestTracker Quest

Der Hauptquest-Belohnungs-Tracker → siehe eigener Abschnitt
**„🏆 Main Quest Tracker einrichten"** weiter unten (Quest anlegen,
`MainQuests`-Array füllen). Danach im MCM-Quest die `QuestTracker`-Property
darauf verweisen lassen.

---

## 🔧 Schritt 9: Scripts Kompilieren

Dieser Schritt ist oft problematisch - hier detailliert:

### 9.1 Vorbereitung

1. Stelle sicher, dass alle `.psc` Dateien im Ordner sind:
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
   > Hinweis: Skyrim SE nutzt `Data\Source\Scripts` (ältere Anleitungen sagen
   > `Data\Scripts\Source`). Nutze das Verzeichnis, in dem deine `Game.psc` liegt.

2. **WICHTIG:** SKSE, **SkyUI** (für MCM) und **UIExtensions** (für die Menüs)
   müssen installiert sein – ihre Skript-Quellen müssen zum Kompilieren auffindbar
   sein. Alternativ nutzt du das Repo-Skript `compile.ps1` (siehe `Scripts/SourceDeps/`).

### 9.2 Kompilierung

1. Im Creation Kit: **Gameplay → Compile Scripts** (oder drücke F6)
2. Es öffnet sich das "Compile Scripts" Fenster
3. **WICHTIG:** Setze einen Haken bei **"All"** (oben links)
4. Oder: Wähle nur die Isekai-Scripts aus
5. Klicke **Compile**

### 9.3 Fehler Beheben

Wenn Fehler auftreten:

**"Script not found"**
- Script-Datei fehlt im `Scripts\Source` Ordner
- Dateiname stimmt nicht überein (Groß-/Kleinschreibung!)

**"Unknown type"**
- SKSE ist nicht installiert
- Falsche Skyrim-Version (Oldrim vs SE)

**"Missing master"**
- Skyrim.esm nicht geladen
- Andere Dependencies fehlen

**"Failed to compile" ohne Details**
- CK Bug! Versuche:
  1. Schließe CK
  2. Lösche `Data\Scripts\Source\temp`
  3. Starte CK neu
  4. Versuche erneut

---

## 💾 Schritt 10: Speichern & Testen

### 10.1 Speichern

1. **File → Save**
2. Klicke **YES** zum Überschreiben
3. Warte bis gespeichert ist

### 10.2 Dateien Kopieren

Kopiere diese Dateien in deinen Mod-Ordner:

```
Aus: Skyrim Special Edition\Data\
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
> Hinweis: Das MCM läuft über das `IsekaiMCMScript` (SKI_ConfigBase) – es wird
> **keine** `IsekaiMCMConfig.json` mehr benötigt (wurde entfernt).

### 10.3 In-Game Test

1. Starte Skyrim SE über SKSE
2. Lade einen bestehenden Save oder starte neu
3. Nach der Charakter-Erstellung sollte das System starten:
   - Du siehst: "[SYSTEM] Initializing..."
   - Dann: "[SYSTEM] Detecting soul signature..."
   - Dann die Dialoge

---

## 🐛 Troubleshooting

### "Script not found" im Spiel

**Ursache:** Script wurde nicht kompiliert oder ist am falschen Ort

**Lösung:**
1. Prüfe: Existiert `Scripts\IsekaiIntroQuest.pex`?
2. Wenn nein: Kompiliere im CK erneut (Schritt 9)
3. Wenn ja: Stelle sicher, dass die .pex Dateien im Data-Ordner sind

### Quest startet nicht

**Ursache:** "Start Game Enabled" nicht gesetzt oder falsche Priority

**Lösung:**
1. Öffne die Quest im CK
2. Prüfe: ☑ **Start Game Enabled** ist angehakt
3. Prüfe: **Priority** ist mindestens 50
4. Speichere neu

### Message-Boxen erscheinen nicht

**Ursache:** Falsche Button-Indizes oder Message Forms nicht verknüpft

**Lösung:**
1. Prüfe: Message Form IDs stimmen mit Script-Properties überein
2. Prüfe: Buttons sind in richtiger Reihenfolge (0, 1, 2...)
3. Im Script: `IsekaiMsg_WorldSelect.Show()` gibt den Button-Index zurück

### CTD (Crash to Desktop)

**Ursachen & Lösungen:**

| Symptom | Ursache | Lösung |
|---------|---------|--------|
| Beim Start | ESL nicht korrekt | Form IDs kompaktifizieren |
| Beim Dialog | Falsche Message Form | Button-Indizes prüfen |
| Bei Equipment | Falsche Form ID | FormLists prüfen |
| Zufällig | Script-Loop | OnUpdate-Interval erhöhen |

### CK stürzt ab

**Häufige CK Bugs:**

1. **"Out of memory"**
   - CK ist 32-bit und hat Speicherlimit
   - Lösung: Speichere oft, starte CK neu

2. **"Failed to open file"**
   - CK läuft nicht als Administrator
   - Lösung: Rechtsklick → Als Administrator ausführen

3. **Scripts werden nicht kompiliert**
   - Lösche `Data\Scripts\Source\temp` Ordner
   - Starte CK neu

---

## 🏆 Main Quest Tracker einrichten (Solo-Leveling-Belohnungen)

Das Skript [`IsekaiQuestTracker.psc`](Scripts/Source/IsekaiQuestTracker.psc) belohnt
Hauptquest-Abschlüsse mit Perk-Punkten + Titeln. So verdrahtest du es:

### 1. Quest anlegen
1. **Object Window → Character → Quest → Rechtsklick → New**
2. **ID:** `IsekaiQuestTracker`
3. Reiter **Quest Data:** Haken bei **Start Game Enabled** und **Run Once**
4. Reiter **Scripts → Add → `IsekaiQuestTracker`**

### 2. MainQuests-Array füllen (WICHTIG: exakte Reihenfolge!)
Im Script-Properties-Fenster `MainQuests` markieren → **Edit Value** → Größe **12**,
dann jeden Index mit der passenden Vanilla-Quest belegen:

| Index | Quest Editor ID | Quest-Name | Belohnung |
|------:|-----------------|------------|-----------|
| 0 | `MQ101` | Unbound | +10 Perks, „Survivor" |
| 1 | `MQ102` | Before the Storm | +5 Perks |
| 2 | `MQ103` | Bleak Falls Barrow | +15 Perks, „Tomb Raider" |
| 3 | `MQ104` | Dragon Rising | +50 Perks, „Dragon Slayer" |
| 4 | `MQ105` | The Way of the Voice | +25 Perks, „Voice Wielder" |
| 5 | `MQ106` | The Horn of Jurgen Windcaller | +30 Perks |
| 6 | `MQ201` | A Blade in the Dark | +20 Perks |
| 7 | `MQ202` | Diplomatic Immunity | +40 Perks, „Spy" |
| 8 | `MQ203` | A Cornered Rat | +25 Perks |
| 9 | `MQ204` | Alduin's Wall | +30 Perks, „Time Reader" |
| 10 | `MQ205` | The Fallen | +50 Perks, „Dragon Tamer" |
| 11 | `MQ206` | Dragonslayer | +500 Perks, „World Savior" |

> ⚠️ Reihenfolge muss stimmen — die Belohnungen sind im Skript fest an den Index
> gekoppelt. Suche die Quests im Object Window per ID (Filter `MQ1`/`MQ2`).

### 3. Optionen (Properties)
- `EnableQuestRewards` = `True` (Standard)
- `RetroactiveRewards` = `False` → bereits abgeschlossene Quests bei Installation
  werden **nicht** nachträglich belohnt (für Bestands-Saves empfohlen)

### 4. Im MCM-Quest verknüpfen
In der Quest mit `IsekaiMCMScript` die neue Property `QuestTracker` auf
`IsekaiQuestTracker` setzen (für Status-Anzeige + Toggle im MCM).

---

## 📤 Distribution

Für die Veröffentlichung benötigst du:

### Erforderlich:
- `IsekaiHero.esp` (ESL-flagged)
- `Scripts\*.pex` (kompilierte Scripts)
- SKSE64, SkyUI, UIExtensions (als Mod-Abhängigkeiten im Mod-Beschreibungstext)

### Optional (empfohlen):
- `Scripts\Source\*.psc` (Quellcode)
- `README.md`
- `CREATION_KIT_GUIDE.md`

### Nicht mitliefern:
- `*.bsa` (wenn du keine Assets hast)
- `*.esp.bak` oder `*.esp.save`
- `temp` Ordner

---

**Bei Problemen:** Erstelle ein Issue im GitHub Repository mit:
1. CK Version
2. Skyrim Version
3. Exakte Fehlermeldung
4. Was du gerade versucht hast

_Good luck, modder!_ 🐉
