; IsekaiDialogScript.psc
; Handles the System Interface for the Isekai Hero mod
; "Welcome, Reincarnated One. The System has recognized your soul."

Scriptname IsekaiDialogScript extends Quest

; Reference to main quest
IsekaiIntroQuest Property MainQuest Auto

; ============================================
; SYSTEM INTERFACE PROPERTIES
; ============================================

; Message forms for System dialogs
Message Property IsekaiMsg_SystemWelcome Auto
Message Property IsekaiMsg_StatusWindow Auto
Message Property IsekaiMsg_PowerChoice Auto
Message Property IsekaiMsg_SkillFocus Auto  
Message Property IsekaiMsg_EquipmentChoice Auto
Message Property IsekaiMsg_SystemComplete Auto

; System sounds (optional)
Sound Property QSTMagicCastArea Auto ; Magical activation sound
Sound Property UIUnlock Auto ; Unlock/powerup sound

; ============================================
; SYSTEM STATE
; ============================================

Bool Property UIExtensionsInstalled Auto Hidden
Bool Property SystemActivated Auto Hidden

; Player's previous world (flavor text)
String Property PreviousWorld = "Earth" Auto Hidden

; ============================================
; SYSTEM STRINGS (Isekai flavor)
; ============================================

String Property SYS_HEADER = "╔══════════════════════════════════════╗" AutoReadOnly
String Property SYS_FOOTER = "╚══════════════════════════════════════╝" AutoReadOnly
String Property SYS_DIVIDER = "═══════════════════════════════════════" AutoReadOnly

; ============================================
; INITIALIZATION
; ============================================

Event OnInit()
    CheckUIExtensions()
EndEvent

Function CheckUIExtensions()
    UIExtensionsInstalled = Game.IsPluginInstalled("UIExtensions.esp")
EndFunction

; ============================================
; SYSTEM ACTIVATION SEQUENCE
; ============================================

; Called when the awakening begins - dramatic system startup
Function ActivateSystem()
    SystemActivated = True
    
    ; Play activation sound if available
    If QSTMagicCastArea
        QSTMagicCastArea.Play(Game.GetPlayer())
    EndIf
    
    ; Show dramatic system boot sequence
    ShowSystemBootSequence()
    
    ; Then show the main welcome
    ShowSystemWelcome()
EndFunction

Function ShowSystemBootSequence()
    ; Simulate a system booting up - dramatic pauses
    Debug.Notification("[SYSTEM] Detecting soul signature...")
    Utility.Wait(0.8)
    
    Debug.Notification("[SYSTEM] Analyzing dimensional origin...")
    Utility.Wait(0.8)
    
    Debug.Notification("[SYSTEM] Origin confirmed: " + PreviousWorld)
    Utility.Wait(0.6)
    
    Debug.Notification("[SYSTEM] Reincarnation protocol initiated!")
    Utility.Wait(1.0)
    
    ; Dramatic pause before the big reveal
    Debug.Notification("[SYSTEM] Welcome to NIRN, Reincarnated One.")
    Utility.Wait(1.5)
EndFunction

; ============================================
; SYSTEM WELCOME DIALOG
; ============================================

Function ShowSystemWelcome()
    ; Build immersive system welcome message
    String welcomeText = BuildSystemHeader("WORLD SYSTEM") + "\n\n"
    welcomeText += "Greetings, Soul from Another World.\n\n"
    welcomeText += "You have been reincarnated into the realm of Nirn.\n"
    welcomeText += "Your previous existence in " + PreviousWorld + " has ended.\n\n"
    welcomeText += "The SYSTEM grants you a choice:\n"
    welcomeText += "How much power shall you retain from your past life?\n\n"
    welcomeText += BuildSystemFooter()
    
    ; Show welcome with dramatic flair
    Debug.MessageBox(welcomeText)
    Utility.Wait(0.5)
    
    ; Then proceed to power selection
    ShowPowerChoice()
EndFunction

; ============================================
; POWER LEVEL SELECTION (The Isekai Choice)
; ============================================

