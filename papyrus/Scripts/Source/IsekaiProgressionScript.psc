; IsekaiProgressionScript.psc
; Isekai Quest System - Progression instead of instant power
; "The System rewards growth, not just potential"

Scriptname IsekaiProgressionScript extends Quest

; ============================================
; REFERENCES
; ============================================

IsekaiIntroQuest Property MainQuest Auto
IsekaiPowerScript Property PowerScript Auto

; ============================================
; PROGRESSION STATE
; ============================================

; Achievement tracking
Bool Property FirstDragonKilled = False Auto Hidden
Bool Property FirstWordLearned = False Auto Hidden
Bool Property ReachedLevel50 = False Auto Hidden
Bool Property ReachedLevel100 = False Auto Hidden
Bool Property JoinedFaction = False Auto Hidden
Bool Property ClearedDungeon = False Auto Hidden

; Milestone rewards
Int Property DragonsKilled = 0 Auto Hidden
Int Property DungeonsCleared = 0 Auto Hidden
Int Property WordsLearned = 0 Auto Hidden
Int Property TotalPerksEarned = 0 Auto Hidden

; ============================================
; SYSTEM MILESTONES
; ============================================
;
; Skyrim's Papyrus compiler does NOT support Structs (a Fallout 4 feature),
; so milestones are stored as parallel arrays indexed 0..MILESTONE_COUNT-1.

Int Property MILESTONE_COUNT = 10 AutoReadOnly

String[] Property MilestoneNames Auto Hidden
String[] Property MilestoneDescriptions Auto Hidden
Int[] Property MilestoneRewardPerks Auto Hidden
Bool[] Property MilestoneCompleted Auto Hidden

; ============================================
; INITIALIZATION
; ============================================

Event OnInit()
    InitializeMilestones()
    RegisterForEvents()
EndEvent

Function InitializeMilestones()
    MilestoneNames = new String[10]
    MilestoneDescriptions = new String[10]
    MilestoneRewardPerks = new Int[10]
    MilestoneCompleted = new Bool[10] ; defaults to False

    ; Milestone 0: First Steps
    MilestoneNames[0] = "First Steps"
    MilestoneDescriptions[0] = "Begin your journey in Nirn"
    MilestoneRewardPerks[0] = 5

    ; Milestone 1: Word of Power
    MilestoneNames[1] = "Voice of the Dragonborn"
    MilestoneDescriptions[1] = "Learn your first Word of Power"
    MilestoneRewardPerks[1] = 10

    ; Milestone 2: Dragon Slayer
    MilestoneNames[2] = "Dragon Slayer"
    MilestoneDescriptions[2] = "Defeat your first Dragon"
    MilestoneRewardPerks[2] = 15

    ; Milestone 3: Rising Power (Level 25)
    MilestoneNames[3] = "Rising Power"
    MilestoneDescriptions[3] = "Reach Level 25"
    MilestoneRewardPerks[3] = 10

    ; Milestone 4: Dungeon Delver
    MilestoneNames[4] = "Dungeon Delver"
    MilestoneDescriptions[4] = "Clear 5 dungeons"
    MilestoneRewardPerks[4] = 10

    ; Milestone 5: Faction Member
    MilestoneNames[5] = "Faction Initiate"
    MilestoneDescriptions[5] = "Join a major faction"
    MilestoneRewardPerks[5] = 10

    ; Milestone 6: Adept (Level 50)
    MilestoneNames[6] = "Adept"
    MilestoneDescriptions[6] = "Reach Level 50"
    MilestoneRewardPerks[6] = 20

    ; Milestone 7: Dragonborn
    MilestoneNames[7] = "Dragonborn"
    MilestoneDescriptions[7] = "Learn 10 Words of Power"
    MilestoneRewardPerks[7] = 25

    ; Milestone 8: Master (Level 100)
    MilestoneNames[8] = "Master"
    MilestoneDescriptions[8] = "Reach Level 100"
    MilestoneRewardPerks[8] = 50

    ; Milestone 9: Legend
    MilestoneNames[9] = "Legend"
    MilestoneDescriptions[9] = "Defeat 10 Dragons"
    MilestoneRewardPerks[9] = 100
EndFunction

Function RegisterForEvents()
    ; RegisterForTrackedStatsEvent() takes NO arguments - it registers for ALL
    ; stat changes. OnTrackedStatsEvent then filters by stat name
    ; ("Dragon Souls Collected", "Dungeons Cleared", "Words Of Power Learned").
    RegisterForTrackedStatsEvent()

    ; Register for level up
    RegisterForSingleUpdateGameTime(1.0)
EndFunction

; ============================================
; EVENT HANDLERS
; ============================================

Event OnTrackedStatsEvent(string statFilter, int statValue)
    If statFilter == "Dragon Souls Collected"
        DragonsKilled = statValue
        CheckDragonMilestones()
    ElseIf statFilter == "Dungeons Cleared"
        DungeonsCleared = statValue
        CheckDungeonMilestones()
    ElseIf statFilter == "Words Of Power Learned"
        WordsLearned = statValue
        CheckWordMilestones()
    EndIf
EndEvent

