; IsekaiMCMScript.psc
; Mod Configuration Menu for Isekai Hero
; Requires SkyUI

Scriptname IsekaiMCMScript extends SKI_ConfigBase

; ============================================
; REFERENCES TO MAIN SYSTEM
; ============================================

IsekaiIntroQuest Property MainQuest Auto
IsekaiDialogScript Property DialogScript Auto
IsekaiPowerScript Property PowerScript Auto

; ============================================
; MCM STATE
; ============================================

Int Property CurrentPage = 0 Auto Hidden

; Option IDs
Int OID_SystemStatus
Int OID_PowerLevel
Int OID_SkillFocus
Int OID_Equipment
Int OID_Wealth
Int OID_ReSpec
Int OID_ForceTrigger
Int OID_EnableNotifications
Int OID_DebugMode

; Toggle states
Bool Property EnableNotifications = True Auto Hidden
Bool Property DebugMode = False Auto Hidden

; ============================================
; MCM PAGES
; ============================================

Event OnConfigInit()
    Pages = new String[3]
    Pages[0] = "Status"
    Pages[1] = "Settings"
    Pages[2] = "Advanced"
EndEvent

Event OnPageReset(string page)
    If page == ""
        page = "Status"
    EndIf
    
    If page == "Status"
        ShowStatusPage()
    ElseIf page == "Settings"
        ShowSettingsPage()
    ElseIf page == "Advanced"
        ShowAdvancedPage()
    EndIf
EndEvent

; ============================================
; STATUS PAGE
; ============================================

Function ShowStatusPage()
    SetCursorFillMode(TOP_TO_BOTTOM)
    
    AddHeaderOption("SYSTEM STATUS")
    
    If MainQuest
        String status = "Inactive"
        If MainQuest.IsSystemActive()
            status = "ACTIVE"
        ElseIf MainQuest.GetStage() > 0
            status = "In Progress"
        EndIf
        AddTextOption("System State", status)
        
        If MainQuest.IsSystemActive()
            AddTextOption("Power Level", MainQuest.GetCurrentPowerName())
            AddTextOption("Dimensional Origin", DialogScript.PreviousWorld)
            AddEmptyOption()
            AddHeaderOption("CURRENT BONUSES")
            AddTextOption("Skills", GetSkillStatus())
            AddTextOption("Equipment Tier", GetEquipmentName(MainQuest.ChosenEquipment))
            AddTextOption("Wealth Level", GetWealthName(MainQuest.ChosenWealth))
        EndIf
    Else
        AddTextOption("ERROR", "Main Quest not found!")
    EndIf
    
    SetCursorPosition(1)
    AddHeaderOption("ACTIONS")
    OID_ReSpec = AddTextOption("Re-Spec Character", "CLICK")
    AddEmptyOption()
    AddHeaderOption("INFORMATION")
    AddTextOption("Mod Version", "2.0")
    AddTextOption("MCM Version", "1.0")
EndFunction

; ============================================
; SETTINGS PAGE
; ============================================

Function ShowSettingsPage()
    SetCursorFillMode(TOP_TO_BOTTOM)
    
    AddHeaderOption("NOTIFICATIONS")
    OID_EnableNotifications = AddToggleOption("Enable System Notifications", EnableNotifications)
    
    AddEmptyOption()
    AddHeaderOption("VISUAL")
    AddTextOption("Ascended Aura", "Coming Soon")
    AddTextOption("System Theme", "ASCII Style")
    
    AddEmptyOption()
    AddHeaderOption("COMPATIBILITY")
    AddTextOption("Skyrim Unbound", GetModStatus("SkyrimUnbound.esp"))
    AddTextOption("Alternate Start", GetModStatus("AlternateStart.esp"))
    AddTextOption("Live Another Life", GetModStatus("Alternate Start - Live Another Life.esp"))
    AddTextOption("UIExtensions", GetModStatus("UIExtensions.esp"))
EndFunction

; ============================================
; ADVANCED PAGE
; ============================================

