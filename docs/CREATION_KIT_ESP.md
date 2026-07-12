# Creation-Kit-Anleitung: `IsekaiHero.esp`

Einmalige CK-Sitzung, ca. 20–30 Minuten. Danach macht wieder ausschließlich C++ die
Arbeit — du musst das Creation Kit nie wieder anfassen.

## Warum überhaupt eine ESP?

Zwei Dinge gehen ESP-los technisch nicht:

1. **Passives im Magie-Menü.** Ein Eintrag unter *Aktive Effekte* ist zwingend ein
   Ability-Spell mit MagicEffect — beides Forms, und Forms kommen nur aus einer
   Plugin-Datei. (Zur Laufzeit erzeugte Forms speichert Skyrim nicht zuverlässig; beim
   nächsten Laden zeigt der Spielstand ins Leere.)
2. **Interdimensional Storage.** Eine Truhe ist eine Form plus eine platzierte
   Referenz. Gleiches Problem — nur wären hier deine *Items* weg.

## Warum nur 8 Passives und nicht 61?

Die Stärke eines Ability-Effekts ist in der Plugin-Datei **fest einbetoniert**.
„+25 Health" ist ein anderer Spell als „+100 Health". Ein Passive pro Quest, mal drei
Blessing-Stufen, wären **183 Spells** von Hand.

Stattdessen: **ein Passive pro Statwert**, dessen Stärke der Code zur Laufzeit setzt.
Die Werte summieren sich sichtbar hoch, während du Quests abschließt — und dein
Effektmenü wird nicht von 61 Einzeleinträgen zugemüllt.

---

## Vorbereitung

1. Creation Kit starten.
2. **File → Data**
3. Häkchen bei **`Skyrim.esm`** und **`Update.esm`**. Sonst nichts.
   *(Keine DLC-Master: wir referenzieren keine DLC-Inhalte. Das hält die ESP
   abhängigkeitsfrei — sie läuft auch bei Leuten ohne DLCs.)*
4. **KEIN** "Set as Active File" antippen — es gibt noch keine ESP.
5. **OK**, warten bis geladen (dauert, und wirft Warnungen — alle mit *Yes to All* wegklicken).
6. **File → Save**, Dateiname: **`IsekaiHero.esp`**

Ab jetzt ist `IsekaiHero.esp` die aktive Datei; alles, was du anlegst, landet darin.

---

## Teil A — Magic Effects (8 Stück)

**Object Window** → Baum links: **Magic → Magic Effect**
→ Rechtsklick in die Liste → **New**

Für **jeden** der acht Einträge unten identisch ausfüllen:

| Feld | Wert |
|---|---|
| **ID** | siehe Tabelle |
| **Name** | siehe Tabelle *(das ist der Text, den du später im Effektmenü siehst)* |
| **Effect Archetype** | `Value Modifier` |
| **Casting Type** | `Constant Effect` |
| **Delivery** | `Self` |
| **Actor Value** | siehe Tabelle |
| **Flags** | **nur** `No Hit Event` und `No Duration` ankreuzen |
| | ❌ **NICHT** `Detrimental` |
| | ❌ **NICHT** `Hide in UI` — genau das wollen wir ja sehen |
| Magnitude/Duration/Area | leer lassen (0) |

Die acht:

| ID | Name | Actor Value |
|---|---|---|
| `IsekaiME_Health` | `System: Vitality` | `Health` |
| `IsekaiME_Magicka` | `System: Arcane` | `Magicka` |
| `IsekaiME_Stamina` | `System: Endurance` | `Stamina` |
| `IsekaiME_CarryWeight` | `System: Burden` | `CarryWeight` |
| `IsekaiME_ResistMagic` | `System: Warding` | `ResistMagic` |
| `IsekaiME_ResistFire` | `System: Emberskin` | `ResistFire` |
| `IsekaiME_ResistFrost` | `System: Frostskin` | `ResistFrost` |
| `IsekaiME_ResistDisease` | `System: Purity` | `ResistDisease` |

