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
| **Effect Archetype** | **`Peak Value Modifier`** |
| **Casting Type** | `Constant Effect` |
| **Delivery** | `Self` |
| **Assoc. Item 1** | der Actor Value — siehe Tabelle |
| **Flags** | **`Recover`** ✅ und `No Duration` ✅ |
| | ❌ **NICHT** `Detrimental` |
| | ❌ **NICHT** `Hide in UI` — genau das wollen wir ja sehen |
| Magnitude/Duration/Area | leer lassen (0) |

> ### ⚠️ `Recover` ist der Haken, an dem alles hängt
>
> Ohne `Recover` behandelt Skyrim einen Werte-Modifikator **nicht als Fortify**, sondern
> als einmaligen Heil-/Schadenseffekt: Er ändert deinen *aktuellen* Wert einmal und dein
> *Maximum* nie. Der Effekt steht dann sauber im Menü, hat eine Magnitude — und wirkt
> trotzdem nicht.
>
> Ausgelesen aus den echten Spieldaten, alle mit `Recover` und Archetype `34`
> (= `Peak Value Modifier`):
>
> ```
> Fortify Magicka        archetype=34  Recover
> Fortify Stamina        archetype=34  Recover | NoDuration
> Fortify Carry Weight   archetype=34  Recover
> The Steed Stone        archetype=34  Recover
> Resist Magic           archetype=34  Recover | NoDuration
> ```

> **Es gibt kein Feld namens „Actor Value".** Der Actor Value steckt in
> **`Assoc. Item 1`** (links, unter *Minimum Skill Level*). Das Feld ist generisch
> benannt, weil sein Inhalt vom Archetype abhängt — bei `Value Modifier` listet es
> Actor Values. Wenn dort Objekte statt Actor Values auftauchen, `Effect Archetype`
> einmal weg- und wieder zurückstellen; das lädt die Liste neu.

Die acht:

| ID | Name | Assoc. Item 1 |
|---|---|---|
| `IsekaiME_Health` | `System: Health` | `Health` |
| `IsekaiME_Magicka` | `System: Magicka` | `Magicka` |
| `IsekaiME_Stamina` | `System: Stamina` | `Stamina` |
| `IsekaiME_CarryWeight` | `System: Carry Weight` | `CarryWeight` |
| `IsekaiME_ResistMagic` | `System: Magic Resist` | `ResistMagic` |
| `IsekaiME_ResistFire` | `System: Fire Resist` | `ResistFire` |
| `IsekaiME_ResistFrost` | `System: Frost Resist` | `ResistFrost` |
| `IsekaiME_ResistDisease` | `System: Disease Resist` | `ResistDisease` |

> **Warum die nüchternen Namen?** Skyrim zeigt unter *Aktive Effekte* den Namen des
> Magic Effects plus die Magnitude. Mit Fantasienamen stand da `+100 System: Burden` —
> hübsch, aber unlesbar. Jetzt steht dort `+100 System: Carry Weight`. Ein Statuseffekt,
> den man nachschlagen muss, ist ein schlechter Statuseffekt.

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

## Teil D — entfällt

> **Frühere Fassungen dieser Anleitung sagten hier, du sollst eine Zelle anlegen, die
> Truhe hineinstellen und sie als *Persistent Reference* markieren. Das war falsch:
> Skyrims Creation Kit hat diesen Haken gar nicht** (er stammt aus Oblivion/Fallout).
>
> Nötig ist er trotzdem — eine nicht-persistente Referenz existiert nur, solange ihre
> Zelle geladen ist, und wäre für den Code unauffindbar.
>
> **Lösung:** Der Code erzeugt die Referenz selbst, mit
> `PlaceObjectAtMe(base, forcePersist = true)`. Skyrim persistiert sie sauber im
> Savegame. Wir brauchen aus dem CK also **nur das Basis-Objekt aus Teil C** — keine
> Zelle, keine platzierte Truhe.
>
> Falls du die Zelle und die Truhe darin schon angelegt hast: einfach drinlassen, der
> Code ignoriert sie. Löschen geht auch, ist aber nicht nötig.

---

## Teil E — Sound Descriptors (4 Stück, per SSEEdit)

