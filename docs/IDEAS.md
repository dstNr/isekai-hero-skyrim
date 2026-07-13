# 💡 Feature-Ideen (C++ Version)

Sammelstelle für Ideen zur aktuellen native-SKSE-Version, bevor sie zu echten
Tasks werden. Unsortiert nach Priorität — einfach chronologisch angehängt.

---

## Cross-Save Legacy / "New Game+"

**Status:** 🧠 Idee, noch nicht spezifiziert
**Ursprung:** Der eigentliche Auslöser für den ganzen Isekai-Mod: Beim x-ten
Neustart eines Spielstands hat man keine Lust, wieder alles von null
freizuschalten — und cheatet es sich am Ende ohnehin rein. Die
Isekai-Prämisse ("du wirst mit Boni reinkarniert") soll genau das ersetzen,
aber ehrlich verdient über vorherige Playthroughs statt per Konsolenbefehl.

### Kernidee

Fortschritt soll nicht nur *innerhalb* eines Spielstands zählen (wie
aktuell: Milestones, Blessing, Perk-Punkte über `SKSE::SerializationInterface`
im Co-Save), sondern auch **zwischen** verschiedenen Playthroughs vererbbar
sein. Wer Run 1 durchspielt, soll in Run 2 spürbar schneller wieder auf
Touren kommen — nicht durch rohes Cheaten, sondern durch das
Isekai-Reincarnation-Menü, das es eh schon gibt.

### Warum "Spielstand wählen → Items rüberkopieren" so nicht funktioniert

- Der aktuelle Fortschritt lebt im Co-Save der jeweiligen `.ess`-Datei
  (`System.cpp`: `SaveCallback` / `LoadCallback`). Das ist pro Playthrough
  isoliert; `RevertCallback` setzt `g_state` bei jedem New Game sogar hart
  zurück.
- Zwei Saves können sich nicht gegenseitig lesen — komplett getrennte
  Spielwelten, getrennte Objekt-Referenzen.
- Physische Items (insbesondere individuell verzauberte/benannte) sind an
  Referenzen *in genau diesem Save* gebunden. Es gibt kein "Item aus Save A
  exportieren, in Save B importieren" im Skyrim-Save-Format — der Code müsste
  das Item in Save B komplett neu erzeugen.

### Technischer Ansatz, der funktioniert

Ein **globaler Fortschritts-Speicher außerhalb jedes Savegames** — eine
einfache Datei (z. B. JSON) unter
`Documents/My Games/Skyrim Special Edition/SKSE/`, gebunden an die
Mod-Installation, nicht an ein einzelnes Savegame. In C++ trivial per
`fstream`, kein StorageUtil/Papyrus-Umweg wie in der alten Version nötig.

**Vorgeschlagener Flow:**
1. Am Ende eines Playthroughs (oder laufend) wird Fortschritt in diese
   globale "Legacy"-Datei eingezahlt: erreichte Milestones, höchste je
   gewählte Blessing, Dragon-Soul-Guthaben, ggf. freigeschaltete Titel.
2. Bei `kNewGame` (Hook existiert bereits in `System.cpp`) liest der Code
   die Legacy-Datei und bietet im bestehenden Reincarnation-Menü
   (`ShowPowerSelection()`) einen zusätzlichen Punkt an: *"Inherit from
   previous life"*.
3. Das Einlösen nutzt exakt den Mechanismus, den `ApplyReincarnation()`
   heute schon für die Blessings hat (Skills/Level/Gold/Perks setzen) — nur
   gefüttert aus echtem Vorfortschritt statt festen Blessing-Tabellenwerten.

**Was leicht geht (Zahlen-Progression):**
- Skills, Charakterlevel, Gold, Perk-Punkte, Dragon Souls — alles bereits
  simple Werte im State, banking/vererben ist unkompliziert.

**Was schwerer ist (Item-Vererbung) — bewusster Split:**
- *Leicht:* Eine kuratierte Liste fester "Legacy Items" (Forms aus der ESP,
  analog zu den 8 Passive-Abilities). Wird beim ersten Erhalt global
  freigeschaltet und im neuen Leben z. B. aus einer Art Dimensional-Storage-
  Truhe (siehe altes Backlog) abholbar.
- *Schwer/später:* Beliebige selbst verzauberte/gecraftete Gegenstände 1:1
  mitnehmen — für den ersten Wurf bewusst außen vor lassen.

### Offene Fragen (noch zu klären, bevor das zur Task wird)
- Was genau vererbt sich: nur Zahlen, oder auch die Legacy-Item-Liste von
  Anfang an?
- Vererbt sich pro Charakter-Save oder global über alle Saves hinweg (auch
  unterschiedliche Charaktere)?
- Soll das Einzahlen automatisch passieren (z. B. bei Enderfolg des
  Hauptquests) oder ein bewusster Menü-Trigger sein?
- Balance: Wie stark darf Run 2 dadurch beschleunigt werden, ohne dass die
  Isekai-Prämisse selbst trivial wird?
