; IsekaiQuestTracker.psc
; Main Quest Integration - Solo-Leveling style System rewards.
; "The System rewards those who shape the fate of Nirn."
;
; Watches the vanilla main questline. When a tracked quest completes, the
; System shows a notification box and grants Perk Points (and a flavor title).
;
; NOTE: Skyrim's Papyrus has no Structs, so reward data is held in parallel
; arrays indexed in lockstep with the MainQuests array.

Scriptname IsekaiQuestTracker extends Quest

; ============================================
; CONFIG (set in CK / MCM)
; ============================================

; Fill MainQuests in the Creation Kit with the vanilla main-quest stages
; in EXACTLY this order (see QUEST_COUNT entries below):
;   0  MQ101  Unbound
;   1  MQ102  Before the Storm
;   2  MQ103  Bleak Falls Barrow
;   3  MQ104  Dragon Rising
;   4  MQ105  The Way of the Voice
;   5  MQ106  The Horn of Jurgen Windcaller
;   6  MQ201  A Blade in the Dark
;   7  MQ202  Diplomatic Immunity
;   8  MQ203  A Cornered Rat
;   9  MQ204  Alduin's Wall
;   10 MQ205  The Fallen
;   11 MQ206  Dragonslayer
Quest[] Property MainQuests Auto

Bool Property EnableQuestRewards = True Auto
Bool Property RetroactiveRewards = False Auto
{If True, quests already completed when this mod is installed are rewarded on
 the first scan. If False (default) they are silently marked as already done.}

; ============================================
; STATE
; ============================================

Bool[] Property Rewarded Auto Hidden
Int Property TotalQuestPerksEarned = 0 Auto Hidden
Int Property QuestsRewarded = 0 Auto Hidden

Int Property QUEST_COUNT = 12 AutoReadOnly

; Parallel reward data (initialized in code, index matches MainQuests)
String[] QuestLabels
String[] QuestTitles
Int[] QuestPerks

; ============================================
; INITIALIZATION
; ============================================

Event OnInit()
    InitRewardData()
    Rewarded = new Bool[12] ; defaults to False

    If !RetroactiveRewards
        ; Don't flood existing characters: treat already-completed quests as done.
        PreMarkCompleted()
    EndIf

    ; Half a game-hour cadence is plenty for quest-completion checks.
    RegisterForSingleUpdateGameTime(0.5)
EndEvent

Function InitRewardData()
    QuestLabels = new String[12]
    QuestTitles = new String[12]
    QuestPerks = new Int[12]

    SetReward(0,  "Unbound",                      "Survivor",        10)
    SetReward(1,  "Before the Storm",             "",                 5)
    SetReward(2,  "Bleak Falls Barrow",           "Tomb Raider",     15)
    SetReward(3,  "Dragon Rising",                "Dragon Slayer",   50)
    SetReward(4,  "The Way of the Voice",         "Voice Wielder",   25)
    SetReward(5,  "The Horn of Jurgen Windcaller","",                30)
    SetReward(6,  "A Blade in the Dark",          "",                20)
    SetReward(7,  "Diplomatic Immunity",          "Spy",             40)
    SetReward(8,  "A Cornered Rat",               "",                25)
    SetReward(9,  "Alduin's Wall",                "Time Reader",     30)
    SetReward(10, "The Fallen",                   "Dragon Tamer",    50)
    SetReward(11, "Dragonslayer",                 "World Savior",   500)
EndFunction

Function SetReward(Int index, String label, String title, Int perks)
    QuestLabels[index] = label
    QuestTitles[index] = title
    QuestPerks[index] = perks
EndFunction

; Mark any quest already completed at install time as rewarded (no reward given).
Function PreMarkCompleted()
    If !MainQuests
        Return
    EndIf
    Int n = ScanLength()
    Int i = 0
    While i < n
        Quest q = MainQuests[i]
        If q && q.IsCompleted()
            Rewarded[i] = True
        EndIf
        i += 1
    EndWhile
EndFunction

; ============================================
; SCAN LOOP
; ============================================

Event OnUpdateGameTime()
    If EnableQuestRewards
        ScanQuests()
    EndIf
    RegisterForSingleUpdateGameTime(0.5)
EndEvent

Function ScanQuests()
    If !MainQuests
        Return
    EndIf
    Int n = ScanLength()
    Int i = 0
    While i < n
        Quest q = MainQuests[i]
        If q && !Rewarded[i] && q.IsCompleted()
            GrantQuestReward(i)
        EndIf
        i += 1
    EndWhile
EndFunction

; Never index past either the CK-filled array or our reward data.
Int Function ScanLength()
    Int n = MainQuests.Length
    If n > Rewarded.Length
        n = Rewarded.Length
    EndIf
    If n > QuestPerks.Length
        n = QuestPerks.Length
    EndIf
    Return n
EndFunction

; ============================================
; REWARD
; ============================================

Function GrantQuestReward(Int index)
    Rewarded[index] = True
    QuestsRewarded += 1

    Int perks = QuestPerks[index]
    If perks > 0
        Game.AddPerkPoints(perks)
        TotalQuestPerksEarned += perks
    EndIf

    ShowQuestRewardBox(index, perks)
    PlayRewardSound(index)
EndFunction

Function ShowQuestRewardBox(Int index, Int perks)
    String msgText = "╔══════════════════════════════════════╗\n"
    msgText += "║         QUEST COMPLETED!             ║\n"
    msgText += "╠══════════════════════════════════════╣\n"
    msgText += "  \"" + QuestLabels[index] + "\"\n\n"
    msgText += "  ┌─ REWARDS ──────────────────────┐\n"
    msgText += "  │  +" + perks + " Perk Points\n"

    String title = QuestTitles[index]
    If title != ""
        msgText += "  │  Title: \"" + title + "\"\n"
    EndIf

    msgText += "  └────────────────────────────────┘\n"
    msgText += "╚══════════════════════════════════════╝"

    Debug.MessageBox(msgText)
    Debug.Notification("[SYSTEM] Quest complete: " + QuestLabels[index] + " (+" + perks + " Perks)")
EndFunction

Function PlayRewardSound(Int index)
    Int soundID = 0x0003C5A0 ; UIQuestComplete
    If index == (QUEST_COUNT - 1)
        soundID = 0x000B6435 ; DRSoulCaptureSequenceA (epic finale)
    EndIf
    Sound rewardSound = Game.GetFormFromFile(soundID, "Skyrim.esm") as Sound
    If rewardSound
        rewardSound.Play(Game.GetPlayer())
    EndIf
EndFunction

; ============================================
; STATUS QUERIES (FOR MCM)
; ============================================

Int Function GetQuestsRewarded()
    Return QuestsRewarded
EndFunction

Int Function GetTotalQuestPerks()
    Return TotalQuestPerksEarned
EndFunction

; ============================================
; MANUAL / DEBUG
; ============================================

Function ForceScan()
    ScanQuests()
EndFunction

Function ResetRewards()
    Rewarded = new Bool[12]
    TotalQuestPerksEarned = 0
    QuestsRewarded = 0
    Debug.Notification("[SYSTEM] Quest reward tracking reset.")
EndFunction
