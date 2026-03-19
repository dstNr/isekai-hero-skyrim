# Creation Kit Setup Guide — Isekai Hero

**Voraussetzung:** Du führst die Schritte in Creation Kit aus, ich beschreibe sie.

---

## 1. Creation Kit Starten

1. Steam → Bibliothek → Tools
2. "Skyrim Special Edition Creation Kit" installieren/starten
3. Warte bis geladen (kann ein paar Minuten dauern)

---

## 2. Neues Plugin Erstellen

1. **File → Data...**
2. Warte bis "Master Files" geladen sind
3. **Skyrim.esm** auswählen → **Set as Active**
4. Klicke **OK** (lädt jetzt, dauert lange)
5. **File → Save** → Speichere als: `IsekaiHero.esp`

---

## 3. Als ESL Flaggen (wichtig!)

1. **File → Data...** erneut öffnen
2. **IsekaiHero.esp** auswählen → **Set as Active**
3. Rechtsklick auf **IsekaiHero.esp**
4. **Light (ESL)** ankreuzen
5. **OK**

---

## 4. Quest Erstellen

### 4.1 Quest-Record anlegen

1. **Character → Quest...**
2. Rechtsklick → **New**
3. ID: `IsekaiIntroQuest`
4. Name: "Isekai Awakening"
5. Priority: `50`
6. **OK**

### 4.2 Quest-Eigenschaften

1. Doppelklick auf `IsekaiIntroQuest`
2. Tab **Quest Data**:
   - Type: `Miscellaneous`
   - Start Game Enabled: ☑ **HÄKCHEN SETZEN** (wichtig!)
   - Allow Repeated Stages: ☐
3. Tab **Quest Stages**:
   - Klicke **New** (mehrfach für Stages)
   
   Stage 10:
   - Index: `10`
   - Log Entry: "Waiting for player to spawn..."
   
   Stage 20:
   - Index: `20`
   - Log Entry: "The awakening begins..."
   
   Stage 30:
   - Index: `30`
   - Log Entry: "Choose your power level"
   
   Stage 40:
   - Index: `40`
   - Log Entry: "Choose skill focus"
   
   Stage 50:
   - Index: `50`
   - Log Entry: "Choose equipment"
   
   Stage 60:
   - Index: `60`
   - Log Entry: "Your destiny is sealed"
   
   Stage 100:
   - Index: `100`
   - Log Entry: "Isekai awakening complete"
   
4. Tab **Scripts**:
   - Klicke **Add**
   - Name: `IsekaiIntroQuest`
   - **OK**
   - Klicke **Properties**
   - **Add Property** für:
     - `DialogScript` → Type: `IsekaiDialogScript`
     - `PowerScript` → Type: `IsekaiPowerScript`
   - **OK** → **OK**

---

## 5. Message Forms Erstellen (Dialoge)

**WICHTIG:** Diese Message Forms sind **zwingend erforderlich** — sie liefern die Button-Rückgabewerte für die Dialoge.

### 5.1 Message Kategorie öffnen

1. **Miscellaneous → Message**
2. Rechtsklick → **New**

### 5.2 Message 1: System Welcome (Optional - wird im Script gebaut)

1. ID: `IsekaiMsg_SystemWelcome`
2. Name: "System Welcome"
3. Message Text: `[SYSTEM] Initializing...`
4. Button 1: `Continue`
5. **OK**

### 5.3 Message 2: Power Choice (WICHTIG)

1. ID: `IsekaiMsg_PowerChoice`
2. Name: "Status Allocation"
3. Message Text: `[SYSTEM] Select blessing level`
4. **Buttons (Rückgabewerte):**
   - Button 1: `NORMAL` → **0**
   - Button 2: `HERO` → **1**
   - Button 3: `GOD MODE` → **2**
   - Button 4: `DECLINE` → **3**
5. **OK**

### 5.4 Message 3: Skill Focus (WICHTIG)

1. ID: `IsekaiMsg_SkillFocus`
2. Name: "Skill Allocation"
3. Message Text: `[SYSTEM] Select expertise`
4. **Buttons:**
   - Button 1: `BALANCED` → **0**
   - Button 2: `WARRIOR` → **1**
   - Button 3: `MAGE` → **2**
   - Button 4: `THIEF` → **3**
   - Button 5: `CUSTOM` → **4**
   - Button 6: `BACK` → **5**
5. **OK**

### 5.5 Message 4: Equipment (WICHTIG)

1. ID: `IsekaiMsg_EquipmentChoice`
2. Name: "Equipment Summoning"
3. Message Text: `[SYSTEM] Select equipment`
4. **Buttons:**
   - Button 1: `HUMBLE` → **0**
   - Button 2: `ADVENTURER` → **1**
   - Button 3: `HERO` → **2**
   - Button 4: `NONE` → **3**
   - Button 5: `BACK` → **4**
5. **OK**

### 5.6 Message 5: System Complete (Optional)

1. ID: `IsekaiMsg_SystemComplete`
2. Name: "Reincarnation Complete"
3. Message Text: `[SYSTEM] Status applied`
4. Button 1: `Acknowledge`
5. **OK**

---

## 6. FormLists für Equipment (optional)

1. **Items → FormList**
2. Rechtsklick → **New**
3. Erstelle 3 Listen:
   - `Isekai_Equipment_Humble`
   - `Isekai_Equipment_Adventurer`
   - `Isekai_Equipment_Hero`
4. Füge Items hinzu (oder lass leer für jetzt)

---

## 7. Scripts Kompilieren

1. **Gameplay → Scripts → Compile**
2. Finde `IsekaiIntroQuest.psc`
3. **Compile**
4. Wiederhole für:
   - `IsekaiDialogScript.psc`
   - `IsekaiPowerScript.psc`
5. Prüfe auf Fehler (grün = OK, rot = Fehler)

---

## 8. Speichern & Testen

1. **File → Save**
2. Schließe Creation Kit
3. Kopiere `IsekaiHero.esp` nach:
   - `C:\Users\[NAME]\Documents\My Games\Skyrim Special Edition\Data\`
   - Oder dein Mod-Organizer-Profil
4. Starte Skyrim SE mit aktiviertem Mod
5. Neues Spiel starten → Testen!

---

## 9. UIExtensions Integration (Optional)

Für ein verbessertes Erlebnis kannst du **UIExtensions** installieren:

### Was verbessert sich?
- Schönere Listen-Menüs statt MessageBoxen
- Bessere Tastatur-Navigation
- Beschreibungen zu jedem Eintrag

### Installation
1. Lade [UIExtensions](https://www.nexusmods.com/skyrimspecialedition/mods/17561) herunter
2. Installiere mit deinem Mod Manager
3. Das Skript erkennt UIExtensions automatisch

### Ohne UIExtensions
Der Mod funktioniert komplett ohne UIExtensions — dann werden die Standard-Message Forms verwendet.

---

## Troubleshooting

### Quest startet nicht
- Prüfe: "Start Game Enabled" ist angehakt?
- Warte länger (mod kann nach Char-Erstellung einige Sekunden brauchen)

### Scripts nicht gefunden
- Scripts müssen im `Scripts` Ordner liegen
- Kompilieren funktioniert? (Keine roten Fehler?)

### Dialoge nicht sichtbar
- Message Forms korrekt erstellt?
- ID-Namen stimmen mit Script überein?

---

**Nächster Schritt nach dem Testen:**
- Balancing (zu stark/schwach?)
- MCM-Integration (optional)
- Nexus-Release

---

*Bei Problemen: Screenshots vom Creation Kit schicken!*
