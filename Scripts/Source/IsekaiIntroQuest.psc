; IsekaiIntroQuest.psc
; Main quest script for Isekai Hero mod
; VERSION 2.0 - With Progression & Perks
; "The System has chosen you. Your new life begins."
; Compatible with: Vanilla, Skyrim Unbound, Alternate Start, LAL, N.Y.A Modlist

Scriptname IsekaiIntroQuest extends Quest

; ============================================
; SYSTEM COMPONENTS
; ============================================

IsekaiDialogScript Property DialogScript Auto
IsekaiPowerScript Property PowerScript Auto
IsekaiProgressionScript Property ProgressionScript Auto
IsekaiPerkDefinitions Property PerkDefs Auto
IsekaiMCMScript Property MCM Auto

; ============================================
; QUEST STAGES
; ============================================

Int Property STAGE_WAIT_FOR_SPAWN = 10 AutoReadOnly
Int Property STAGE_SYSTEM_BOOT = 20 AutoReadOnly
Int Property STAGE_WORLD_SELECT = 25 AutoReadOnly
Int Property STAGE_POWER_CHOICE = 30 AutoReadOnly
Int Property STAGE_SKILL_FOCUS = 40 AutoReadOnly
Int Property STAGE_EQUIPMENT = 50 AutoReadOnly
Int Property STAGE_WEALTH = 55 AutoReadOnly
Int Property STAGE_APPLY = 60 AutoReadOnly
Int Property STAGE_COMPLETE = 100 AutoReadOnly

; ============================================
; PLAYER CHOICES
; ============================================

Int Property ChosenPowerLevel = 0 Auto Hidden ; 0=Normal, 1=Hero, 2=Ascended
Int Property ChosenSkillFocus = 0 Auto Hidden ; 0=Equal, 1=Warrior, 2=Mage, 3=Thief
Int Property ChosenEquipment = 0 Auto Hidden ; 0=Humble, 1=Adventurer, 2=Hero, 3=None
Int Property ChosenWealth = 0 Auto Hidden ; 0=Modest, 1=Wealthy, 2=Noble, 3=MerchantPrince

; ============================================
; START MOD DETECTION
; ============================================

Bool Property AlternateStartInstalled Auto Hidden
Bool Property LALInstalled Auto Hidden
Bool Property SkyrimUnboundInstalled Auto Hidden

; ============================================
; SYSTEM INITIALIZATION
; ============================================

Event OnInit()
    Debug.Notification("[SYSTEM] Initializing...")
    CheckStartMods()
    RegisterForSingleUpdate(5.0)
EndEvent

Function CheckStartMods()
    ; Detect popular start mods
    AlternateStartInstalled = Game.IsPluginInstalled("AlternateStart.esp")
    LALInstalled = Game.IsPluginInstalled("Alternate Start - Live Another Life.esp")
    SkyrimUnboundInstalled = Game.IsPluginInstalled("SkyrimUnbound.esp")
    
    If SkyrimUnboundInstalled
        Debug.Notification("[SYSTEM] Detected: Skyrim Unbound")
    ElseIf LALInstalled
        Debug.Notification("[SYSTEM] Detected: Live Another Life")
    ElseIf AlternateStartInstalled
        Debug.Notification("[SYSTEM] Detected: Alternate Start")
    Else
        Debug.Notification("[SYSTEM] Standard start detected")
    EndIf
EndFunction

; ============================================
; SMART TRIGGER SYSTEM
; ============================================

Event OnUpdate()
    If IsReadyForAwakening()
        Debug.Notification("[SYSTEM] Soul stabilized. Initiating contact...")
        Utility.Wait(2.0)
        SetStage(STAGE_SYSTEM_BOOT)
    Else
        ; Check again in a few seconds
        RegisterForSingleUpdate(3.0)
    EndIf
EndEvent

; Check if player is truly ready (not in character creation, not imprisoned)
Bool Function IsReadyForAwakening()
    Actor player = Game.GetPlayer()
    
    ; Basic checks
    If !player
        Return False
    EndIf
    
    ; Check if we're in a valid worldspace
    Worldspace currentWorld = player.GetWorldspace()
    If !currentWorld
        Return False
    EndIf
    
    ; Check if player is in character creation/prison
    ; These worlds/locations indicate "not ready yet"
    String worldName = currentWorld.GetName()
    
    ; Blocked worlds (character creation, prison, etc.)
    If worldName == ""
        Return False
    EndIf
    
    ; Check if player can move (not in cutscene)
    If player.IsInScene()
        Return False
    EndIf
    
    ; Check if player has control (not in menu/dialog)
    If UI.IsMenuOpen("Dialogue Menu") || UI.IsMenuOpen("RaceSex Menu")
        Return False
    EndIf
    
    ; Special check: Is player in "chargen" mode?
    ; This is true during character creation
    If IsInChargenArea(player)
        Return False
    EndIf
    
    ; All checks passed - player is ready
    Return True
EndFunction

