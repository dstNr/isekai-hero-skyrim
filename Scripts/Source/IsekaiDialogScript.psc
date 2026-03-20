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
Message Property IsekaiMsg_WealthChoice Auto
Message Property IsekaiMsg_SystemComplete Auto
Message Property IsekaiMsg_WorldSelect Auto

; System sounds
Sound Property QSTMagicCastArea Auto ; Magical activation
Sound Property UIUnlock Auto ; Unlock/powerup
Sound Property NPCDragonDeathSeqWind Auto ; Epic wind for God Mode
Sound Property MAGPowerAttackOrc Auto ; Power surge
Sound Property MAGIllusionNightEyeAuto Auto ; Aura activate

; Visual effects
Explosion Property FXDragonDeath seq Auto ; God Mode arrival
ActiveMagicEffect Property AbFXShock Auto ; Electric aura

; ============================================
; SYSTEM STATE
; ============================================

Bool Property UIExtensionsInstalled Auto Hidden
Bool Property SystemActivated Auto Hidden
Bool Property AscendedAuraActive Auto Hidden

; ============================================
; ORIGIN WORLD SELECTION
; ============================================

String Property PreviousWorld = "Earth" Auto Hidden
Int Property WorldIndex = 0 Auto Hidden ; 0=Earth, 1=Japan, 2=Korea, 3=Fantasy, 4=SciFi, 5=Apocalyptic

String[] Property AvailableWorlds Auto
String[] Property WorldDescriptions Auto
String[] Property WorldFlavors Auto

; ============================================
; SYSTEM STRINGS
; ============================================

String Property SYS_HEADER = "╔══════════════════════════════════════╗" AutoReadOnly
String Property SYS_FOOTER = "╚══════════════════════════════════════╝" AutoReadOnly
String Property SYS_DIVIDER = "═══════════════════════════════════════" AutoReadOnly

; ============================================
; INITIALIZATION
; ============================================

Event OnInit()
    InitializeWorldData()
    CheckUIExtensions()
EndEvent

Function InitializeWorldData()
    ; Available origin worlds for the reincarnated
    AvailableWorlds = new String[6]
    AvailableWorlds[0] = "Earth"
    AvailableWorlds[1] = "Japan"
    AvailableWorlds[2] = "Korea"
    AvailableWorlds[3] = "Fantasy World"
    AvailableWorlds[4] = "Sci-Fi Future"
    AvailableWorlds[5] = "Apocalyptic Wasteland"
    
    ; Descriptions for each world
    WorldDescriptions = new String[6]
    WorldDescriptions[0] = "A world of technology without magic"
    WorldDescriptions[1] = "Land of anime, ramen, and truck-kun"
    WorldDescriptions[2] = "Kingdom of webtoons and dungeon breaks"
    WorldDescriptions[3] = "A realm already filled with swords and sorcery"
    WorldDescriptions[4] = "Advanced technology, space travel, AI"
    WorldDescriptions[5] = "Survival, mutations, scarce resources"
    
    ; Special flavor text for each world
    WorldFlavors = new String[6]
    WorldFlavors[0] = "You were an ordinary person until the accident..."
    WorldFlavors[1] = "Truck-kun sent you on your next adventure..."
    WorldFlavors[2] = "The dungeon break claimed you, but the System intervened..."
    WorldFlavors[3] = "You were a hero there too. Now you begin again..."
    WorldFlavors[4] = "Cryosleep failed. Your consciousness was saved..."
    WorldFlavors[5] = "Radiation took your body, not your soul..."
EndFunction

Function CheckUIExtensions()
    UIExtensionsInstalled = Game.IsPluginInstalled("UIExtensions.esp")
EndFunction

; ============================================
; WORLD SELECTION (New Step!)
; ============================================

Function ShowWorldSelection()
    Int result
    
    Debug.Notification("[SYSTEM] Scanning dimensional origins...")
    Utility.Wait(0.5)
    
    If UIExtensionsInstalled
        result = ShowWorldSelection_UIExt()
    Else
        result = ShowWorldSelection_Vanilla()
    EndIf
    
    If result >= 0 && result < AvailableWorlds.Length
        WorldIndex = result
        PreviousWorld = AvailableWorlds[result]
        Debug.Notification("[SYSTEM] Origin confirmed: " + PreviousWorld)
        Debug.Notification("[SYSTEM] " + WorldFlavors[result])
        
        ; Play special sound based on world
        PlayWorldArrivalSound(result)
    Else
        WorldIndex = 0
        PreviousWorld = "Earth"
    EndIf
    
    ; Continue to system welcome
    Utility.Wait(1.0)
    ShowSystemWelcome()
