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

---

## System-Skilltree im Interface

**Status:** 🧠 Idee, noch nicht spezifiziert
**Ursprung:** Zwei Dinge liegen im Code bereits halb fertig herum und wollen
zusammengeführt werden: `System.h` definiert ein `SkillFocus`-Enum
(`Balanced/Warrior/Mage/Thief/Custom`), das aktuell **nirgendwo verwendet**
wird — reiner Platzhalter. Und `Passives.cpp`/`Progression.cpp` haben schon
einen sauberen Mechanismus, um Actor-Value-Boni aus einer Liste "verdienter"
Passives *abzuleiten* (nie zu akkumulieren). Ein Skilltree im System-Interface
würde beides tatsächlich benutzen, statt es brachliegen zu lassen.

### Kernidee

Ein eigenes Menü ("System: Skill Tree"), erreichbar aus dem bestehenden
System-Interface, in dem der Spieler eine neue Währung — **System-Punkte** —
in frei wählbare Knoten investiert. Jeder Knoten gewährt einen passiven
Stat-Bonus (genau wie die bestehenden 8 Passives), aber der Spieler
entscheidet *welchen*, statt dass er automatisch aus einem Milestone fällt.

**Struktur:** kein linearer Pfad, sondern ein verzweigtes Netz mit
Voraussetzungen — ähnlich Skyrims eigenem Perk-Baum. Organisiert in drei
Regionen, die endlich `SkillFocus` einen Zweck geben:

- **Warrior** — Health/Stamina-lastige Knoten, Rüstungs-/Nahkampf-Resistenzen
- **Mage** — Magicka-lastige Knoten, Elementarresistenzen
- **Thief** — Carry Weight/Stamina, Ausweich-/Diebstahl-nahe Stats

Dazu ein kleiner neutraler **Hub** in der Mitte, von dem aus alle drei
Branches abzweigen — Voraussetzungsketten können auch branch-übergreifend
verlaufen (ein Warrior-Knoten kann einen Mage-Knoten als Vorbedingung
verlangen), das Netz ist nicht strikt in drei Silos getrennt.

Die bei der Reincarnation gewählte `SkillFocus`-Richtung könnte den Hub-Knoten
der eigenen Branch vergünstigen oder direkt freischalten — ohne die anderen
beiden Branches zu sperren. Das gibt der bisher folgenlosen Wahl im
Reincarnation-Menü endlich eine spürbare Konsequenz.

### Währung: System-Punkte

Bewusst **nicht** dieselben Perk Points, die in Skyrims eigenes Perk-System
fließen (`GrantPerkPoints`, gedeckelt bei 127 durch die Engine) — eine
getrennte, ungedeckelte Zählgröße, analog zu Dragon Souls.

**Quelle, zwei Ideen, die sich kombinieren lassen:**
1. Jeder Milestone zahlt zusätzlich zu Passive/Souls/Perk-Points einen
   kleinen Batzen System-Punkte aus (Endpoints entsprechend mehr) —
   skaliert mit `RewardScale()` wie alles andere, damit die
   Blessing-Wahl auch hier weiter mitzählt.
2. Dragon Souls sind nach der Hauptquest ziemlich nutzlos, sobald alle
   Word Walls leer sind. Ein Umtausch-Kurs (z. B. 1 Dragon Soul → X
   System-Punkte) gäbe der Währung einen Sinn über Shouts hinaus — genau
   die Art Dragon-Soul-Sink, die in `papyrus/FEATURES.md` schon als
   offene Idee ("System Shop") herumsteht.

### Technischer Ansatz

**Datenmodell:** ein `SkillTree`-Modul analog zu `Progression`/`Passives` —
eine statische Knoten-Tabelle (`key`, Voraussetzungen als Liste von Keys,
`Passive`-Payload, Kosten in System-Punkten, Branch-Zugehörigkeit). Freigeschaltete
Knoten landen in `State` (neues Feld neben `grantedMilestones`) und werden im
Co-Save mitgespeichert.

**Passives-Integration:** `Progression::EarnedPassives()` liefert aktuell nur
Milestone-Passives. Naheliegend, das auf eine gemeinsame Quelle zu erweitern
(Milestones **+** freigeschaltete Baum-Knoten), damit `Passives::Refresh()`
unverändert weiter alles aus einer Liste ableitet, statt einen zweiten,
parallelen Anwendungspfad zu bauen.

