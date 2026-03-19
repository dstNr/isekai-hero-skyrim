; IsekaiDialogScript.psc
; Handles dialog messages and player choices

Scriptname IsekaiDialogScript extends Quest

; Reference to main quest
IsekaiIntroQuest Property MainQuest Auto

; Message forms (will be created in ESP)
Message Property IsekaiMsg_PowerChoice Auto
Message Property IsekaiMsg_SkillFocus Auto  
Message Property IsekaiMsg_EquipmentChoice Auto
Message Property IsekaiMsg_Confirmation Auto

; Button texts for dialogs
String[] PowerButtons
String[] SkillButtons
String[] EquipmentButtons

; Called when script initializes
Event OnInit()
    ; Initialize button arrays
    PowerButtons = new String[4]
    PowerButtons[0] = "Normal"
    PowerButtons[1] = "Hero"
    PowerButtons[2] = "God Mode"
    PowerButtons[3] = "Cancel"
    
    SkillButtons = new String[6]
    SkillButtons[0] = "All Equal"
    SkillButtons[1] = "Warrior"
    SkillButtons[2] = "Mage"
    SkillButtons[3] = "Thief"
    SkillButtons[4] = "Custom (MCM later)"
    SkillButtons[5] = "Cancel"
    
    EquipmentButtons = new String[5]
    EquipmentButtons[0] = "Humble Beginnings"
    EquipmentButtons[1] = "Adventurer's Kit"
    EquipmentButtons[2] = "Hero Gear"
    EquipmentButtons[3] = "None"
    EquipmentButtons[4] = "Cancel"
EndEvent

; Show power level choice dialog
Function ShowPowerChoice()
    String title = "Isekai Awakening"
    String text = "You feel strange energy flowing through your veins...\n\n" + \
                  "Choose your destiny:\n\n" + \
                  "[Normal] - Start as a regular adventurer (Vanilla)\n" + \
                  "[Hero] - Start with max skills (Level 1, Skills 100, 50 perks)\n" + \
                  "[God Mode] - True Isekai power (Level 255, 500 perks)"
    
    Int result = ShowMessageDialog(title, text, PowerButtons)
    
    If result == 3 ; Cancel
        MainQuest.OnPowerChosen(0) ; Default to normal
    Else
        MainQuest.OnPowerChosen(result)
    EndIf
EndFunction

; Show skill focus dialog
Function ShowSkillFocus()
    String title = "Distribute Your Potential"
    String text = "How do you wish to focus your abilities?\n\n" + \
                  "[All Equal] - 100 in every skill\n" + \
                  "[Warrior] - One/Two-Handed, Block, Heavy Armor, Smithing\n" + \
                  "[Mage] - Destruction, Restoration, Conjuration, Alteration\n" + \
                  "[Thief] - Sneak, Lockpicking, Pickpocket, Light Armor\n" + \
                  "[Custom] - Configure later via MCM"
    
    Int result = ShowMessageDialog(title, text, SkillButtons)
    
    If result == 5 ; Cancel
        MainQuest.OnSkillFocusChosen(0) ; Default to all equal
    Else
        MainQuest.OnSkillFocusChosen(result)
    EndIf
EndFunction

; Show equipment choice dialog
Function ShowEquipment()
    String title = "Choose Your Equipment"
    String text = "What gear do you bring to this world?\n\n" + \
                  "[Humble] - Iron armor, basic supplies\n" + \
                  "[Adventurer] - Steel gear, potions, gold\n" + \
                  "[Hero] - Legendary items, enchanted\n" + \
                  "[None] - Pure skill only"
    
    Int result = ShowMessageDialog(title, text, EquipmentButtons)
    
    If result == 4 ; Cancel
        MainQuest.OnEquipmentChosen(0) ; Default to humble
    Else
        MainQuest.OnEquipmentChosen(result)
    EndIf
EndFunction

; Helper function for message box
Int Function ShowMessageDialog(String title, String text, String[] buttons)
    ; Use Debug.MessageBox for compatibility
    ; Returns button index (0-based)
    
    String fullText = "[" + title + "]\n\n" + text
    
    ; Build button string
    String buttonText = buttons[0]
    Int i = 1
    While i < buttons.Length
        buttonText += "\n" + buttons[i]
        i += 1
    EndWhile
    
    Int result = Debug.MessageBox(fullText, buttonText)
    Return result
EndFunction

; Alternative using vanilla message system
Int Function ShowMessageForm(Message msg)
    Return msg.Show()
EndFunction