EndFunction

Int Function ShowWorldSelection_Vanilla()
    If IsekaiMsg_WorldSelect
        Return IsekaiMsg_WorldSelect.Show()
    Else
        ; Build custom world selection
        String text = BuildSystemHeader("DIMENSIONAL ORIGIN") + "\n\n"
        text += "From which world do you hail, Reincarnated One?\n\n"
        text += "[1] EARTH - Modern world, no magic\n"
        text += "[2] JAPAN - Land of truck-kun incidents\n"
        text += "[3] KOREA - Dungeons and hunters\n"
        text += "[4] FANTASY WORLD - Swords and sorcery\n"
        text += "[5] SCI-FI FUTURE - Advanced technology\n"
        text += "[6] APOCALYPTIC - Survival and mutations\n\n"
        text += BuildSystemFooter()
        
        Debug.MessageBox(text)
        Return 0
    EndIf
EndFunction

Int Function ShowWorldSelection_UIExt()
    String title = "═══ SYSTEM: DIMENSIONAL ORIGIN ═══"
    String desc = "Select your previous incarnation's world:"
    
    Return ShowSystemMenu(title, desc, AvailableWorlds, WorldDescriptions, 0)
EndFunction

Function PlayWorldArrivalSound(Int worldIdx)
    ; Different sounds for different origins
    If worldIdx == 1 || worldIdx == 2 ; Japan/Korea
        ; Anime-style magical sound
        If MAGIllusionNightEyeAuto
            MAGIllusionNightEyeAuto.Play(Game.GetPlayer())
        EndIf
    ElseIf worldIdx == 3 ; Fantasy
        ; Mystical sound
        If QSTMagicCastArea
            QSTMagicCastArea.Play(Game.GetPlayer())
        EndIf
    ElseIf worldIdx == 4 ; Sci-Fi
        ; Tech sound (use unlock as substitute)
        If UIUnlock
            UIUnlock.Play(Game.GetPlayer())
        EndIf
    ElseIf worldIdx == 5 ; Apocalyptic
        ; Darker sound
        If NPCDragonDeathSeqWind
            NPCDragonDeathSeqWind.Play(Game.GetPlayer())
        EndIf
    EndIf
EndFunction

; ============================================
; SYSTEM ACTIVATION SEQUENCE
; ============================================

Function ActivateSystem()
    SystemActivated = True
    
    ; Play activation sound
    If QSTMagicCastArea
        QSTMagicCastArea.Play(Game.GetPlayer())
    EndIf
    
    ShowSystemBootSequence()
    
    ; NEW: Show world selection first!
    ShowWorldSelection()
EndFunction

Function ShowSystemBootSequence()
    Debug.Notification("[SYSTEM] ╔══════════════════════════════════════╗")
    Utility.Wait(0.5)
    Debug.Notification("[SYSTEM] ║     WORLD SYSTEM v2.0.1 ONLINE       ║")
    Utility.Wait(0.5)
    Debug.Notification("[SYSTEM] ╚══════════════════════════════════════╝")
    Utility.Wait(0.8)
    
    Debug.Notification("[SYSTEM] Detecting soul signature...")
    Utility.Wait(0.8)
    Debug.Notification("[SYSTEM] Analyzing dimensional residue...")
    Utility.Wait(0.8)
    Debug.Notification("[SYSTEM] Multiple world signatures detected!")
    Utility.Wait(1.0)
    
    Debug.Notification("[SYSTEM] Initiating reincarnation protocol...")
    Utility.Wait(1.5)
EndFunction

; ============================================
; SYSTEM WELCOME
; ============================================

