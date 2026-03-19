; IsekaiIntroQuest.psc
; Main quest script for Isekai Hero mod
; Handles the awakening sequence after character creation

Scriptname IsekaiIntroQuest extends Quest

; Properties
IsekaiDialogScript Property DialogScript Auto
IsekaiPowerScript Property PowerScript Auto

; Quest stages
Int Property STAGE_WAIT_FOR_SPAWN = 10 AutoReadOnly
Int Property STAGE_TRIGGER_AWAKENING = 20 AutoReadOnly
Int Property STAGE_POWER_CHOICE = 30 AutoReadOnly
Int Property STAGE_SKILL_FOCUS = 40 AutoReadOnly
Int Property STAGE_EQUIPMENT = 50 AutoReadOnly
Int Property STAGE_APPLY = 60 AutoReadOnly
Int Property STAGE_COMPLETE = 100 AutoReadOnly

; Player choices stored for application
Int Property ChosenPowerLevel = 0 Auto Hidden ; 0=Normal, 1=Hero, 2=God
Int Property ChosenSkillFocus = 0 Auto Hidden ; 0=Equal, 1=Warrior, 2=Mage, 3=Thief
Int Property ChosenEquipment = 0 Auto Hidden ; 0=Humble, 1=Adventurer, 2=Hero, 3=None

; Called when quest starts
Event OnInit()
    Debug.Notification("Isekai Hero: Waiting for player spawn...")
    RegisterForSingleUpdate(5.0) ; Check every 5 seconds
EndEvent

; Wait for player to leave character creation
Event OnUpdate()
    Actor player = Game.GetPlayer()
    
    ; Check if player has left Helgen/alternate start area
    If IsPlayerSpawned()
        SetStage(STAGE_TRIGGER_AWAKENING)
    Else
        RegisterForSingleUpdate(5.0) ; Keep waiting
    EndIf
EndEvent

; Check if player has spawned in the world
Bool Function IsPlayerSpawned()
    Worldspace currentWorld = Game.GetPlayer().GetWorldspace()
    
    ; Check if player is in any valid play area
    If currentWorld
        ; Player is in a worldspace (not in character creation)
        Return True
    EndIf
    
    Return False
EndFunction

; Stage 20: Trigger the awakening
Function TriggerAwakening()
    ; Pause game slightly for dramatic effect
    Game.SetInChargen(True, True, True)
    Utility.Wait(1.0)
    
    ; Show first dialog
    DialogScript.ShowPowerChoice()
EndFunction

; Called from dialog script when power is chosen
Function OnPowerChosen(Int powerLevel)
    ChosenPowerLevel = powerLevel
    SetStage(STAGE_SKILL_FOCUS)
    DialogScript.ShowSkillFocus()
EndFunction

; Called when skill focus is chosen
Function OnSkillFocusChosen(Int focus)
    ChosenSkillFocus = focus
    SetStage(STAGE_EQUIPMENT)
    DialogScript.ShowEquipment()
EndFunction

; Called when equipment is chosen
Function OnEquipmentChosen(Int equipment)
    ChosenEquipment = equipment
    SetStage(STAGE_APPLY)
    ApplyChoices()
EndFunction

; Apply all chosen settings
Function ApplyChoices()
    Game.SetInChargen(False, False, False)
    
    If ChosenPowerLevel == 0
        ; Normal - do nothing
        Debug.Notification("Isekai Hero: You chose the path of a normal adventurer.")
    Else
        ; Apply power level
        PowerScript.ApplyPowerLevel(ChosenPowerLevel, ChosenSkillFocus)
        
        ; Apply equipment
        PowerScript.GiveEquipment(ChosenEquipment)
        
        String powerText = "Hero"
        If ChosenPowerLevel == 2
            powerText = "God"
        EndIf
        
        Debug.Notification("Isekai Hero: " + powerText + " mode activated!")
    EndIf
    
    SetStage(STAGE_COMPLETE)
EndFunction

; Allow respec via MCM
Function RespecCharacter()
    SetStage(STAGE_TRIGGER_AWAKENING)
    TriggerAwakening()
EndFunction