Die WAV-Dateien liegen bereits in `Data\Sound\fx\isekai\` (macht der Build-Workflow).

> **Warum SSEEdit statt Creation Kit?** Das CK hat einen bekannten Bug: der
> Sound-Descriptor-Dialog stürzt beim Schließen/OK ab. SSEEdit ist hier ohnehin der
> bessere Weg — beim Kopieren eines Vanilla-Descriptors kommen Category und Output
> Model (also die richtige Lautstärkeregler-Anbindung) automatisch mit, statt sie
> abtippen zu müssen.

1. **`SSEEdit.exe` direkt starten** (z. B. `E:\Modlists\NYA\tools\SSEEdit 4.1.5\`) —
   ⚠️ **nicht über MO2!** Direkt gestartet sieht es die Load-Order des Basis-Spiels,
   also genau unser Test-Setup.
2. Im Modul-Dialog: Rechtsklick → *Select None*, dann nur **`IsekaiHero.esp`** anhaken
   (die Master lädt es selbst) → OK. Warten bis unten rechts
   „Background Loader: finished" steht.
3. Links im Baum: **`Skyrim.esm` → `Sound Descriptor`** aufklappen.
4. In das Suchfeld **oben links** `UIMenuOKSD` eintippen + Enter → der Vanilla-Descriptor
   für Skyrims Menü-OK-Klick wird ausgewählt.
5. **Rechtsklick auf `UIMenuOKSD` → „Copy as new record into..."** → Häkchen bei
   `IsekaiHero.esp` → als neue Editor-ID `IsekaiSND_LevelUp` eingeben.
6. Den neuen Record auswählen (jetzt unter `IsekaiHero.esp → Sound Descriptor`).
   Rechts im Datenblatt den Eintrag **`ANAM - Sound File`** suchen → Doppelklick auf den
   Pfad → ersetzen durch `fx\isekai\Cinematic_6_1.wav`.
   *(Falls der kopierte Record mehrere Sound-Dateien listet: die überzähligen Zeilen
   per Rechtsklick → Remove löschen, genau eine bleibt.)*
7. Schritte 5–6 **in exakt dieser Reihenfolge** für die übrigen drei wiederholen:

| Nr. | Editor-ID | Sound File |
|---|---|---|
| 1 | `IsekaiSND_LevelUp` | `fx\isekai\Cinematic_6_1.wav` |
| 2 | `IsekaiSND_WindowOpen` | `fx\isekai\Cinematic_7_2.wav` |
| 3 | `IsekaiSND_ButtonClick` | `fx\isekai\Modern_2_2.wav` |
| 4 | `IsekaiSND_WindowClose` | `fx\isekai\Modern_5_2.wav` |

8. SSEEdit schließen → der Speichern-Dialog erscheint → Häkchen bei `IsekaiHero.esp`
   lassen → OK. (Ein Backup legt SSEEdit automatisch an.)

> ⚠️ **Die Reihenfolge ist wichtig.** Sound Descriptors tragen zur Laufzeit weder Namen
> noch Editor-ID, und ihre Dateipfade sind nur als Hash gespeichert — der Code ordnet
> sie über die FormID-Reihenfolge zu, und neue Records bekommen aufsteigende IDs in
> Erstellungsreihenfolge. Falls doch etwas vertauscht ist: sofort hörbar, leicht
> korrigierbar.

---

## Teil G — Das Storage-Token (per SSEEdit)

Der Zugang zum Dimensional Storage läuft über ein Inventar-Item: „benutzen" wie einen
Trank → Truhe öffnet sich, das Item bleibt erhalten (der Code fängt den Konsum ab).

> **Warum ein ALCH-Item und kein Ring?** Ein Ring müsste ausgerüstet werden, und
> Ausrüstungs-Slots sind zwischen Mods hart umkämpft (Cloaks, Bandoliers, …).
> Konsumieren berührt keinen einzigen Slot — null Konfliktfläche. Aussehen (Modell)
> und Name des Items sind trotzdem frei wählbar; nur die Inventar-Kategorie bleibt
> „Tränke".

In SSEEdit (wie in Teil E):

1. `Skyrim.esm` → Kategorie **`Ingestible`** → einen simplen Trank auswählen
   (z. B. eine *Potion of Minor Healing*)
   *(xEdit nennt den Record-Typ ALCH „Ingestible" — eine Kategorie „Potion" gibt es
   nur im Creation Kit.)*
2. Rechtsklick → **Copy as new record into…** → `IsekaiHero.esp`
   → Editor-ID: **`IsekaiStorageToken`**
3. Am neuen Record:
   - **`FULL - Name`** → `Dimensional Storage`
   - **`Effects`**-Block → Rechtsklick → **Remove** (komplett — keine Heilwirkung)
   - **`DATA - Weight`** → `0`
   - **`ENIT`**: `Value` → `0`, **`Sound - Consume` leeren** (sonst gluckert es beim Öffnen)
4. Speichern beim Schließen.

---

## Teil F — Speichern & einmal starten

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
| Kein Feld „Actor Value" zu finden | Heißt **`Assoc. Item 1`** (Teil A) |
| Kein Haken „Persistent Reference" | Gibt es in Skyrims CK nicht — Teil D entfällt |
| `Detrimental` / `Hide in UI` nicht zu sehen | Der Flags-Kasten hat drei Spalten; beide sollen ohnehin **leer** bleiben |
| CK stürzt beim Laden ab | `Skyrim.esm` **und** `Update.esm` müssen beide angehakt sein |
| Truhe leert sich von selbst | `Respawns`-Haken vergessen (Teil C) |
| Passives greifen nicht | Ability-Typ ist `Spell` statt `Ability` (Teil B) |
| Effekt taucht nicht im Menü auf | `Hide in UI` versehentlich angehakt (Teil A) |
