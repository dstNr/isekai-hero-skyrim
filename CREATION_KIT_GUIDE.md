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

### 5.1 Message Kategorie öffnen

1. **Miscellaneous → Message**
2. Rechtsklick → **New**

### 5.2 Message 1: Power Choice

1. ID: `IsekaiMsg_PowerChoice`
2. Name: "Isekai Awakening"
3. Message Text:
   ```
   You feel strange energy flowing through your veins...

   Choose your destiny:

   [Normal] - Start as a regular adventurer
   [Hero] - Max skills, Level 1, 50 perks  
   [God Mode] - Level 255, 500 perks
   ```
4. Buttons:
   - Button 1: `Normal`
   - Button 2: `Hero`
   - Button 3: `God Mode`
   - Button 4: `Cancel`
5. **OK**

### 5.3 Message 2: Skill Focus

1. ID: `IsekaiMsg_SkillFocus`
2. Name: "Distribute Your Potential"
3. Message Text:
   ```
   How do you wish to focus your abilities?

   [All Equal] - 100 in every skill
   [Warrior] - Combat skills focused
   [Mage] - Magic skills focused
   [Thief] - Stealth skills focused
   [Custom] - Configure later
   ```
4. Buttons:
   - Button 1: `All Equal`
   - Button 2: `Warrior`
   - Button 3: `Mage`
   - Button 4: `Thief`
   - Button 5: `Custom`
   - Button 6: `Cancel`
5. **OK**

### 5.4 Message 3: Equipment

1. ID: `IsekaiMsg_EquipmentChoice`
2. Name: "Choose Your Equipment"
3. Message Text:
   ```
   What gear do you bring to this world?

   [Humble] - Iron armor, basic supplies
   [Adventurer] - Steel gear, potions, gold
   [Hero] - Legendary items
   [None] - Pure skill only
   ```
4. Buttons:
   - Button 1: `Humble`
   - Button 2: `Adventurer`
   - Button 3: `Hero`
   - Button 4: `None`
   - Button 5: `Cancel`
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
