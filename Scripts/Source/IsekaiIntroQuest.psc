; IsekaiIntroQuest.psc
; Main quest script for Isekai Hero mod
; "The System has chosen you. Your new life begins."

Scriptname IsekaiIntroQuest extends Quest

; ============================================
; SYSTEM COMPONENTS
; ============================================

IsekaiDialogScript Property DialogScript Auto
IsekaiPowerScript Property PowerScript Auto

; ============================================
; QUEST STAGES (Updated with World Selection)
; ============================================

Int Property STAGE_WAIT_FOR_SPAWN = 10 AutoReadOnly
Int Property STAGE_SYSTEM_BOOT = 20 AutoReadOnly
Int Property STAGE_WORLD_SELECT = 25 AutoReadOnly ; NEW!
Int Property STAGE_POWER_CHOICE = 30 AutoReadOnly
Int Property STAGE_SKILL_FOCUS = 40 AutoReadOnly
Int Property STAGE_EQUIPMENT = 50 AutoReadOnly
Int Property STAGE_APPLY = 60 AutoReadOnly
Int Property STAGE_COMPLETE = 100 AutoReadOnly

; ============================================
; PLAYER CHOICES (Stored by the System)
; ============================================

Int Property ChosenPowerLevel = 0 Auto Hidden ; 0=Normal, 1=Hero, 2=God
Int Property ChosenSkillFocus = 0 Auto Hidden ; 0=Equal, 1=Warrior, 2=Mage, 3=Thief
Int Property ChosenEquipment = 0 Auto Hidden ; 0=Humble, 1=Adventurer, 2=Hero, 3=None

; ============================================
; SYSTEM INITIALIZATION
; ============================================

Event OnInit()
    Debug.Notification("[SYSTEM] Scanning for soul signature...")
    RegisterForSingleUpdate(3.0)
EndEvent

Event OnUpdate()
    If IsPlayerSpawned()
        Debug.Notification("[SYSTEM] Soul detected in Nirn.")
        SetStage(STAGE_SYSTEM_BOOT)
    Else
        RegisterForSingleUpdate(3.0)
    EndIf
EndEvent

Bool Function IsPlayerSpawned()
    Worldspace currentWorld = Game.GetPlayer().GetWorldspace()
    Return currentWorld != None
EndFunction

; ============================================
; SYSTEM BOOT SEQUENCE
; ============================================

; Stage 20: The System awakens
Function TriggerAwakening()
    Game.SetInChargen(True, True, True)
    
    ; Activate the System - now includes world selection!
    If DialogScript
        DialogScript.ActivateSystem()
    Else
        Debug.Notification("[SYSTEM ERROR] Dialog component not found!")
        ShowPowerChoice()
    EndIf
EndFunction

; Called from DialogScript after world selection
Function OnWorldSelected(Int worldIdx)
    SetStage(STAGE_WORLD_SELECT)
    ; World selection handled in DialogScript, continues to welcome
EndFunction

; ============================================
; SYSTEM SEQUENCES
; ============================================

; Called from DialogScript after system welcome
Function ShowPowerChoice()
    SetStage(STAGE_POWER_CHOICE)
    DialogScript.ShowPowerChoice()
EndFunction

; Called when power level is selected
Function OnPowerChosen(Int powerLevel)
    ChosenPowerLevel = powerLevel
    
    If powerLevel == 0
        ; Normal mode - skip to completion
        Debug.Notification("[SYSTEM] Standard reincarnation selected.")
        SkipToCompletion()
    Else
        ; Hero/God mode - continue with customization
        SetStage(STAGE_SKILL_FOCUS)
        DialogScript.ShowSkillFocus()
    EndIf
EndFunction

; Called when skill focus is selected
Function OnSkillFocusChosen(Int focus)
    ChosenSkillFocus = focus
    SetStage(STAGE_EQUIPMENT)
    DialogScript.ShowEquipment()
EndFunction

; Called when equipment is selected
Function OnEquipmentChosen(Int equipment)
    ChosenEquipment = equipment
    SetStage(STAGE_APPLY)
    ApplyChoices()
EndFunction

; ============================================
; SYSTEM APPLICATION
; ============================================

Function ApplyChoices()
    Game.SetInChargen(False, False, False)
    
    If ChosenPowerLevel == 0
        ; Normal mode
        Debug.Notification("[SYSTEM] Standard mode confirmed.")
        Debug.Notification("[SYSTEM] May your journey be challenging.")
    Else
        ; Apply the System's blessings
        PowerScript.ApplyPowerLevel(ChosenPowerLevel, ChosenSkillFocus)
        PowerScript.GiveEquipment(ChosenEquipment)
        
        ; System confirmation
        If ChosenPowerLevel == 2
            Debug.Notification("[SYSTEM] ⚠ GOD MODE ACTIVATED")
            Debug.Notification("[SYSTEM] The world trembles before you...")
        Else
            Debug.Notification("[SYSTEM] ✓ HERO MODE ACTIVATED")
            Debug.Notification("[SYSTEM] Your legend begins.")
        EndIf
    EndIf
    
    ; Show completion summary
    SetStage(STAGE_COMPLETE)
    ShowSystemCompletion()
EndFunction

Function ShowSystemCompletion()
    If DialogScript
        DialogScript.ShowSystemComplete(ChosenPowerLevel, ChosenSkillFocus, ChosenEquipment)
    EndIf
EndFunction

; Skip customization for Normal mode
Function SkipToCompletion()
    SetStage(STAGE_COMPLETE)
    
    Game.SetInChargen(False, False, False)
    
    String normalText = "═══════════════════════════════════════\n\n"
    normalText += "You have chosen the path of a native.\n\n"
    normalText += "No memories from your past life remain.\n"
    normalText += "You are truly reborn in this world.\n\n"
    normalText += "Good luck, adventurer.\n\n"
    normalText += "═══════════════════════════════════════"
    
    Debug.MessageBox(normalText)
    
    Debug.Notification("[SYSTEM] Reincarnation complete.")
EndFunction

; ============================================
; SYSTEM RESPEC (For MCM)
; ============================================

Function RespecCharacter()
    Debug.Notification("[SYSTEM] Reinitializing...")
    
    ; Reset choices
    ChosenPowerLevel = 0
    ChosenSkillFocus = 0
    ChosenEquipment = 0
    
    ; Restart the sequence
    SetStage(STAGE_SYSTEM_BOOT)
    TriggerAwakening()
EndFunction

; ============================================
; UTILITY FUNCTIONS
; ============================================

; Get current power level name
String Function GetCurrentPowerName()
    If ChosenPowerLevel == 0
        Return "NORMAL"
    ElseIf ChosenPowerLevel == 1
        Return "HERO"
    ElseIf ChosenPowerLevel == 2
        Return "GOD MODE"
    EndIf
    Return "UNKNOWN"
EndFunction

; Check if System is active
Bool Function IsSystemActive()
    Return GetStage() >= STAGE_COMPLETE
EndFunction