Function ShowPowerChoice()
    Int result
    
    If UIExtensionsInstalled
        result = ShowPowerChoice_SystemStyle()
    Else
        result = ShowPowerChoice_Vanilla()
    EndIf
    
    ; Play sound on selection
    If UIUnlock && result >= 0 && result < 3
        UIUnlock.Play(Game.GetPlayer())
    EndIf
    
    ; Handle result with system flavor
    If result == 3 || result == -1
        Debug.Notification("[SYSTEM] User declined power. Standard mode activated.")
        MainQuest.OnPowerChosen(0)
    Else
        String powerName = GetPowerLevelName(result)
        Debug.Notification("[SYSTEM] " + powerName + " mode selected.")
        MainQuest.OnPowerChosen(result)
    EndIf
EndFunction

Int Function ShowPowerChoice_Vanilla()
    If IsekaiMsg_PowerChoice
        Return IsekaiMsg_PowerChoice.Show()
    Else
        ; Fallback if message form not configured
        String text = BuildSystemHeader("STATUS ALLOCATION") + "\n\n"
        text += "Choose your reincarnation blessing:\n\n"
        text += "[1] NORMAL - No memories retained\n"
        text += "    Start as a native of this world\n\n"
        text += "[2] HERO - Partial awakening\n"
        text += "    Level 1 | Skills 100 | 50 Perk Points\n\n"
        text += "[3] GOD MODE - Full awakening\n"
        text += "    Level 255 | Max Skills | 500 Perk Points\n\n"
        text += "[4] DECLINE - Refuse the blessing\n\n"
        text += BuildSystemFooter()
        
        Debug.MessageBox(text)
        Return 0 ; Default to Normal if no form
    EndIf
EndFunction

Int Function ShowPowerChoice_SystemStyle()
    ; Enhanced UI version with system aesthetic
    String title = "═══ SYSTEM: STATUS ALLOCATION ═══"
    String desc = "Reincarnated One, choose your blessing level:"
    
    String[] options = new String[4]
    options[0] = "NORMAL"
    options[1] = "HERO"
    options[2] = "GOD MODE"
    options[3] = "DECLINE"
    
    String[] details = new String[4]
    details[0] = "No memories | Native start"
    details[1] = "Level 1 | Skills 100 | 50 Perks"
    details[2] = "Level 255 | Max Skills | 500 Perks"
    details[3] = "Refuse the System's gift"
    
    Return ShowSystemMenu(title, desc, options, details, 1)
EndFunction

; ============================================
; SKILL FOCUS SELECTION
; ============================================

Function ShowSkillFocus()
    Int result
    
    ; System flavor notification
    Debug.Notification("[SYSTEM] Allocating skill memories...")
    Utility.Wait(0.5)
    
    If UIExtensionsInstalled
        result = ShowSkillFocus_SystemStyle()
    Else
        result = ShowSkillFocus_Vanilla()
    EndIf
    
    If result == 5 || result == -1
        Debug.Notification("[SYSTEM] Default allocation: BALANCED")
        MainQuest.OnSkillFocusChosen(0)
    Else
        String focusName = GetSkillFocusName(result)
        Debug.Notification("[SYSTEM] " + focusName + " memories restored.")
        MainQuest.OnSkillFocusChosen(result)
    EndIf
EndFunction

Int Function ShowSkillFocus_Vanilla()
    If IsekaiMsg_SkillFocus
        Return IsekaiMsg_SkillFocus.Show()
    Else
        String text = BuildSystemHeader("SKILL ALLOCATION") + "\n\n"
        text += "Select your past life's expertise:\n\n"
        text += "[1] BALANCED - Equal mastery in all arts\n"
        text += "[2] WARRIOR - Combat mastery (Weapons/Armor)\n"
        text += "[3] MAGE - Arcane mastery (Magic schools)\n"
        text += "[4] THIEF - Shadow mastery (Stealth/Agility)\n"
        text += "[5] CUSTOM - Configure later via System Menu\n"
        text += "[6] BACK - Return to previous menu\n\n"
        text += BuildSystemFooter()
        
        Debug.MessageBox(text)
        Return 0
    EndIf
