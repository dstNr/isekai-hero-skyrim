> ⚠️ **Legacy / archived.** This documents the old Papyrus implementation of the mod
> (tag `papyrus-v1.0`), which has been superseded by the native SKSE C++ version. Kept
> for reference only.

# 🚀 Planned features & ideas

This file documents planned features and ideas for future versions of the Isekai Hero mod.

---

## 🎯 Feature: Main-quest integration with System notifications

**Status:** ✅ Core implemented (`IsekaiQuestTracker.psc`, compiles) — CK wiring needed
**Priority:** High
**Target version:** v2.1

> **Implemented:** [IsekaiQuestTracker.psc](Scripts/Source/IsekaiQuestTracker.psc) watches the
> 12 main quests (MQ101–MQ206), shows a System box on completion and grants perk points +
> flavor titles. The MCM shows progress + a toggle. **CK setup see
> [CREATION_KIT_GUIDE.md](CREATION_KIT_GUIDE.md).**
> Still open from the concept below: faction/DLC quests, a switchable title system with
> bonuses, daily quests.

### Concept

As in isekai anime (Solo Leveling, Re:Monster, etc.), the System shows progress
notifications at important quest milestones with matching rewards.

**Example from Solo Leveling:**
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

### Implementation ideas

#### 1. Quest-tracking system

**New script:** `IsekaiQuestTracker.psc`

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

#### 2. Quest-rewards system

**Structure for rewards:**

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

**Example rewards:**

| Quest | Reward | Value |
|-------|-----------|------|
| **Unbound** (escape Helgen) | "Survivor" title, +10 perks | Surviving the execution |
| **Before the Storm** (Riverwood) | +5 perks, +20 carry weight | First steps in Skyrim |
| **Bleak Falls Barrow** | "Tomb Raider" title, +15 perks, -5% shout cooldown | First draugr defeated |
| **Dragon Rising** (first dragon) | "Dragon Slayer" title, +50 perks, -10% shout cooldown | First dragon-soul absorption |
| **The Way of the Voice** | "Voice Wielder" title, +25 perks, -15% shout cooldown | Met the Greybeards |
| **Horn of Jurgen Windcaller** | +30 perks, -20% shout cooldown, +10% Magicka | Full Way of the Voice |
| **Diplomatic Immunity** | "Spy" title, +40 perks | Survived the Thalmor embassy |
| **Alduin's Bane** | "Time Walker" title, +100 perks, -25% shout cooldown | Survived the time jump |
| **Dragonslayer** (final quest) | "World Savior" title, +500 perks, -50% shout cooldown, +25% magic resist | Alduin defeated |

**Faction rewards:**

| Faction | Masterwork reward |
|----------|----------------------|
| **Companions** (Glory of the Dead) | "Harbinger's Heir" title, +100 perks, +20% two-handed damage |
| **College of Winterhold** (Eye of Magnus) | "Arch-Mage" title, +100 perks, -20% spell cost, +100 Magicka |
| **Thieves Guild** (Darkness Returns) | "Nightingale" title, +100 perks, +20% pickpocket/lockpick, invisibility perk |
| **Dark Brotherhood** (Hail Sithis!) | "Listener" title, +100 perks, +20% sneak-attack damage |
| **Civil War** (Siege) | "War Hero" title, +150 perks, +20% one-handed/block |
| **Dawnguard** (Kindred Judgment) | "Vampire Hunter" / "Vampire Lord" title, +200 perks |
| **Dragonborn** (Summit of Apocrypha) | "Miraak's Bane" title, +300 perks, all shouts cost 50% less |

#### 3. System notifications

**Visual design:**

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

**Sound design:**
- Level-up sound for small quests
- Dragon-soul-absorb sound for large quests
- Unique sound for the final quests

#### 4. Optional: System quests (daily/weekly)

As in Solo Leveling's "Daily Quests":

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

**Possible daily quests:**
- "Slay 10 Creatures" → +5 perks
- "Clear 1 Dungeon" → +10 perks
- "Craft 5 Items" → +5 perks, +10 Smithing
- "Read 3 Skill Books" → +5 perks, +20% XP for 1 hour
- "Travel 10,000 Steps" → +5 perks, +20 Stamina

#### 5. Title system

**Unlockable titles with bonuses:**

| Title | Requirement | Bonus |
|-------|-------------|-------|
| "Survivor" | Escape Helgen | +10 Health |
| "Dragon Slayer" | Defeat the first dragon | +5% dragon damage |
| "Tomb Raider" | Clear 5 draugr barrows | +10% treasure find rate |
| "Dragonborn" | Complete Way of the Voice | -10% shout cooldown |
| "Arch-Mage" | College questline | -10% spell cost |
| "Listener" | Dark Brotherhood | +10% sneak damage |
| "Nightingale" | Thieves Guild | +20 carry weight |
| "Harbinger" | Companions | +10% two-handed |
| "War Hero" | Civil War | +10% one-handed/block |
| "Vampire Hunter" | Dawnguard (Hunter) | +25% vampire damage |
| "Vampire Lord" | Dawnguard (Vampire) | Vampire powers stronger |
| "Miraak's Bane" | Dragonborn DLC | All shouts +20% effect |
| "World Savior" | Main quest complete | +50 all attributes |

**Titles can be switched (only one active):**
- MCM menu for title selection
- The active title grants its bonus
- Inactive titles are collected but not active

### Technical implementation

#### Phase 1: Base system
1. Create `IsekaiQuestTracker.psc`
2. Watch quest stages (RegisterForUpdate or events)
3. Message boxes for quest completions
4. Distribute rewards

#### Phase 2: Title system
1. Create `IsekaiTitleManager.psc`
2. Titles as global variables or a FormList
3. MCM integration for switching titles
4. Bonus application on switch

#### Phase 3: Daily quests
1. Create `IsekaiDailyQuests.psc`
2. StorageUtil for progress
3. Timer system for reset
4. Random quest selection

### Dependencies

- SKSE64 (for StorageUtil, time tracking)
- SkyUI (for MCM, title selection)
- Optional: UIExtensions (better menus)

### Save compatibility

- Should be compatible with existing saves
- Quest progress is detected
- Already-completed quests → retroactive reward optional

---

## 📝 More ideas

### High priority
- [ ] **Dimensional Storage** — inventory access anywhere (like an Ender Chest)
- [ ] **System Shop** — trade perk points for items
- [ ] **Skill Respec** — MCM menu for a full re-skill

### Medium priority
- [ ] **Isekai Companions** — followers from other worlds
- [ ] **Unique Equipment** — scales with player level
- [ ] **Boss Scaling** — enemies scale with power level (optional)

### Low priority / nice to have
- [ ] **Achievements** — Steam-style achievements in-game
- [ ] **Statistics** — playtime, kills, etc. in the MCM
- [ ] **New Game+** — restart with full power level

---

**Got an idea?** Open an issue or suggest it here!
