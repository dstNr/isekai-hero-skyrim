# 🚀 Geplante Features & Ideen

Diese Datei dokumentiert geplante Features und Ideen für zukünftige Versionen des Isekai Hero Mods.

---

## 🎯 Feature: Main Quest Integration mit System-Benachrichtigungen

**Status:** 💡 Idee / In Planung  
**Priorität:** Hoch  
**Zielversion:** v2.1 oder v3.0

### Konzept

Wie in Isekai-Anime (Solo Leveling, Re:Monster, etc.) zeigt das System Fortschritts-Benachrichtigungen bei wichtigen Quest-Meilensteinen mit passenden Belohnungen.

**Beispiel aus Solo Leveling:**
```
╔══════════════════════════════════════╗
║     QUEST COMPLETED!                 ║
╠══════════════════════════════════════╣
  "The Call of High Hrothgar"
  
  ✓ Survived the trek to High Hrothgar
  ✓ Met the Greybeards
  ✓ Learned Unrelenting Force
  
  REWARDS:
  • +50 Perk Points
  • +10% Shout Cooldown Reduction
  • Title: "Dragonborn Initiate"
╚══════════════════════════════════════╝
```

### Implementierungsideen

#### 1. Quest-Tracking System

**Neues Script:** `IsekaiQuestTracker.psc`

```papyrus
Scriptname IsekaiQuestTracker extends Quest

; Track major questlines
Quest Property MQ101 Auto ; Unbound
Quest Property MQ102 Auto ; Before the Storm
Quest Property MQ103 Auto ; Bleak Falls Barrow
Quest Property MQ104 Auto ; Dragon Rising
Quest Property MQ105 Auto ; The Way of the Voice
Quest Property MQ106 Auto ; Horn of Jurgen Windcaller
Quest Property MQ201 Auto ; Diplomatic Immunity
Quest Property MQ202 Auto ; A Cornered Rat
Quest Property MQ203 Auto ; Alduin's Wall
Quest Property MQ204 Auto ; Elder Knowledge
Quest Property MQ205 Auto ; Alduin's Bane
Quest Property MQ206 Auto ; Dragonslayer

; Faction Quests
Quest Property C00 Auto ; Companions - Take Up Arms
Quest Property C01 Auto ; Companions - Proving Honor
Quest Property C02 Auto ; Companions - The Silver Hand
Quest Property C03 Auto ; Companions - Blood's Honor
Quest Property C04 Auto ; Companions - Purity of Revenge
Quest Property C05 Auto ; Companions - Glory of the Dead

Quest Property MG01 Auto ; College - First Lessons
Quest Property MG02 Auto ; College - Under Saarthal
Quest Property MG03 Auto ; College - Hitting the Books
Quest Property MG04 Auto ; College - Good Intentions
Quest Property MG05 Auto ; College - Revealing the Unseen
Quest Property MG06 Auto ; College - Containment
Quest Property MG07 Auto ; College - The Staff of Magnus
Quest Property MG08 Auto ; College - The Eye of Magnus

Quest Property TG01 Auto ; Thieves Guild - A Chance Arrangement
Quest Property TG02 Auto ; Thieves Guild - Taking Care of Business
Quest Property TG03 Auto ; Thieves Guild - Loud and Clear
Quest Property TG04 Auto ; Thieves Guild - Dampened Spirits
Quest Property TG05 Auto ; Thieves Guild - Scoundrel's Folly
Quest Property TG06 Auto ; Thieves Guild - Speaking With Silence
Quest Property TG07 Auto ; Thieves Guild - Hard Answers
Quest Property TG08 Auto ; Thieves Guild - The Pursuit
Quest Property TG09 Auto ; Thieves Guild - Trinity Restored
Quest Property TG10 Auto ; Thieves Guild - Blindsighted
Quest Property TG11 Auto ; Thieves Guild - Darkness Returns

Quest Property DB01 Auto ; Dark Brotherhood - Innocence Lost
Quest Property DB02 Auto ; Dark Brotherhood - With Friends Like These
Quest Property DB03 Auto ; Dark Brotherhood - Sanctuary
Quest Property DB04 Auto ; Dark Brotherhood - Mourning Never Comes
Quest Property DB05 Auto ; Dark Brotherhood - Whispers in the Dark
Quest Property DB06 Auto ; Dark Brotherhood - The Silence Has Been Broken
Quest Property DB07 Auto ; Dark Brotherhood - Bound Until Death
Quest Property DB08 Auto ; Dark Brotherhood - Breaching Security
Quest Property DB09 Auto ; Dark Brotherhood - The Cure for Madness
Quest Property DB10 Auto ; Dark Brotherhood - Recipe for Disaster
Quest Property DB11 Auto ; Dark Brotherhood - To Kill an Empire
Quest Property DB12 Auto ; Dark Brotherhood - Death Incarnate
Quest Property DB13 Auto ; Dark Brotherhood - Hail Sithis!

; Civil War
Quest Property CW01A Auto ; Joining the Stormcloaks
Quest Property CW01B Auto ; Joining the Legion
Quest Property CW02A Auto ; The Jagged Crown (Stormcloaks)
Quest Property CW02B Auto ; The Jagged Crown (Imperial)
Quest Property CW03 Auto ; Message to Whiterun
Quest Property CW04 Auto ; Battle for Whiterun
Quest Property CW05 Auto ; Liberation of Skyrim / Reunification
Quest Property CWSiege Auto ; Final Siege

; DLC Quests
Quest Property DLC1MQ01 Auto ; Dawnguard - Dawnguard
Quest Property DLC1MQ02 Auto ; Dawnguard - Awakening
Quest Property DLC1MQ03 Auto ; Dawnguard - Bloodline
Quest Property DLC1MQ04 Auto ; Dawnguard - A New Order
Quest Property DLC1MQ05 Auto ; Dawnguard - Prophet
Quest Property DLC1MQ06 Auto ; Dawnguard - Chasing Echoes
Quest Property DLC1MQ07 Auto ; Dawnguard - Beyond Death
Quest Property DLC1MQ08 Auto ; Dawnguard - Unseen Visions
Quest Property DLC1MQ09 Auto ; Dawnguard - Touching the Sky
Quest Property DLC1MQ10 Auto ; Dawnguard - Kindred Judgment

Quest Property DLC2MQ01 Auto ; Dragonborn - Dragonborn
Quest Property DLC2MQ02 Auto ; Dragonborn - The Temple of Miraak
Quest Property DLC2MQ03 Auto ; Dragonborn - The Fate of the Skaal
Quest Property DLC2MQ04 Auto ; Dragonborn - Cleansing the Stones
Quest Property DLC2MQ05 Auto ; Dragonborn - The Path of Knowledge
Quest Property DLC2MQ06 Auto ; Dragonborn - The Gardener of Men
Quest Property DLC2MQ07 Auto ; Dragonborn - At the Summit of Apocrypha
```