Function ShowSystemWelcome()
    String welcomeText = BuildSystemHeader("WORLD SYSTEM") + "\n\n"
    
    welcomeText += "Greetings, Soul from " + PreviousWorld + ".\n\n"
    welcomeText += WorldFlavors[WorldIndex] + "\n\n"
    welcomeText += "You have been reincarnated into the realm of NIRN.\n"
    welcomeText += "Your previous life has ended. Your new story begins.\n\n"
    welcomeText += "The SYSTEM grants you a choice:\n"
    welcomeText += "How much power shall you retain?\n\n"
    
    ; Add world-specific bonuses
    If WorldIndex == 1 || WorldIndex == 2
        welcomeText += "⚠ Bonus: Isekai Knowledge detected!\n"
    ElseIf WorldIndex == 3
        welcomeText += "⚠ Bonus: Prior magic affinity detected!\n"
    ElseIf WorldIndex == 4
        welcomeText += "⚠ Bonus: Advanced intelligence detected!\n"
    ElseIf WorldIndex == 5
        welcomeText += "⚠ Bonus: Survival instincts detected!\n"
    EndIf
    
    welcomeText += "\n" + BuildSystemFooter()
    
    Debug.MessageBox(welcomeText)
    Utility.Wait(0.5)
    
    ShowPowerChoice()
EndFunction

; ============================================
; POWER LEVEL SELECTION
; ============================================

Function ShowPowerChoice()
    Int result
    
    If UIExtensionsInstalled
        result = ShowPowerChoice_SystemStyle()
    Else
        result = ShowPowerChoice_Vanilla()
    EndIf
    
    If UIUnlock && result >= 0 && result < 3
        UIUnlock.Play(Game.GetPlayer())
    EndIf
    
    If result == 3 || result == -1
        Debug.Notification("[SYSTEM] User declined power. Standard mode.")
        MainQuest.OnPowerChosen(0)
    Else
        String powerName = GetPowerLevelName(result)
        Debug.Notification("[SYSTEM] " + powerName + " mode selected.")
        
        ; Special effects for Ascended mode
        If result == 2
            TriggerAscendedEffects()
        EndIf
        
        MainQuest.OnPowerChosen(result)
    EndIf
EndFunction

Function TriggerAscendedEffects()
    ; Dramatic God Mode activation
    Debug.Notification("[SYSTEM] ⚠⚠⚠ ASCENSION ACTIVATED ⚠⚠⚠")
    Utility.Wait(0.3)
    
    ; Play epic sound
    If NPCDragonDeathSeqWind
        NPCDragonDeathSeqWind.Play(Game.GetPlayer())
    EndIf
    
    ; Screen shake effect via magic
    If MAGPowerAttackOrc
        MAGPowerAttackOrc.Play(Game.GetPlayer())
    EndIf
    
    ; Visual effect
    Actor player = Game.GetPlayer()
    If player
        ; Apply temporary god-like visual
        Debug.Notification("[SYSTEM] Reality bending...")
        Utility.Wait(0.5)
        Debug.Notification("[SYSTEM] Power level: MAXIMUM")
    EndIf
    
    GodModeAuraActive = True
    Utility.Wait(1.0)
EndFunction

Int Function ShowPowerChoice_Vanilla()
    If IsekaiMsg_PowerChoice
        Return IsekaiMsg_PowerChoice.Show()
    Else
        String text = BuildSystemHeader("STATUS ALLOCATION") + "\n\n"
        text += "Choose your reincarnation blessing:\n\n"
        text += "[1] NORMAL - No memories, native start\n"
        text += "[2] HERO - Level 1 | Skills 100 | 50 Perks\n"
        text += "[3] ASCENDED - Level 255 | Max Skills | 500 Perks\n"
        text += "    ⚠ Ultimate power achieved!\n"
        text += "[4] DECLINE - Refuse the blessing\n\n"
        text += BuildSystemFooter()
        
        Debug.MessageBox(text)
        Return 0
    EndIf
EndFunction

Int Function ShowPowerChoice_SystemStyle()
    String title = "═══ SYSTEM: STATUS ALLOCATION ═══"
    String desc = "Reincarnated One, choose your blessing:"
    
    String[] options = new String[4]
    options[0] = "NORMAL"
    options[1] = "HERO"
    options[2] = "ASCENDED"
    options[3] = "DECLINE"
    
    String[] details = new String[4]
    details[0] = "No boost | Pure challenge"
    details[1] = "Lv1 | Skill 100 | 50 Perks"
    details[2] = "⚠ Lv255 | MAX | 500 Perks"
    details[3] = "Refuse the System"
    
    Return ShowSystemMenu(title, desc, options, details, 1)
