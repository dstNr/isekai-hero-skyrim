; IsekaiPowerScript.psc
; Handles skill application and equipment distribution

Scriptname IsekaiPowerScript extends Quest

; All 18 skill AVs
ActorValue Property OneHanded Auto
ActorValue Property TwoHanded Auto
ActorValue Property Marksman Auto
ActorValue Property Block Auto
ActorValue Property Smithing Auto
ActorValue Property HeavyArmor Auto
ActorValue Property LightArmor Auto
ActorValue Property Pickpocket Auto
ActorValue Property Lockpicking Auto
ActorValue Property Sneak Auto
ActorValue Property Alchemy Auto
ActorValue Property Speechcraft Auto
ActorValue Property Alteration Auto
ActorValue Property Conjuration Auto
ActorValue Property Destruction Auto
ActorValue Property Illusion Auto
ActorValue Property Restoration Auto
ActorValue Property Enchanting Auto

; Form lists for equipment
FormList Property Isekai_Equipment_Humble Auto
FormList Property Isekai_Equipment_Adventurer Auto
FormList Property Isekai_Equipment_Hero Auto

; Apply chosen power level
Function ApplyPowerLevel(Int powerLevel, Int skillFocus)
    If powerLevel == 0
        Return ; Normal - do nothing
    EndIf
    
    Actor player = Game.GetPlayer()
    
    ; Apply base skill levels
    If skillFocus == 0
        ; All equal
        SetAllSkills(player, 100)
    ElseIf skillFocus == 1
        ; Warrior build
        SetWarriorSkills(player, 100)
    ElseIf skillFocus == 2
        ; Mage build
        SetMageSkills(player, 100)
    ElseIf skillFocus == 3
        ; Thief build
        SetThiefSkills(player, 100)
    Else
        ; Custom - all equal for now
        SetAllSkills(player, 100)
    EndIf
    
    ; Set level based on power choice
    If powerLevel == 2
        ; God Mode - Level 255 (max)
        player.SetLevel(255)
        ; Give plenty of perk points for vanilla + custom skill trees
        player.AddPerkPoints(500)
    ElseIf powerLevel == 1
        ; Hero - Level 1 but skills maxed
        player.SetLevel(1)
        ; Some perk points to distribute
        player.AddPerkPoints(50)
    EndIf
EndFunction

; Set all skills to value
Function SetAllSkills(Actor player, Int value)
    player.SetActorValue("OneHanded", value)
    player.SetActorValue("TwoHanded", value)
    player.SetActorValue("Marksman", value)
    player.SetActorValue("Block", value)
    player.SetActorValue("Smithing", value)
    player.SetActorValue("HeavyArmor", value)
    player.SetActorValue("LightArmor", value)
    player.SetActorValue("Pickpocket", value)
    player.SetActorValue("Lockpicking", value)
    player.SetActorValue("Sneak", value)
    player.SetActorValue("Alchemy", value)
    player.SetActorValue("Speechcraft", value)
    player.SetActorValue("Alteration", value)
    player.SetActorValue("Conjuration", value)
    player.SetActorValue("Destruction", value)
    player.SetActorValue("Illusion", value)
    player.SetActorValue("Restoration", value)
    player.SetActorValue("Enchanting", value)
EndFunction

; Set warrior skills to value, others lower
Function SetWarriorSkills(Actor player, Int value)
    player.SetActorValue("OneHanded", value)
    player.SetActorValue("TwoHanded", value)
    player.SetActorValue("Marksman", value)
    player.SetActorValue("Block", value)
    player.SetActorValue("Smithing", value)
    player.SetActorValue("HeavyArmor", value)
    player.SetActorValue("LightArmor", value)
    ; Others at 50
    SetOtherSkills(player, 50)
EndFunction

; Set mage skills to value, others lower
Function SetMageSkills(Actor player, Int value)
    player.SetActorValue("Alteration", value)
    player.SetActorValue("Conjuration", value)
    player.SetActorValue("Destruction", value)
    player.SetActorValue("Illusion", value)
    player.SetActorValue("Restoration", value)
    player.SetActorValue("Enchanting", value)
    ; Others at 50
    SetOtherSkills(player, 50)
EndFunction

; Set thief skills to value, others lower
Function SetThiefSkills(Actor player, Int value)
    player.SetActorValue("Marksman", value)
    player.SetActorValue("LightArmor", value)
    player.SetActorValue("Pickpocket", value)
    player.SetActorValue("Lockpicking", value)
    player.SetActorValue("Sneak", value)
    player.SetActorValue("Alchemy", value)
    player.SetActorValue("Speechcraft", value)
    ; Others at 50
    SetOtherSkills(player, 50)
EndFunction

; Set non-main skills to value
Function SetOtherSkills(Actor player, Int value)
    ; Already set by specific functions
EndFunction

; Give equipment based on choice
Function GiveEquipment(Int choice)
    Actor player = Game.GetPlayer()
    
    If choice == 0
        ; Humble - iron armor, basic supplies
        GiveHumbleGear(player)
    ElseIf choice == 1
        ; Adventurer - steel, potions, gold
        GiveAdventurerGear(player)
    ElseIf choice == 2
        ; Hero - legendary items
        GiveHeroGear(player)
    ElseIf choice == 3
        ; None - pure skill
        Return
    EndIf
EndFunction

; Give humble equipment
Function GiveHumbleGear(Actor player)
    ; Iron armor set
    ; Iron sword
    ; Basic food and water
    ; 100 gold
    player.AddItem(Game.GetForm(0x00012E4E), 1) ; Iron Sword
    player.AddItem(Game.GetForm(0x00012E4D), 1) ; Iron Armor
    player.AddItem(Game.GetForm(0x000F6ADE), 100) ; Gold
EndFunction

; Give adventurer equipment
Function GiveAdventurerGear(Actor player)
    ; Steel armor set
    ; Steel weapon
    ; Health/Magicka potions
    ; 500 gold
    player.AddItem(Game.GetForm(0x0001398C), 1) ; Steel Sword
    player.AddItem(Game.GetForm(0x00013958), 1) ; Steel Armor
    player.AddItem(Game.GetForm(0x000F6ADE), 500) ; Gold
    ; Potions
    player.AddItem(Game.GetForm(0x0003EADE), 5) ; Potion of Health
    player.AddItem(Game.GetForm(0x0003EADA), 3) ; Potion of Magicka
EndFunction

; Give hero equipment
Function GiveHeroGear(Actor player)
    ; Legendary/Daedric items
    ; Powerful enchanted gear
    ; 2000 gold
    player.AddItem(Game.GetForm(0x000139B8), 1) ; Daedric Sword
    player.AddItem(Game.GetForm(0x0001396A), 1) ; Daedric Armor
    player.AddItem(Game.GetForm(0x000F6ADE), 2000) ; Gold
    ; Powerful potions
    player.AddItem(Game.GetForm(0x00039BE5), 10) ; Potion of Ultimate Health
    player.AddItem(Game.GetForm(0x00039BE7), 10) ; Potion of Ultimate Magicka
    player.AddItem(Game.GetForm(0x00039BE6), 10) ; Potion of Ultimate Stamina
EndFunction