EndFunction

Int Function ShowSkillFocus_SystemStyle()
    String title = "═══ SYSTEM: SKILL ALLOCATION ═══"
    String desc = "Select expertise from your previous life:"
    
    String[] options = new String[6]
    options[0] = "BALANCED"
    options[1] = "WARRIOR"
    options[2] = "MAGE"
    options[3] = "THIEF"
    options[4] = "CUSTOM"
    options[5] = "BACK"
    
    String[] details = new String[6]
    details[0] = "All skills equal mastery"
    details[1] = "Weapons | Armor | Smithing"
    details[2] = "Destruction | Restoration | Conjuration"
    details[3] = "Sneak | Lockpicking | Pickpocket"
    details[4] = "Manual configuration"
    details[5] = "Return to power selection"
    
    Return ShowSystemMenu(title, desc, options, details, 0)
EndFunction

; ============================================
; EQUIPMENT SELECTION
; ============================================

Function ShowEquipment()
    Int result
    
    Debug.Notification("[SYSTEM] Summoning equipment from the void...")
    Utility.Wait(0.5)
    
    If UIExtensionsInstalled
        result = ShowEquipment_SystemStyle()
    Else
        result = ShowEquipment_Vanilla()
    EndIf
    
    If result == 4 || result == -1
        Debug.Notification("[SYSTEM] Minimal equipment selected.")
        MainQuest.OnEquipmentChosen(0)
    Else
        String equipName = GetEquipmentName(result)
        Debug.Notification("[SYSTEM] " + equipName + " equipment manifested.")
        MainQuest.OnEquipmentChosen(result)
    EndIf
EndFunction

Int Function ShowEquipment_Vanilla()
    If IsekaiMsg_EquipmentChoice
        Return IsekaiMsg_EquipmentChoice.Show()
    Else
        String text = BuildSystemHeader("EQUIPMENT SUMMONING") + "\n\n"
        text += "Choose your starting gear:\n\n"
        text += "[1] HUMBLE - Iron set, basic supplies, 100g\n"
        text += "[2] ADVENTURER - Steel set, potions, 500g\n"
        text += "[3] HERO - Daedric set, ultimate potions, 2000g\n"
        text += "[4] NONE - No equipment (hard mode)\n"
        text += "[5] BACK - Return to skill selection\n\n"
        text += BuildSystemFooter()
        
        Debug.MessageBox(text)
        Return 0
    EndIf
EndFunction

Int Function ShowEquipment_SystemStyle()
    String title = "═══ SYSTEM: EQUIPMENT SUMMONING ═══"
    String desc = "Select gear from the dimensional storage:"
    
    String[] options = new String[5]
    options[0] = "HUMBLE"
    options[1] = "ADVENTURER"
    options[2] = "HERO"
    options[3] = "NONE"
    options[4] = "BACK"
    
    String[] details = new String[5]
    details[0] = "Iron | Basic supplies | 100 gold"
    details[1] = "Steel | Health potions | 500 gold"
    details[2] = "Daedric | Ultimate potions | 2000 gold"
    details[3] = "No equipment | Pure skill"
    details[4] = "Return to skills"
    
    Return ShowSystemMenu(title, desc, options, details, 0)
EndFunction

; ============================================
; SYSTEM COMPLETION
; ============================================

Function ShowSystemComplete(Int powerLevel, Int skillFocus, Int equipment)
    String completionText = BuildSystemHeader("REINCARNATION COMPLETE") + "\n\n"
    
    completionText += "Status applied successfully.\n\n"
    completionText += "Power Level: " + GetPowerLevelName(powerLevel) + "\n"
    completionText += "Skill Focus: " + GetSkillFocusName(skillFocus) + "\n"
    completionText += "Equipment: " + GetEquipmentName(equipment) + "\n\n"
    
    If powerLevel == 2
        completionText += "⚠ WARNING: God Mode detected.\n"
        completionText += "   The world may not be ready...\n\n"
    ElseIf powerLevel == 1
        completionText += "✓ Hero Mode active.\n"
        completionText += "   Your legend begins now.\n\n"
    Else
        completionText += "✓ Standard Mode active.\n"
        completionText += "   Forge your own destiny.\n\n"
    EndIf
    
    completionText += "[SYSTEM] Good luck, Reincarnated One.\n"
    completionText += BuildSystemFooter()
    
    Debug.MessageBox(completionText)
    
    ; Final system notification
    Utility.Wait(0.5)
    Debug.Notification("[SYSTEM] Reincarnation protocol complete.")
    Debug.Notification("[SYSTEM] May your new life be glorious.")