> **Tipp:** Den ersten anlegen, dann in der Liste **rechtsklicken → Duplicate** und nur
> ID, Name und Actor Value ändern. Spart viel Klickerei.

---

## Teil B — Abilities (8 Stück)

**Object Window** → **Magic → Spell** → Rechtsklick → **New**

Für jeden:

| Feld | Wert |
|---|---|
| **ID** | siehe Tabelle |
| **Name** | derselbe wie beim zugehörigen Magic Effect |
| **Type** | `Ability` |
| **Casting** | `Constant Effect` |
| **Delivery** | `Self` |
| **Cost / Charge Time** | 0 |

Dann unten im Kasten **Effects** → Rechtsklick → **New**:
- Den passenden Magic Effect aus Teil A auswählen
- **Magnitude: `0`**, **Duration: `0`**, **Area: `0`**

> **Die Magnitude `0` ist Absicht.** Der Code setzt sie beim Laden auf deinen echten,
> aufsummierten Wert. Stünde hier eine Zahl, wäre das nur eine Lüge, die überschrieben wird.

| Ability-ID | Magic Effect aus Teil A |
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

## Teil C — Die Truhe

**Object Window** → **World Objects → Container**

Suche in der Liste eine vorhandene Truhe, z. B. **`TreasChestSmall01`**.
→ Rechtsklick → **Duplicate** *(nicht "New" — so erbst du Modell und Sound gratis)*

Am Duplikat ändern:

| Feld | Wert |
|---|---|
| **ID** | `IsekaiStorageContainer` |
| **Name** | `Dimensional Storage` |
| **Respawns** | ❌ **UNBEDINGT ABWÄHLEN** |
| Inhalt (Item-Liste) | alles löschen — die Truhe startet leer |

> ⚠️ **`Respawns` ist der kritische Haken.** Bleibt er gesetzt, **leert Skyrim deine
> Truhe alle paar Spieltage automatisch aus.** Deine Items wären weg.

---

## Teil D — Die Truhe platzieren

Die Truhe braucht einen Ort. Wir bauen ihr eine eigene, leere Zelle, die kein Spieler
je betritt — so kann sie mit nichts kollidieren.

1. **Cell View** (Fenster links unten). Im Dropdown **Interiors** auswählen.
2. In der Zellenliste **Rechtsklick → New**
3. **ID:** `IsekaiVault` → **OK**
4. Doppelklick auf `IsekaiVault` → das (leere, schwarze) Render-Fenster öffnet sich.
5. Deine `IsekaiStorageContainer` aus dem Object Window **per Drag & Drop** ins
   Render-Fenster ziehen.
6. **Doppelklick auf die platzierte Truhe** → Reference-Fenster geht auf.
7. Reiter **"Reference"**: Häkchen bei **`Persistent Reference`** setzen. ⚠️ **Wichtig** —
   ohne das kann der Code sie nicht von überall aus ansprechen.
8. **OK**.

---

## Teil E — Speichern & einmal starten

1. **File → Save** (überschreibt `IsekaiHero.esp`).
2. Sicherstellen, dass die ESP im Spiel **aktiviert** ist (Steam-Launcher, MO2, oder
   `plugins.txt` — je nachdem, wie du das Basis-Setup startest).
3. Skyrim starten, bis ins Spiel (`coc riverwood` reicht).
4. **Bescheid sagen.**

Das Plugin schreibt dann **alle Forms aus `IsekaiHero.esp` samt FormID ins Log**. Ich lese
sie aus und verdrahte sie im Code — dieselbe Methode, mit der wir schon die Quest-IDs
verifiziert haben. So gibt es keine geratene Zahl im Code.

---

## Häufige Stolpersteine

| Symptom | Ursache |
|---|---|
| CK stürzt beim Laden ab | `Skyrim.esm` **und** `Update.esm` müssen beide angehakt sein |
| Truhe leert sich von selbst | `Respawns`-Haken vergessen (Teil C) |
| Passives greifen nicht | Ability-Typ ist `Spell` statt `Ability` (Teil B) |
| Effekt taucht nicht im Menü auf | `Hide in UI` versehentlich angehakt (Teil A) |