; Check if player is still in character generation
Bool Function IsInChargenArea(Actor player)
    ; Check common chargen cells/worldspaces
    Location currentLoc = player.GetCurrentLocation()
    
    If currentLoc
        String locName = currentLoc.GetName()
        
        ; Common chargen location names
        If StringUtil.Find(locName, "Prison") >= 0 || \
           StringUtil.Find(locName, "Chargen") >= 0 || \
           StringUtil.Find(locName, "CharGen") >= 0 || \
           StringUtil.Find(locName, "Prisoner") >= 0 || \
           StringUtil.Find(locName, "Cell") >= 0
            Return True
        EndIf
    EndIf
    
    ; Check worldspace
    Worldspace currentWorld = player.GetWorldspace()
    If currentWorld
        String worldName = currentWorld.GetName()
        
        ; Helgen Keep is the vanilla prison
        If worldName == "HelgenKeep" || worldName == ""
            Return True
        EndIf
    EndIf
    
    Return False
EndFunction

; ============================================
; SYSTEM BOOT SEQUENCE
; ============================================

Function TriggerAwakening()
    ; Pause game for dramatic effect (but allow UI)
    Game.SetInChargen(True, True, True)
    
    ; Activate the System
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
EndFunction

; ============================================
; SYSTEM SEQUENCES
; ============================================

Function ShowPowerChoice()
    SetStage(STAGE_POWER_CHOICE)
    DialogScript.ShowPowerChoice()
EndFunction

Function OnPowerChosen(Int powerLevel)
    ChosenPowerLevel = powerLevel
    
    If powerLevel == 0
        Debug.Notification("[SYSTEM] Standard reincarnation selected.")
        SkipToCompletion()
    Else
        SetStage(STAGE_SKILL_FOCUS)
        DialogScript.ShowSkillFocus()
    EndIf
EndFunction

Function OnSkillFocusChosen(Int focus)
    ChosenSkillFocus = focus
    SetStage(STAGE_EQUIPMENT)
    DialogScript.ShowEquipment()
EndFunction

Function OnEquipmentChosen(Int equipment)
    ChosenEquipment = equipment
    SetStage(STAGE_WEALTH)
    DialogScript.ShowWealthChoice()
EndFunction

Function OnWealthChosen(Int wealth)
    ChosenWealth = wealth
    SetStage(STAGE_APPLY)
    ApplyChoices()
EndFunction

; ============================================
; SYSTEM APPLICATION
; ============================================

Function ApplyChoices()
    ; Release chargen lock
    Game.SetInChargen(False, False, False)
    
    If ChosenPowerLevel == 0
        Debug.Notification("[SYSTEM] Standard mode confirmed.")
        Debug.Notification("[SYSTEM] May your journey be challenging.")
    Else
        PowerScript.ApplyPowerLevel(ChosenPowerLevel, ChosenSkillFocus)
        PowerScript.GiveEquipment(ChosenEquipment)
        PowerScript.GiveWealth(ChosenWealth)
        
        ; Grant Isekai perks based on choices
        If PerkDefs
            PerkDefs.GrantStartingPerks(Game.GetPlayer(), ChosenPowerLevel, ChosenSkillFocus)
        EndIf
        
        ; Initialize progression system
        If ProgressionScript
            ProgressionScript.GrantStartingBonus(ChosenPowerLevel)
        EndIf
        
        If ChosenPowerLevel == 2
            Debug.Notification("[SYSTEM] ⚠ ASCENDED STATUS ACHIEVED")
            Debug.Notification("[SYSTEM] You have transcended mortal limits...")
        Else
            Debug.Notification("[SYSTEM] ✓ HERO MODE ACTIVATED")
            Debug.Notification("[SYSTEM] Your legend begins.")
        EndIf
    EndIf
    
    SetStage(STAGE_COMPLETE)
    ShowSystemCompletion()
EndFunction

Function ShowSystemCompletion()
    If DialogScript
        DialogScript.ShowSystemComplete(ChosenPowerLevel, ChosenSkillFocus, ChosenEquipment)
    EndIf
EndFunction

; ============================================
; NORMAL MODE SKIP
; ============================================

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
; SYSTEM RESPEC
; ============================================

Function RespecCharacter()
    Debug.Notification("[SYSTEM] Reinitializing...")
    
    Actor player = Game.GetPlayer()
    
    ; Reset choices
    ChosenPowerLevel = 0
    ChosenSkillFocus = 0
    ChosenEquipment = 0
    ChosenWealth = 0
    
    ; Reset skills if PowerScript available
    If PowerScript
        PowerScript.ResetAllSkills(player)
    EndIf
    
    ; Remove Isekai perks
    If PerkDefs
        ; This would need individual perk removal - simplified for now
        Debug.Notification("[SYSTEM] Note: Remove perks manually or use console")
    EndIf
    
    SetStage(STAGE_SYSTEM_BOOT)
    TriggerAwakening()
EndFunction

; ============================================
; UTILITY
; ============================================

String Function GetCurrentPowerName()
    If ChosenPowerLevel == 0
        Return "NORMAL"
    ElseIf ChosenPowerLevel == 1
        Return "HERO"
    ElseIf ChosenPowerLevel == 2
        Return "ASCENDED"
    EndIf
    Return "UNKNOWN"
EndFunction

Bool Function IsSystemActive()
    Return GetStage() >= STAGE_COMPLETE
EndFunction

; Manual trigger (for debug or MCM)
Function ForceTrigger()
    Debug.Notification("[SYSTEM] Manual activation initiated...")
    Utility.Wait(1.0)
    TriggerAwakening()
EndFunction