EndFunction

; ============================================
; SYSTEM MENU HELPER
; ============================================

Int Function ShowSystemMenu(String title, String description, String[] options, String[] details, Int defaultIndex)
    ; Enhanced menu with system aesthetic
    ; Falls back to vanilla if UIExtensions not available
    
    If UIExtensionsInstalled
        ; Try to use UIExtensions for fancy menu
        ; For now, use formatted message box
        Return ShowFormattedSystemMenu(title, description, options, details)
    Else
        Return ShowFormattedSystemMenu(title, description, options, details)
    EndIf
EndFunction

Int Function ShowFormattedSystemMenu(String title, String description, String[] options, String[] details)
    String text = BuildSystemHeader(title) + "\n\n"
    text += description + "\n\n"
    text += SYS_DIVIDER + "\n"
    
    Int i = 0
    While i < options.Length
        text += "[" + (i + 1) + "] " + options[i]
        If details && i < details.Length
            text += " | " + details[i]
        EndIf
        text += "\n"
        i += 1
    EndWhile
    
    text += SYS_DIVIDER + "\n\n"
    text += BuildSystemFooter()
    
    Debug.MessageBox(text)
    
    ; Return -1 to indicate we need the Message Form for actual input
    Return -1
EndFunction

; ============================================
; SYSTEM TEXT BUILDERS
; ============================================

String Function BuildSystemHeader(String title)
    Return "╔═══ 「 " + title + " 」 ═══╗"
EndFunction

String Function BuildSystemFooter()
    Return "╚═══ [SYSTEM] ═══╝"
EndFunction

; ============================================
; UTILITY FUNCTIONS
; ============================================

String Function GetPowerLevelName(Int level)
    If level == 0
        Return "NORMAL"
    ElseIf level == 1
        Return "HERO"
    ElseIf level == 2
        Return "GOD MODE"
    EndIf
    Return "UNKNOWN"
EndFunction

String Function GetSkillFocusName(Int focus)
    If focus == 0
        Return "BALANCED"
    ElseIf focus == 1
        Return "WARRIOR"
    ElseIf focus == 2
        Return "MAGE"
    ElseIf focus == 3
        Return "THIEF"
    ElseIf focus == 4
        Return "CUSTOM"
    EndIf
    Return "UNKNOWN"
EndFunction

String Function GetEquipmentName(Int equip)
    If equip == 0
        Return "HUMBLE"
    ElseIf equip == 1
        Return "ADVENTURER"
    ElseIf equip == 2
        Return "HERO"
    ElseIf equip == 3
        Return "NONE"
    EndIf
    Return "UNKNOWN"
EndFunction

; Show a system status window (for MCM or debug)
Function ShowStatusWindow()
    Actor player = Game.GetPlayer()
    
    String status = BuildSystemHeader("STATUS WINDOW") + "\n\n"
    status += "Name: " + player.GetActorBase().GetName() + "\n"
    status += "Level: " + player.GetLevel() + "\n"
    status += "Race: " + player.GetRace().GetName() + "\n\n"
    status += "Origin: " + PreviousWorld + "\n"
    status += "Status: Reincarnated\n"
    status += BuildSystemFooter()
    
    Debug.MessageBox(status)
EndFunction

; Allow changing the previous world (for RP)
Function SetPreviousWorld(String worldName)
    PreviousWorld = worldName
    Debug.Notification("[SYSTEM] Origin updated: " + worldName)
EndFunction