#### 2. Quest-Rewards System

**Struktur für Belohnungen:**

```papyrus
Struct QuestReward
    String QuestName
    String CompletionText
    Int PerkPoints
    Float ShoutCooldownReduction
    Float MagicCostReduction
    Float DamageResistance
    Float CarryWeightBonus
    String TitleUnlocked
    Form[] ItemsRewarded
EndStruct
```

**Beispiel-Belohnungen:**

| Quest | Belohnung | Wert |
|-------|-----------|------|
| **Unbound** (Helgen entkommen) | "Survivor" Titel, +10 Perks | Überleben der Hinrichtung |
| **Before the Storm** (Riverwood) | +5 Perks, +20 Traglast | Erste Schritte in Skyrim |
| **Bleak Falls Barrow** | "Tomb Raider" Titel, +15 Perks, -5% Shout Cooldown | Erster Draugr besiegt |
| **Dragon Rising** (erster Drache) | "Dragon Slayer" Titel, +50 Perks, -10% Shout Cooldown | Erster Drachenseelen-Absorbtion |
| **The Way of the Voice** | "Voice Wielder" Titel, +25 Perks, -15% Shout Cooldown | Greybeards getroffen |
| **Horn of Jurgen Windcaller** | +30 Perks, -20% Shout Cooldown, +10% Magicka | Komplettes Way of the Voice |
| **Diplomatic Immunity** | "Spy" Titel, +40 Perks | Thalmor-Embassy überlebt |
| **Alduin's Bane** | "Time Walker" Titel, +100 Perks, -25% Shout Cooldown | Zeitsprung überlebt |
| **Dragonslayer** (finale Quest) | "World Savior" Titel, +500 Perks, -50% Shout Cooldown, +25% Magic Resist | Alduin besiegt |

**Fraktions-Belohnungen:**

| Fraktion | Meisterwerk-Belohnung |
|----------|----------------------|
| **Companions** (Glory of the Dead) | "Harbingers Heir" Titel, +100 Perks, +20% Zweihand-Schaden |
| **College of Winterhold** (Eye of Magnus) | "Arch-Mage" Titel, +100 Perks, -20% Spell Cost, +100 Magicka |
| **Thieves Guild** (Darkness Returns) | "Nightengale" Titel, +100 Perks, +20% Pickpocket/Lockpick, Unsichtbarkeit-Perk |
| **Dark Brotherhood** (Hail Sithis!) | "Listener" Titel, +100 Perks, +20% Schleich-Angriff-Schaden |
| **Civil War** (Siege) | "War Hero" Titel, +150 Perks, +20% Einhand/Block |
| **Dawnguard** (Kindred Judgment) | "Vampire Hunter" / "Vampire Lord" Titel, +200 Perks |
| **Dragonborn** (Summit of Apocrypha) | "Miraaks Bane" Titel, +300 Perks, Alle Schreie kosten 50% weniger |

#### 3. System-Benachrichtigungen

**Visuelles Design:**