EndFunction

; ============================================
; SKILL FOCUS SELECTION
; ============================================

Function ShowSkillFocus()
    Int result
    
    Debug.Notification("[SYSTEM] Restoring skill memories...")
    Utility.Wait(0.5)
    
    If UIExtensionsInstalled
        result = ShowSkillFocus_SystemStyle()
    Else
        result = ShowSkillFocus_Vanilla()
    EndIf
    
    If result == 5 || result == -1
        Debug.Notification("[SYSTEM] Default: BALANCED allocation")
        MainQuest.OnSkillFocusChosen(0)
    Else
        String focusName = GetSkillFocusName(result)
        Debug.Notification("[SYSTEM] " + focusName + " expertise restored")
        MainQuest.OnSkillFocusChosen(result)
    EndIf
EndFunction

Int Function ShowSkillFocus_Vanilla()
    If IsekaiMsg_SkillFocus
        Return IsekaiMsg_SkillFocus.Show()
    Else
        String text = BuildSystemHeader("SKILL ALLOCATION") + "\n\n"
        text += "Select your past life's expertise:\n\n"
        text += "[1] BALANCED - Equal mastery\n"
        text += "[2] WARRIOR - Combat mastery\n"
        text += "[3] MAGE - Arcane mastery\n"
        text += "[4] THIEF - Shadow mastery\n"
        text += "[5] CUSTOM - Configure later\n"
        text += "[6] BACK\n\n"
        text += BuildSystemFooter()
        
        Debug.MessageBox(text)
        Return 0
    EndIf
EndFunction

Int Function ShowSkillFocus_SystemStyle()
    String title = "═══ SYSTEM: SKILL ALLOCATION ═══"
    String desc = "Select expertise from " + PreviousWorld + ":"
    
    String[] options = new String[6]
    options[0] = "BALANCED"
    options[1] = "WARRIOR"
    options[2] = "MAGE"
    options[3] = "THIEF"
    options[4] = "CUSTOM"
    options[5] = "BACK"
    
    String[] details = new String[6]
    details[0] = "All skills equal"
    details[1] = "Weapons | Armor | Smithing"
    details[2] = "All magic schools"
    details[3] = "Stealth | Agility"
    details[4] = "Manual config"
    details[5] = "Return"
    
    Return ShowSystemMenu(title, desc, options, details, 0)
EndFunction

; ============================================
; EQUIPMENT SELECTION
; ============================================

Function ShowEquipment()
    Int result
    
    Debug.Notification("[SYSTEM] Accessing dimensional storage...")
    Utility.Wait(0.5)
    
    If UIExtensionsInstalled
        result = ShowEquipment_SystemStyle()
    Else
        result = ShowEquipment_Vanilla()
    EndIf
    
    ; Special equipment for certain worlds
    String bonus = ""
    If WorldIndex == 3 ; Fantasy World
        bonus = " [+Magic Items]"
    ElseIf WorldIndex == 4 ; Sci-Fi
        bonus = " [+Tech Adaptation]"
    EndIf
    
    If result == 4 || result == -1
        Debug.Notification("[SYSTEM] Minimal gear selected")
        MainQuest.OnEquipmentChosen(0)
    Else
        String equipName = GetEquipmentName(result)
        Debug.Notification("[SYSTEM] " + equipName + " gear summoned" + bonus)
        MainQuest.OnEquipmentChosen(result)
    EndIf
EndFunction

; ============================================
; WEALTH SELECTION (NEW!)
; ============================================

Function ShowWealthChoice()
    Int result
    
    Debug.Notification("[SYSTEM] Accessing dimensional treasury...")
    Utility.Wait(0.5)
    
    If UIExtensionsInstalled
        result = ShowWealth_SystemStyle()
    Else
        result = ShowWealth_Vanilla()
    EndIf
    
    If result == 4 || result == -1
        Debug.Notification("[SYSTEM] Modest wealth selected")
        MainQuest.OnWealthChosen(0)
    Else
        String wealthName = GetWealthName(result)
        Debug.Notification("[SYSTEM] " + wealthName + " wealth granted")
        MainQuest.OnWealthChosen(result)
    EndIf
EndFunction