Function ShowAdvancedPage()
    SetCursorFillMode(TOP_TO_BOTTOM)
    
    AddHeaderOption("DEBUG OPTIONS")
    OID_DebugMode = AddToggleOption("Debug Mode", DebugMode)
    
    AddEmptyOption()
    AddHeaderOption("MANUAL TRIGGERS")
    OID_ForceTrigger = AddTextOption("Force System Trigger", "CLICK")
    
    AddEmptyOption()
    AddHeaderOption("DANGER ZONE")
    AddTextOption("Reset Character", "Use Re-Spec on Status page")
EndFunction

; ============================================
; OPTION SELECTION HANDLING
; ============================================

Event OnOptionSelect(Int option)
    If option == OID_ReSpec
        ShowMessage("Re-Spec will reset your character and restart the System sequence.\n\nContinue?", 
            "Yes", "No", 0)
    ElseIf option == OID_ForceTrigger
        If MainQuest
            MainQuest.ForceTrigger()
            ShowMessage("System trigger forced!", "OK", "", 0)
        EndIf
    ElseIf option == OID_EnableNotifications
        EnableNotifications = !EnableNotifications
        SetToggleOptionValue(OID_EnableNotifications, EnableNotifications)
    ElseIf option == OID_DebugMode
        DebugMode = !DebugMode
        SetToggleOptionValue(OID_DebugMode, DebugMode)
    EndIf
EndEvent

Event OnOptionHighlight(Int option)
    If option == OID_ReSpec
        SetInfoText("Reset your character choices and restart the Isekai sequence")
    ElseIf option == OID_ForceTrigger
        SetInfoText("Manually trigger the System awakening (for debugging)")
    ElseIf option == OID_EnableNotifications
        SetInfoText("Toggle System notification messages")
    ElseIf option == OID_DebugMode
        SetInfoText("Enable debug logging and extra information")
    EndIf
EndEvent

; ============================================
; MESSAGE BOX CALLBACK
; ============================================

Event OnMessageBoxResponse(Int response)
    ; Response 0 = Yes, 1 = No
    If response == 0
        ; User confirmed Re-Spec
        If MainQuest
            MainQuest.RespecCharacter()
            ShowMessage("Character reset initiated.\n\nThe System will contact you shortly...", "OK", "", 0)
        EndIf
    EndIf
EndEvent

; ============================================
; UTILITY FUNCTIONS
; ============================================

String Function GetSkillStatus()
    If MainQuest.ChosenSkillFocus == 0
        Return "Balanced"
    ElseIf MainQuest.ChosenSkillFocus == 1
        Return "Warrior"
    ElseIf MainQuest.ChosenSkillFocus == 2
        Return "Mage"
    ElseIf MainQuest.ChosenSkillFocus == 3
        Return "Thief"
    Else
        Return "Custom"
    EndIf
EndFunction

String Function GetEquipmentName(Int equipment)
    If equipment == 0
        Return "Humble"
    ElseIf equipment == 1
        Return "Adventurer"
    ElseIf equipment == 2
        Return "Hero"
    ElseIf equipment == 3
        Return "None"
    Else
        Return "Unknown"
    EndIf
EndFunction

String Function GetWealthName(Int wealth)
    If wealth == 0
        Return "Modest (1k)"
    ElseIf wealth == 1
        Return "Wealthy (10k)"
    ElseIf wealth == 2
        Return "Noble (50k)"
    ElseIf wealth == 3
        Return "Merchant Prince (100k)"
    Else
        Return "Unknown"
    EndIf
EndFunction

String Function GetModStatus(String pluginName)
    If Game.IsPluginInstalled(pluginName)
        Return "Detected"
    Else
        Return "Not Found"
    EndIf
EndFunction

; ============================================
; EXTERNAL NOTIFICATION CONTROL
; ============================================

Function SystemNotification(String message)
    If EnableNotifications
        Debug.Notification(message)
    EndIf
EndFunction
