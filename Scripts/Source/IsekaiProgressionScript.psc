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

Struct Milestone
    String Name
    String Description
    Int RewardPerks
    Bool Completed
EndStruct

Milestone[] Property Milestones Auto

; ============================================
; INITIALIZATION
; ============================================

Event OnInit()
    InitializeMilestones()
    RegisterForEvents()
EndEvent

Function InitializeMilestones()
    Milestones = new Milestone[10]
    
    ; Milestone 0: First Steps
    Milestones[0].Name = "First Steps"
    Milestones[0].Description = "Begin your journey in Nirn"
    Milestones[0].RewardPerks = 5
    Milestones[0].Completed = False
    
    ; Milestone 1: Word of Power
    Milestones[1].Name = "Voice of the Dragonborn"
    Milestones[1].Description = "Learn your first Word of Power"
    Milestones[1].RewardPerks = 10
    Milestones[1].Completed = False
    
    ; Milestone 2: Dragon Slayer
    Milestones[2].Name = "Dragon Slayer"
    Milestones[2].Description = "Defeat your first Dragon"
    Milestones[2].RewardPerks = 15
    Milestones[2].Completed = False
    
    ; Milestone 3: Rising Power (Level 25)
    Milestones[3].Name = "Rising Power"
    Milestones[3].Description = "Reach Level 25"
    Milestones[3].RewardPerks = 10
    Milestones[3].Completed = False
    
    ; Milestone 4: Dungeon Delver
    Milestones[4].Name = "Dungeon Delver"
    Milestones[4].Description = "Clear 5 dungeons"
    Milestones[4].RewardPerks = 10
    Milestones[4].Completed = False
    
    ; Milestone 5: Faction Member
    Milestones[5].Name = "Faction Initiate"
    Milestones[5].Description = "Join a major faction"
    Milestones[5].RewardPerks = 10
    Milestones[5].Completed = False
    
    ; Milestone 6: Adept (Level 50)
    Milestones[6].Name = "Adept"
    Milestones[6].Description = "Reach Level 50"
    Milestones[6].RewardPerks = 20
    Milestones[6].Completed = False
    
    ; Milestone 7: Dragonborn
    Milestones[7].Name = "Dragonborn"
    Milestones[7].Description = "Learn 10 Words of Power"
    Milestones[7].RewardPerks = 25
    Milestones[7].Completed = False
    
    ; Milestone 8: Master (Level 100)
    Milestones[8].Name = "Master"
    Milestones[8].Description = "Reach Level 100"
    Milestones[8].RewardPerks = 50
    Milestones[8].Completed = False
    
    ; Milestone 9: Legend
    Milestones[9].Name = "Legend"
    Milestones[9].Description = "Defeat 10 Dragons"
    Milestones[9].RewardPerks = 100
    Milestones[9].Completed = False
EndFunction

Function RegisterForEvents()
    ; Register for death events
    RegisterForTrackedStatsEvent("Dragons Killed")
    RegisterForTrackedStatsEvent("Dungeons Cleared")
    RegisterForTrackedStatsEvent("Words of Power Learned")
    
    ; Register for level up
    RegisterForSingleUpdateGameTime(1.0)
EndEvent

; ============================================
; EVENT HANDLERS
; ============================================

Event OnTrackedStatsEvent(string statFilter, int statValue)
    If statFilter == "Dragons Killed"
        DragonsKilled = statValue
        CheckDragonMilestones()
    ElseIf statFilter == "Dungeons Cleared"
        DungeonsCleared = statValue
        CheckDungeonMilestones()
    ElseIf statFilter == "Words of Power Learned"
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
    If !Milestones[2].Completed && DragonsKilled >= 1
        CompleteMilestone(2)
    EndIf
    
    ; Legend (10 Dragons)
    If !Milestones[9].Completed && DragonsKilled >= 10
        CompleteMilestone(9)
    EndIf
EndFunction

Function CheckDungeonMilestones()
    ; Dungeon Delver (5 dungeons)
    If !Milestones[4].Completed && DungeonsCleared >= 5
        CompleteMilestone(4)
    EndIf