```
╔══════════════════════════════════════════════════╗
║  ⭐ SYSTEM NOTIFICATION ⭐                        ║
╠══════════════════════════════════════════════════╣
║                                                  ║
║  QUEST COMPLETED                                 ║
║  ═══════════════════                             ║
║  "Dragon Rising"                                 ║
║                                                  ║
║  ✓ Absorbed first dragon soul                   ║
║  ✓ Proven to be Dragonborn                      ║
║                                                  ║
║  ┌─ REWARDS ─────────────────────────────┐      ║
║  │  +50 Perk Points                       │      ║
║  │  -10% Shout Cooldown                   │      ║
║  │  Title: "Dragon Slayer"                │      ║
║  └────────────────────────────────────────┘      ║
║                                                  ║
║  [Press TAB to continue]                         ║
╚══════════════════════════════════════════════════╝
```

**Sound-Design:**
- Level-Up Sound für kleine Quests
- Dragon Soul Absorb Sound für große Quests
- Unique Sound für finale Quests

#### 4. Optional: System-Quests (Daily/Weekly)

Wie in Solo Leveling "Daily Quests":

```
╔══════════════════════════════════════╗
║     DAILY QUEST AVAILABLE            ║
╠══════════════════════════════════════╣
  "Slay 10 Bandits"
  
  Reward: +5 Perk Points
  Bonus: +20 Health (permanent)
  
  Time Remaining: 23:59:59
╚══════════════════════════════════════╝
```

**Mögliche Daily Quests:**
- "Slay 10 Creatures" → +5 Perks
- "Clear 1 Dungeon" → +10 Perks
- "Craft 5 Items" → +5 Perks, +10 Smithing
- "Read 3 Skill Books" → +5 Perks, +20% XP für 1 Stunde
- "Travel 10,000 Steps" → +5 Perks, +20 Stamina

#### 5. Titel-System

**Freischaltbare Titel mit Boni:**

| Titel | Anforderung | Bonus |
|-------|-------------|-------|
| "Survivor" | Helgen entkommen | +10 Health |
| "Dragon Slayer" | Ersten Drachen besiegt | +5% Drachen-Schaden |
| "Tomb Raider" | 5 Draugr-Gräber geleert | +10% Schatz-Fundrate |
| "Dragonborn" | Way of the Voice komplett | -10% Shout Cooldown |
| "Arch-Mage" | College Questline | -10% Spell Cost |
| "Listener" | Dark Brotherhood | +10% Schleich-Schaden |
| "Nightengale" | Thieves Guild | +20 Traglast |
| "Harbinger" | Companions | +10% Zweihand |
| "War Hero" | Civil War | +10% Einhand/Block |
| "Vampire Hunter" | Dawnguard (Hunter) | +25% Vampir-Schaden |
| "Vampire Lord" | Dawnguard (Vampire) | Vampir-Powers stärker |
| "Miraaks Bane" | Dragonborn DLC | Alle Schreie +20% Effekt |
| "World Savior" | Hauptquest komplett | +50 alle Attribute |

**Titel können gewechselt werden (nur einer aktiv):**
- MCM-Menü für Titel-Auswahl
- Aktiver Titel gibt seinen Bonus
- Inaktive Titel sind gesammelt aber nicht aktiv

### Technische Umsetzung

#### Phase 1: Grundsystem
1. `IsekaiQuestTracker.psc` erstellen
2. Quest-Stages überwachen (RegisterForUpdate oder Events)
3. Message Boxen für Quest-Abschlüsse
4. Belohnungen verteilen

#### Phase 2: Titel-System
1. `IsekaiTitleManager.psc` erstellen
2. Titel als global variables oder FormList
3. MCM-Integration für Titel-Wechsel
4. Bonus-Anwendung beim Wechsel

#### Phase 3: Daily Quests
1. `IsekaiDailyQuests.psc` erstellen
2. StorageUtil für Fortschritt
3. Timer-System für Reset
4. Zufällige Quest-Auswahl

### Abhängigkeiten

- SKSE64 (für StorageUtil, Zeit-Tracking)
- SkyUI (für MCM, Titel-Auswahl)
- Optional: UIExtensions (bessere Menüs)

### Speicher-Kompatibilität

- Sollte mit bestehenden Saves kompatibel sein
- Quest-Fortschritt wird erkannt
- Bereits abgeschlossene Quests → Retroaktive Belohnung optional

---

## 📝 Weitere Ideen

### High Priority
- [ ] **Dimensional Storage** - Inventar-Zugriff überall (wie Ender Chest)
- [ ] **System Shop** - Perk Points gegen Items eintauschen
- [ ] **Skill Respec** - MCM-Menü für komplette Neuskillung

### Medium Priority
- [ ] **Isekai Companions** - Begleiter aus anderen Welten
- [ ] **Unique Equipment** - Skaliert mit Spieler-Level
- [ ] **Boss Scaling** - Gegner skalieren mit Power-Level (optional)

### Low Priority / Nice to Have
- [ ] **Achievements** - Steam-style Achievements im Spiel
- [ ] **Statistics** - Spielzeit, Kills, etc. im MCM
- [ ] **New Game+** - Mit vollem Power-Level neu starten

---

**Hast du eine Idee?** Erstelle ein Issue oder schlage sie hier vor!