**Grenze, die früh geklärt werden muss:** `Passives::Refresh()` treibt aktuell
nur 8 feste Ability-Spells (`kAbilityFormIDs`), eine pro Actor Value. Ein
Skilltree mit mehr Knoten als Actor Values bräuchte entweder mehrere Knoten
pro Actor Value (mehrere Knoten addieren sich auf denselben Bonus — passt zum
bestehenden "Summe aller Passives pro AV"-Modell) oder zusätzliche
Ability-Spells in der ESP für neue Actor Values. Ersteres ist ohne
ESP-Änderung machbar, zweites braucht Creation-Kit-Arbeit.

**UI:** Das bestehende `SystemWindow` ist ein reiner Text+Button-Dialog
(Titel, Body, Choices) — kein Graph-Layout. Ein Skilltree mit Knoten,
Verbindungslinien und Voraussetzungs-Highlighting ist eine neue
ImGui-Komponente (z. B. `UI/SkillTreeWindow.cpp`), die den visuellen Stil aus
`Style.h` (Glow-Border, Corner-Brackets, Akzentfarbe) übernimmt, aber ein
eigenes Layout braucht — vermutlich mit festen Node-Koordinaten pro Branch
statt automatischem Graph-Layout, um die Renderlast und Komplexität klein zu
halten.

**Anknüpfung an Cross-Save Legacy:** Ein Pool aus System-Punkten +
freigeschalteten Knoten ist genau die Art einfacher, banking-fähiger
Zahlen-Progression, die die Legacy-Idee weiter oben als "leicht vererbbar"
einstuft — ein Knotenpunkt für später, falls beide Features kommen.

### Offene Fragen (noch zu klären, bevor das zur Task wird)
- Exakter Umtausch-Kurs Dragon Souls → System-Punkte, falls Idee 2 kommt —
  und ob er überhaupt nötig ist oder Milestones allein genug Fluss liefern.
- Wie groß wird das Netz (grobe Knotenzahl pro Branch), bevor es umsetzbar
  entworfen werden kann?
- Respec: Sind einmal gesetzte Knoten permanent, oder gibt es (wie in
  `papyrus/FEATURES.md` als Idee vermerkt) einen Respec-Mechanismus, der dann
  auch System-Punkte zurückzahlen müsste?
- Verändert die bei der Reincarnation gewählte `SkillFocus`-Richtung nur den
  Hub-Knoten der eigenen Branch, oder auch die Kosten/Verfügbarkeit in den
  beiden anderen Branches?

---

## User-Feedback-Backlog (Nexus, v0.4.x)

Sammlung aus einem ausführlichen User-Report. Umgesetzt in v0.5.0: das
"Shattered"-Erwachen (Segens-Tier ohne flachen Start-Grant), repeatable
Utility-Knoten (Fleet of Foot / Beast of Burden / Enduring Vigor) und die
optionale INI `HideSealedNodes`. Der Hotkey öffnet nicht mehr über einem
offenen Spielmenü. Offen / auf der Roadmap:

- **Storage-Codex als physischer Fallback (Item 4).** Ein Buch/Item im Inventar
  (per Default vorhanden, nicht wegwerfbar), das die Dimensional Storage öffnet
  — als Backup, falls das ImGui-Overlay mal klemmt, und als Zugang ohne
  Crafting-Station. Umsetzung: kleines MISC/BOOK in der ESP + Aktivierungs-Hook
  (oder Papyrus-Fragment), das `Storage::Open()` ruft. Klein–mittel.
  *Hinweis:* Storage ist bereits jederzeit über das RShift+S-Panel erreichbar
  (nicht nur am Tisch); der Cell-Reset-Reparatur-Fix (v0.4.0) entschärft den
  "Menü klemmt"-Fall schon deutlich — der Codex ist der Gürtel zur Hosenträger.

- **Keybind voll remapbar + Modifier wählbar (Ctrl statt RShift).** Braucht die
  Config (INI existiert seit v0.5.0 — dort ein `[Hotkey]`-Abschnitt mit
  Scancode + Modifier ergänzen und `Progression.cpp`/`Input.cpp` daraus
  speisen). Mittel.

- **Noch mehr Knoten-Varietät.** Weitere repeatable Stats über die bereits
  unterstützten Actor Values (Resistenzen, Regen-Raten). Move Speed läuft schon
  über direktes `kSpeedMult`-Setzen; Attack Speed bleibt bewusst außen vor
  (Animations-/Mod-Konflikte, vom User selbst so eingeordnet).