Int Function ShowWealth_Vanilla()
    If IsekaiMsg_WealthChoice
        Return IsekaiMsg_WealthChoice.Show()
    Else
        String text = BuildSystemHeader("WEALTH ALLOCATION") + "\n\n"
        text += "Choose your starting fortune:\n\n"
        text += "[1] MODEST - 1,000 gold\n"
        text += "    A humble merchant's savings\n\n"
        text += "[2] WEALTHY - 10,000 gold\n"
        text += "    A successful adventurer's hoard\n\n"
        text += "[3] NOBLE - 50,000 gold\n"
        text += "    A minor lord's fortune\n\n"
        text += "[4] MERCHANT PRINCE - 100,000 gold\n"
        text += "    ⚠ Wealth beyond measure!\n\n"
        text += "[5] BACK\n\n"
        text += BuildSystemFooter()
        
        Debug.MessageBox(text)
        Return 0
    EndIf
EndFunction

Int Function ShowWealth_SystemStyle()
    String title = "═══ SYSTEM: WEALTH ═══"
    String desc = "Select your dimensional treasury:"
    
    String[] options = new String[5]
    options[0] = "MODEST"
    options[1] = "WEALTHY"
    options[2] = "NOBLE"
    options[3] = "MERCHANT PRINCE"
    options[4] = "BACK"
    
    String[] details = new String[5]
    details[0] = "1,000 gold"
    details[1] = "10,000 gold"
    details[2] = "50,000 gold"
    details[3] = "⚠ 100,000 gold"
    details[4] = "Return"
    
    Return ShowSystemMenu(title, desc, options, details, 0)
EndFunction

String Function GetWealthName(Int wealth)
    If wealth == 0
        Return "MODEST"
    ElseIf wealth == 1
        Return "WEALTHY"
    ElseIf wealth == 2
        Return "NOBLE"
    ElseIf wealth == 3
        Return "MERCHANT PRINCE"
    EndIf
    Return "UNKNOWN"
EndFunction

Int Function ShowEquipment_Vanilla()
    If IsekaiMsg_EquipmentChoice
        Return IsekaiMsg_EquipmentChoice.Show()
    Else
        String text = BuildSystemHeader("EQUIPMENT SUMMONING") + "\n\n"
        text += "Choose your starting gear:\n\n"
        text += "[1] HUMBLE - Iron, basics, 100g\n"
        text += "[2] ADVENTURER - Steel, potions, 500g\n"
        text += "[3] HERO - Daedric, ultimate, 2000g\n"
        text += "[4] NONE - Hard mode\n"
        text += "[5] BACK\n\n"
        text += BuildSystemFooter()
        
        Debug.MessageBox(text)
        Return 0
    EndIf
EndFunction

Int Function ShowEquipment_SystemStyle()
    String title = "═══ SYSTEM: EQUIPMENT ═══"
    String desc = "Select gear from dimensional storage:"
    
    String[] options = new String[5]
    options[0] = "HUMBLE"
    options[1] = "ADVENTURER"
    options[2] = "HERO"
    options[3] = "NONE"
    options[4] = "BACK"
    
    String[] details = new String[5]
    details[0] = "Iron | 100 gold"
    details[1] = "Steel | Potions | 500g"
    details[2] = "Daedric | Ultimate | 2000g"
    details[3] = "No gear | Hard"
    details[4] = "Return"
    
    Return ShowSystemMenu(title, desc, options, details, 0)
EndFunction

; ============================================
; SYSTEM COMPLETION
; ============================================