Event OnUpdateGameTime()
    CheckLevelMilestones()
    RegisterForSingleUpdateGameTime(1.0)
EndEvent

; ============================================
; MILESTONE CHECKS
; ============================================

Function CheckDragonMilestones()
    ; First Dragon
    If !MilestoneCompleted[2] && DragonsKilled >= 1
        CompleteMilestone(2)
    EndIf

    ; Legend (10 Dragons)
    If !MilestoneCompleted[9] && DragonsKilled >= 10
        CompleteMilestone(9)
    EndIf
EndFunction

Function CheckDungeonMilestones()
    ; Dungeon Delver (5 dungeons)
    If !MilestoneCompleted[4] && DungeonsCleared >= 5
        CompleteMilestone(4)
    EndIf
EndFunction

Function CheckWordMilestones()
    ; First Word
    If !MilestoneCompleted[1] && WordsLearned >= 1
        CompleteMilestone(1)
    EndIf

    ; Dragonborn (10 words)
    If !MilestoneCompleted[7] && WordsLearned >= 10
        CompleteMilestone(7)
    EndIf
EndFunction

Function CheckLevelMilestones()
    Actor player = Game.GetPlayer()
    Int currentLevel = player.GetLevel()

    ; Rising Power (Level 25)
    If !MilestoneCompleted[3] && currentLevel >= 25
        CompleteMilestone(3)
    EndIf

    ; Adept (Level 50)
    If !MilestoneCompleted[6] && currentLevel >= 50
        CompleteMilestone(6)
    EndIf

    ; Master (Level 100)
    If !MilestoneCompleted[8] && currentLevel >= 100
        CompleteMilestone(8)
    EndIf
EndFunction

; ============================================
; MILESTONE COMPLETION
; ============================================

Function CompleteMilestone(Int milestoneIndex)
    If milestoneIndex < 0 || milestoneIndex >= MilestoneNames.Length
        Return
    EndIf

    If MilestoneCompleted[milestoneIndex]
        Return
    EndIf

    ; Mark as completed
    MilestoneCompleted[milestoneIndex] = True

    ; Grant rewards
    Int reward = MilestoneRewardPerks[milestoneIndex]
    If reward > 0
        Game.AddPerkPoints(reward)
        TotalPerksEarned += reward
    EndIf

    ; Show completion message
    ShowMilestoneComplete(milestoneIndex)
EndFunction

Function ShowMilestoneComplete(Int index)
    String msgText = "╔══════════════════════════════════════╗\n"
    msgText += "║     SYSTEM MILESTONE ACHIEVED!       ║\n"
    msgText += "╠══════════════════════════════════════╣\n"
    msgText += "  " + MilestoneNames[index] + "\n"
    msgText += "  " + MilestoneDescriptions[index] + "\n"
    msgText += "╠══════════════════════════════════════╣\n"
    msgText += "  REWARD: " + MilestoneRewardPerks[index] + " Perk Points\n"
    msgText += "╚══════════════════════════════════════╝"

    Debug.MessageBox(msgText)
    Debug.Notification("[SYSTEM] Milestone: " + MilestoneNames[index])

    ; Play sound
    Sound rewardSound = Game.GetFormFromFile(0x0003C5A0, "Skyrim.esm") as Sound ; UIQuestComplete
    If rewardSound
        rewardSound.Play(Game.GetPlayer())
    EndIf
EndFunction

; ============================================
; FACTION TRACKING
; ============================================

Function OnFactionJoined(String factionName)
    If !MilestoneCompleted[5]
        CompleteMilestone(5)
    EndIf
EndFunction

; ============================================
; STATUS QUERIES (FOR MCM)
; ============================================

Int Function GetCompletedMilestoneCount()
    Int count = 0
    Int i = 0
    While i < MilestoneCompleted.Length
        If MilestoneCompleted[i]
            count += 1
        EndIf
        i += 1
    EndWhile
    Return count
EndFunction

Int Function GetTotalPossiblePerks()
    Int total = 0
    Int i = 0
    While i < MilestoneRewardPerks.Length
        total += MilestoneRewardPerks[i]
        i += 1
    EndWhile
    Return total
EndFunction

String Function GetMilestoneStatus(Int index)
    If index < 0 || index >= MilestoneNames.Length
        Return "Invalid"
    EndIf

    If MilestoneCompleted[index]
        Return "✓ " + MilestoneNames[index]
    Else
        Return "○ " + MilestoneNames[index]
    EndIf
EndFunction

; ============================================
; INITIAL STARTUP BONUS
; ============================================

Function GrantStartingBonus(Int powerLevel)
    ; Grant first steps milestone immediately
    CompleteMilestone(0)

    ; Power level affects starting perks
    If powerLevel == 1 ; Hero
        ; Already gets 50 from PowerScript, give some bonus
        Debug.Notification("[SYSTEM] Hero bonus: Additional perks unlocked through milestones")
    ElseIf powerLevel == 2 ; Ascended
        ; Already gets 500, but milestones give even more
        Debug.Notification("[SYSTEM] Ascended bonus: Unlimited growth potential")
    EndIf
EndFunction