EndFunction

Function CheckWordMilestones()
    ; First Word
    If !Milestones[1].Completed && WordsLearned >= 1
        CompleteMilestone(1)
    EndIf
    
    ; Dragonborn (10 words)
    If !Milestones[7].Completed && WordsLearned >= 10
        CompleteMilestone(7)
    EndIf
EndFunction

Function CheckLevelMilestones()
    Actor player = Game.GetPlayer()
    Int currentLevel = player.GetLevel()
    
    ; Rising Power (Level 25)
    If !Milestones[3].Completed && currentLevel >= 25
        CompleteMilestone(3)
    EndIf
    
    ; Adept (Level 50)
    If !Milestones[6].Completed && currentLevel >= 50
        CompleteMilestone(6)
    EndIf
    
    ; Master (Level 100)
    If !Milestones[8].Completed && currentLevel >= 100
        CompleteMilestone(8)
    EndIf
EndFunction

; ============================================
; MILESTONE COMPLETION
; ============================================

Function CompleteMilestone(Int milestoneIndex)
    If milestoneIndex < 0 || milestoneIndex >= Milestones.Length
        Return
    EndIf
    
    Milestone ms = Milestones[milestoneIndex]
    If ms.Completed
        Return
    EndIf
    
    ; Mark as completed
    Milestones[milestoneIndex].Completed = True
    
    ; Grant rewards
    Actor player = Game.GetPlayer()
    If ms.RewardPerks > 0
        player.AddPerkPoints(ms.RewardPerks)
        TotalPerksEarned += ms.RewardPerks
    EndIf
    
    ; Show completion message
    ShowMilestoneComplete(ms)
EndFunction

Function ShowMilestoneComplete(Milestone ms)
    String message = "╔══════════════════════════════════════╗\n"
    message += "║     SYSTEM MILESTONE ACHIEVED!       ║\n"
    message += "╠══════════════════════════════════════╣\n"
    message += "  " + ms.Name + "\n"
    message += "  " + ms.Description + "\n"
    message += "╠══════════════════════════════════════╣\n"
    message += "  REWARD: " + ms.RewardPerks + " Perk Points\n"
    message += "╚══════════════════════════════════════╝"
    
    Debug.MessageBox(message)
    Debug.Notification("[SYSTEM] Milestone: " + ms.Name)
    
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
    If !Milestones[5].Completed
        CompleteMilestone(5)
    EndIf
EndFunction

; ============================================
; STATUS QUERIES (FOR MCM)
; ============================================

Int Function GetCompletedMilestoneCount()
    Int count = 0
    Int i = 0
    While i < Milestones.Length
        If Milestones[i].Completed
            count += 1
        EndIf
        i += 1
    EndWhile
    Return count
EndFunction

Int Function GetTotalPossiblePerks()
    Int total = 0
    Int i = 0
    While i < Milestones.Length
        total += Milestones[i].RewardPerks
        i += 1
    EndWhile
    Return total
EndFunction

String Function GetMilestoneStatus(Int index)
    If index < 0 || index >= Milestones.Length
        Return "Invalid"
    EndIf
    
    If Milestones[index].Completed
        Return "✓ " + Milestones[index].Name
    Else
        Return "○ " + Milestones[index].Name
    EndIf
EndFunction

; ============================================
; INITIAL STARTUP BONUS
; ============================================

Function GrantStartingBonus(Int powerLevel)
    ; Grant first steps milestone immediately
    CompleteMilestone(0)
    
    ; Power level affects starting perks
    Actor player = Game.GetPlayer()
    If powerLevel == 1 ; Hero
        ; Already gets 50 from PowerScript, give some bonus
        Debug.Notification("[SYSTEM] Hero bonus: Additional perks unlocked through milestones")
    ElseIf powerLevel == 2 ; Ascended
        ; Already gets 500, but milestones give even more
        Debug.Notification("[SYSTEM] Ascended bonus: Unlimited growth potential")
    EndIf
EndFunction