Function ShowSystemComplete(Int powerLevel, Int skillFocus, Int equipment)
    String completionText = BuildSystemHeader("REINCARNATION COMPLETE") + "\n\n"
    
    completionText += "Origin: " + PreviousWorld + "\n"
    completionText += "Power: " + GetPowerLevelName(powerLevel) + "\n"
    completionText += "Expertise: " + GetSkillFocusName(skillFocus) + "\n"
    completionText += "Equipment: " + GetEquipmentName(equipment) + "\n"
    completionText += "Wealth: " + GetWealthName(MainQuest.ChosenWealth) + "\n\n"
    
    If powerLevel == 2
        completionText += "⚠⚠⚠ ASCENDED STATUS ACTIVE ⚠⚠⚠\n"
        completionText += "Reality anchor: STABLE\n"
        completionText += "Power limiter: DISABLED\n"
        completionText += "May the gods have mercy...\n\n"
    ElseIf powerLevel == 1
        completionText += "✓ HERO STATUS CONFIRMED\n"
        completionText += "Your legend awaits...\n\n"
    Else
        completionText += "✓ STANDARD REINCARNATION\n"
        completionText += "Forge your own destiny...\n\n"
    EndIf
    
    completionText += "[SYSTEM] Good luck, Reincarnated One.\n"
    completionText += "[SYSTEM] May your new life be glorious.\n\n"
    completionText += BuildSystemFooter()
    
    Debug.MessageBox(completionText)
    
    ; Final system messages
    Utility.Wait(0.5)
    Debug.Notification("[SYSTEM] ╔══════════════════════════════════════╗")
    Debug.Notification("[SYSTEM] ║   REINCARNATION PROTOCOL COMPLETE   ║")
    Debug.Notification("[SYSTEM] ╚══════════════════════════════════════╝")
    
    ; Ascended persistent notification
    If powerLevel == 2
        Utility.Wait(1.0)
        Debug.Notification("[SYSTEM] ⚠ Ascended mode active. Have fun!")
    EndIf
EndFunction

; ============================================
; STATUS WINDOW (NEW!)
; ============================================

Function ShowStatusWindow()
    Actor player = Game.GetPlayer()
    If !player
        Return
    EndIf
    
    String status = BuildSystemHeader("STATUS WINDOW") + "\n\n"
    
    ; Basic Info
    status += "╔═══ IDENTITY ═══╗\n"
    status += "Name: " + player.GetActorBase().GetName() + "\n"
    status += "Race: " + player.GetRace().GetName() + "\n"
    status += "Level: " + player.GetLevel() + "\n\n"
    
    ; Origin Info
    status += "╔═══ ORIGIN ═══╗\n"
    status += "Previous World: " + PreviousWorld + "\n"
    If WorldFlavors && WorldIndex < WorldFlavors.Length
        status += "Memory: " + WorldFlavors[WorldIndex] + "\n"
    EndIf
    status += "Status: Reincarnated\n\n"
    
    ; System Status
    status += "╔═══ SYSTEM ═══╗\n"
    status += "Power Level: " + GetCurrentPowerName() + "\n"
    
    If AscendedAuraActive
        status += "Aura: ASCENDED ⚡\n"
    Else
        status += "Aura: Inactive\n"
    EndIf
    
    ; Show some skill values
    status += "\n╔═══ SKILLS ═══╗\n"
    status += "One-Handed: " + player.GetActorValue("OneHanded") as Int + "\n"
    status += "Destruction: " + player.GetActorValue("Destruction") as Int + "\n"
    status += "Sneak: " + player.GetActorValue("Sneak") as Int + "\n"
    status += "Smithing: " + player.GetActorValue("Smithing") as Int + "\n\n"
    
    ; Special bonuses based on origin world
    status += "╔═══ BONUSES ═══╗\n"
    If WorldIndex == 1 || WorldIndex == 2
        status += "✓ Isekai Knowledge: +Wisdom\n"
    ElseIf WorldIndex == 3
        status += "✓ Magic Affinity: +Magicka\n"
    ElseIf WorldIndex == 4
        status += "✓ Tech Mind: +Intelligence\n"
    ElseIf WorldIndex == 5
        status += "✓ Survival Instinct: +Stamina\n"
    Else
        status += "✓ Adaptability: +Luck\n"
    EndIf
    
    If AscendedAuraActive
        status += "✓ Ascension: ACTIVE\n"
    EndIf
    
    status += "\n" + BuildSystemFooter()
    
    Debug.MessageBox(status)
EndFunction