---

## SkyrimNet — Ausbau der AI-NPC-Integration

**Status:** 🅿️ Geparkt — MVP steht (Push von Segen/Milestones als World-Knowledge,
Commit `87ab76f`), erst nach Tester-Verifikation weiter. Reihenfolge unten.

**Tier 1 — billig, bleibt skriptfrei (baut auf dem Push-MVP auf):**
- **Otherworlder-Persona:** beim Reincarnate eine Spieler-Bio pushen (aus anderer Welt,
  erinnert sich an eine moderne Welt) → Fish-out-of-water-Dialoge in beide Richtungen.
- **Past-Life-Memories:** Erinnerungsfetzen aus dem alten Leben als Memories seeden.
- **Awakening als Moment:** beim Level-Up-Flourish ein Short-Lived-Event → NPCs reagieren
  sofort auf das Licht/den Machtschub. Dormant-Erwachen zusätzlich hoch-salient + evtl.
  kurzer Voice-Effekt.

**Tier 2 — braucht Decorator (kleiner Papyrus-Glue, bricht Skriptfreiheit):**
- **Live-Aura-Decorator:** NPCs kennen den *aktuellen* Zustand in jedem Gespräch (Tier,
  dormant/erwacht, letzte Titel), nicht nur vergangene Events.
- **Legende skaliert:** NPC-Gerede wird ehrfürchtiger mit Milestone-Zahl/höchstem Titel.

**Tier 3 — Marquee, groß/riskant:**
- **Das „[SYSTEM]" als LLM-Stimme:** kontextbezogene System-Meldungen statt fester Strings,
  personalisierte System-Direktiven. Kern der Isekai-Fantasie, aber Tonkontrolle +
  LLM-Latenz/Kosten.

**Bewusst nicht:** System-Actions als Ersatz für echte Quests (überschneidet sich mit
Quest-Mods; Isekai lebt eher vom *Reagieren* der Welt).

Details + offene Verifikationspunkte (DLL-Name, `SkyrimNetApi`-Signaturen) siehe
`src/SkyrimNet.cpp` und `README`.

---

## Skill-tree expansion — "more to spend points on" (user feedback)

**Status:** 🅿️ Roadmap — evaluated, feasibility confirmed against actual actor values,
not yet built (deferred until the VR / perf / storage fixes are tested & released).
**Origin:** player feedback — loves the concept and the UI, wants many more nodes so you
can get "really, really OP"; noted there's fire/frost resist but no shock resist.
(Expands the earlier "Noch mehr Knoten-Varietät" note above with concrete effects.)

### Tier 1 — easy wins (mirror the existing repeatable utility nodes: new node + AV)

All backed by real actor values, applied exactly like Fleet of Foot / Enduring Vigor:
- **Shock Resistance** — `kResistShock` (42). Fills the obvious gap next to Emberskin /
  Frostskin.
- **General Magic Resistance** — `kResistMagic` (44). Currently only the Warding milestone
  passive; add a buyable node.
- **Magic Absorption** — `kAbsorbChance` (83). Percent; cap it (e.g. 50–80%).
- **Physical Resistance / Armor** — `kDamageResist` (39). Armor-rating points.
- **Health / Magicka / Stamina Regen** — `kHealRateMult` / `kMagickaRateMult` /
  `kStaminaRateMult` (155–157), base 100 like move speed.

Design: repeatable, NORMAL tier, small per-rank amounts with sane caps so "OP" is earned
over many System Points, not handed over. Same respec handling as the other stat nodes.

### Tier 2 — harder, no clean global actor value

- **Spell Damage** — Skyrim has no global spell-damage multiplier AV; needs a perk or a
  magic-effect route. Meaningful effort.
- **Melee Damage** — same problem; weapon-type-specific (sword / mace / dagger / **bound**)
  is much harder (perks per type). The reporter plays a bound-weapon build (Biggie Traits).
- **Carry multiple standing-stone bonuses at once** — managing ability spells; complex.

### Separate, larger features

- **Gamepad support for the UI.** PrismaUI 1.5 added gamepad input; the built-in ImGui path
  would need its own controller mapping. Its own effort; ties into the VR work.
- **Modularity / configurable values.** Expose node magnitudes and costs via the ini so
  players can tune them. Medium effort; pairs naturally with Tier 1.

### Recommended order

Tier 1 batch first (biggest value per effort, low risk, exactly the request), then
modularity, then the damage nodes / gamepad as their own tracks.