; Show detailed stats
Function ShowDetailedStats()
    Actor player = Game.GetPlayer()
    If !player
        Return
    EndIf
    
    String stats = "╔══════ DETAILED STATUS ══════╗\n\n"
    
    ; Combat Skills
    stats += "═══ COMBAT ═══\n"
    stats += "One-Handed: " + player.GetActorValue("OneHanded") as Int + "\n"
    stats += "Two-Handed: " + player.GetActorValue("TwoHanded") as Int + "\n"
    stats += "Archery: " + player.GetActorValue("Archery") as Int + "\n"
    stats += "Block: " + player.GetActorValue("Block") as Int + "\n"
    stats += "Smithing: " + player.GetActorValue("Smithing") as Int + "\n\n"
    
    ; Magic Skills
    stats += "═══ MAGIC ═══\n"
    stats += "Destruction: " + player.GetActorValue("Destruction") as Int + "\n"
    stats += "Restoration: " + player.GetActorValue("Restoration") as Int + "\n"
    stats += "Conjuration: " + player.GetActorValue("Conjuration") as Int + "\n"
    stats += "Alteration: " + player.GetActorValue("Alteration") as Int + "\n"
    stats += "Illusion: " + player.GetActorValue("Illusion") as Int + "\n"
    stats += "Enchanting: " + player.GetActorValue("Enchanting") as Int + "\n\n"
    
    ; Stealth Skills
    stats += "═══ STEALTH ═══\n"
    stats += "Sneak: " + player.GetActorValue("Sneak") as Int + "\n"
    stats += "Lockpicking: " + player.GetActorValue("Lockpicking") as Int + "\n"
    stats += "Pickpocket: " + player.GetActorValue("Pickpocket") as Int + "\n"
    stats += "Speech: " + player.GetActorValue("Speech") as Int + "\n"
    stats += "Alchemy: " + player.GetActorValue("Alchemy") as Int + "\n\n"
    
    ; Attributes
    stats += "═══ ATTRIBUTES ═══\n"
    stats += "Health: " + player.GetActorValue("Health") as Int + "\n"
    stats += "Magicka: " + player.GetActorValue("Magicka") as Int + "\n"
    stats += "Stamina: " + player.GetActorValue("Stamina") as Int + "\n"
    stats += "Carry Weight: " + player.GetActorValue("CarryWeight") as Int + "\n\n"
    
    stats += "╚════════════════════════════════╝"
    
    Debug.MessageBox(stats)
EndFunction

; ============================================
; SYSTEM MENU HELPER
; ============================================

Int Function ShowSystemMenu(String title, String description, String[] options, String[] details, Int defaultIndex)
    If UIExtensionsInstalled
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
    Return -1
EndFunction

; ============================================
; UTILITY FUNCTIONS
; ============================================

String Function BuildSystemHeader(String title)
    Return "╔═══ 「 " + title + " 」 ═══╗"
EndFunction

String Function BuildSystemFooter()
    Return "╚═══ [WORLD SYSTEM] ═══╝"
EndFunction

String Function GetPowerLevelName(Int level)
    If level == 0
        Return "NORMAL"
    ElseIf level == 1
        Return "HERO"
    ElseIf level == 2
        Return "ASCENDED"
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

String Function GetCurrentPowerName()
    If MainQuest
        Return GetPowerLevelName(MainQuest.ChosenPowerLevel)
    EndIf
    Return "UNKNOWN"
EndFunction

; Change origin world (for MCM or debug)
Function SetPreviousWorld(String worldName)
    PreviousWorld = worldName
    Debug.Notification("[SYSTEM] Origin updated: " + worldName)
EndFunction

; Toggle God Mode effects
Function ToggleGodModeAura(Bool active)
    GodModeAuraActive = active
    If active
        Debug.Notification("[SYSTEM] ⚡ God Mode Aura activated")
        If MAGIllusionNightEyeAuto
            MAGIllusionNightEyeAuto.Play(Game.GetPlayer())
        EndIf
    Else
        Debug.Notification("[SYSTEM] God Mode Aura deactivated")
    EndIf
EndFunction

; Show System help
Function ShowSystemHelp()
    String help = BuildSystemHeader("SYSTEM HELP") + "\n\n"
    help += "Welcome to the Isekai Hero System!\n\n"
    help += "POWER LEVELS:\n"
    help += "• NORMAL: Vanilla Skyrim experience\n"
    help += "• HERO: Max skills, 50 perks\n"
    help += "• GOD MODE: Max everything, 500 perks\n\n"
    help += "ORIGIN WORLDS:\n"
    help += "Each world gives unique flavor text\n"
    help += "and minor bonuses to your journey.\n\n"
    help += "STATUS WINDOW:\n"
    help += "Check your System stats anytime\n"
    help += "via the MCM menu.\n\n"
    help += BuildSystemFooter()
    
    Debug.MessageBox(help)
EndFunction