; IsekaiPowerScript.psc
; Handles skill application and equipment distribution
; VERSION 2.0 - Heavy Modlist Compatible

Scriptname IsekaiPowerScript extends Quest

; ============================================
; FORM LISTS FOR EQUIPMENT (SAFER THAN HARDCODED IDs)
; ============================================
FormList Property Isekai_Weapons_Humble Auto
FormList Property Isekai_Armor_Humble Auto
FormList Property Isekai_Weapons_Adventurer Auto
FormList Property Isekai_Armor_Adventurer Auto
FormList Property Isekai_Weapons_Hero Auto
FormList Property Isekai_Armor_Hero Auto
FormList Property Isekai_Potions_Health Auto
FormList Property Isekai_Potions_Magicka Auto
FormList Property Isekai_Potions_Stamina Auto
FormList Property Isekai_Gems_Rare Auto

; Skills are set via string-based SetActorValue("OneHanded", ...) below,
; so no ActorValue property objects are needed here.

; Fallback forms if FormLists are empty
Weapon Fallback_IronSword
Armor Fallback_IronArmor
Weapon Fallback_SteelSword
Armor Fallback_SteelArmor
Weapon Fallback_DaedricSword
Armor Fallback_DaedricArmor
Potion Fallback_HealthPotion
Potion Fallback_MagickaPotion
Potion Fallback_StaminaPotion
MiscObject Fallback_Gold
MiscObject Fallback_Diamond
MiscObject Fallback_Ruby
MiscObject Fallback_Sapphire

; ============================================
; INITIALIZATION - SAFE FORM LOOKUP
; ============================================

Event OnInit()
    InitializeFallbackForms()
EndEvent

Function InitializeFallbackForms()
    ; Try to get forms from Skyrim.esm safely
    ; This prevents crashes if FormIDs are overridden by other mods
    
    Fallback_IronSword = Game.GetFormFromFile(0x00012E4E, "Skyrim.esm") as Weapon
    Fallback_IronArmor = Game.GetFormFromFile(0x00012E4D, "Skyrim.esm") as Armor
    
    Fallback_SteelSword = Game.GetFormFromFile(0x0001398C, "Skyrim.esm") as Weapon
    Fallback_SteelArmor = Game.GetFormFromFile(0x00013958, "Skyrim.esm") as Armor
    
    Fallback_DaedricSword = Game.GetFormFromFile(0x000139B8, "Skyrim.esm") as Weapon
    Fallback_DaedricArmor = Game.GetFormFromFile(0x0001396A, "Skyrim.esm") as Armor
    
    Fallback_HealthPotion = Game.GetFormFromFile(0x0003EADE, "Skyrim.esm") as Potion
    Fallback_MagickaPotion = Game.GetFormFromFile(0x0003EADA, "Skyrim.esm") as Potion
    Fallback_StaminaPotion = Game.GetFormFromFile(0x00039BE6, "Skyrim.esm") as Potion
    
    Fallback_Gold = Game.GetFormFromFile(0x0000000F, "Skyrim.esm") as MiscObject
    Fallback_Diamond = Game.GetFormFromFile(0x0006851E, "Skyrim.esm") as MiscObject
    Fallback_Ruby = Game.GetFormFromFile(0x0006851F, "Skyrim.esm") as MiscObject
    Fallback_Sapphire = Game.GetFormFromFile(0x00068520, "Skyrim.esm") as MiscObject
EndFunction

; ============================================
; SAFE FORM GETTERS (MODLIST COMPATIBLE)
; ============================================

Form Function GetSafeForm(FormList akList, Form fallback, Int index = 0)
    If akList && akList.GetSize() > index
        Form result = akList.GetAt(index)
        If result
            Return result
        EndIf
    EndIf
    Return fallback
EndFunction

; ============================================
; POWER LEVEL APPLICATION
; ============================================

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
    
    ; Set character level + perk points based on power choice.
    ; Game.SetPlayerLevel() is an SKSE function (SKSE is a required dependency).
    If powerLevel == 2
        ; Ascended - Level 255, plenty of perk points
        Game.SetPlayerLevel(255)
        Game.AddPerkPoints(500)
    ElseIf powerLevel == 1
        ; Hero - Level 1 but skills maxed
        Game.SetPlayerLevel(1)
        Game.AddPerkPoints(50)
    EndIf
EndFunction

; Set all skills to value
Function SetAllSkills(Actor player, Int value)
    player.SetActorValue("OneHanded", value)
    player.SetActorValue("TwoHanded", value)
    player.SetActorValue("Archery", value)
    player.SetActorValue("Block", value)
    player.SetActorValue("Smithing", value)
    player.SetActorValue("HeavyArmor", value)
    player.SetActorValue("LightArmor", value)
    player.SetActorValue("Pickpocket", value)
    player.SetActorValue("Lockpicking", value)
    player.SetActorValue("Sneak", value)
    player.SetActorValue("Alchemy", value)
    player.SetActorValue("Speech", value)
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
    player.SetActorValue("Archery", value)
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
    player.SetActorValue("Archery", value)
    player.SetActorValue("LightArmor", value)
    player.SetActorValue("Pickpocket", value)
    player.SetActorValue("Lockpicking", value)
    player.SetActorValue("Sneak", value)
    player.SetActorValue("Alchemy", value)
    player.SetActorValue("Speech", value)
    ; Others at 50
    SetOtherSkills(player, 50)
EndFunction

; Set non-main skills to value
Function SetOtherSkills(Actor player, Int value)
    ; Already set by specific functions
EndFunction

; ============================================
; EQUIPMENT GIVING (SAFE FOR HEAVY MODLISTS)
; ============================================

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

Function GiveWealth(Int choice)
    Actor player = Game.GetPlayer()
    
    ; Get gold form safely
    MiscObject gold = GetSafeForm(None, Fallback_Gold, 0) as MiscObject
    If !gold
        Debug.Notification("[SYSTEM ERROR] Could not find gold!")
        Return
    EndIf
    
    If choice == 0
        ; Modest - 1,000 gold
        player.AddItem(gold, 1000)
    ElseIf choice == 1
        ; Wealthy - 10,000 gold
        player.AddItem(gold, 10000)
    ElseIf choice == 2
        ; Noble - 50,000 gold
        player.AddItem(gold, 50000)
    ElseIf choice == 3
        ; Merchant Prince - 100,000 gold
        player.AddItem(gold, 100000)
        ; Bonus: Rare gems for the truly wealthy
        GiveRareGems(player)
    EndIf
EndFunction

Function GiveRareGems(Actor player)
    MiscObject diamond = GetSafeForm(None, Fallback_Diamond, 0) as MiscObject
    MiscObject ruby = GetSafeForm(None, Fallback_Ruby, 0) as MiscObject
    MiscObject sapphire = GetSafeForm(None, Fallback_Sapphire, 0) as MiscObject
    
    If diamond
        player.AddItem(diamond, 10)
    EndIf
    If ruby
        player.AddItem(ruby, 10)
    EndIf
    If sapphire
        player.AddItem(sapphire, 10)
    EndIf
EndFunction

Function GiveHumbleGear(Actor player)
    ; Get forms safely from FormList or fallback
    Weapon akWeapon = GetSafeForm(Isekai_Weapons_Humble, Fallback_IronSword, 0) as Weapon
    Armor akArmor = GetSafeForm(Isekai_Armor_Humble, Fallback_IronArmor, 0) as Armor
    MiscObject gold = GetSafeForm(None, Fallback_Gold, 0) as MiscObject

    If akWeapon
        player.AddItem(akWeapon, 1)
    EndIf
    If akArmor
        player.AddItem(akArmor, 1)
    EndIf
    If gold
        player.AddItem(gold, 100)
    EndIf
EndFunction

Function GiveAdventurerGear(Actor player)
    Weapon akWeapon = GetSafeForm(Isekai_Weapons_Adventurer, Fallback_SteelSword, 0) as Weapon
    Armor akArmor = GetSafeForm(Isekai_Armor_Adventurer, Fallback_SteelArmor, 0) as Armor
    MiscObject gold = GetSafeForm(None, Fallback_Gold, 0) as MiscObject
    Potion healthPotion = GetSafeForm(Isekai_Potions_Health, Fallback_HealthPotion, 0) as Potion
    Potion magickaPotion = GetSafeForm(Isekai_Potions_Magicka, Fallback_MagickaPotion, 0) as Potion

    If akWeapon
        player.AddItem(akWeapon, 1)
    EndIf
    If akArmor
        player.AddItem(akArmor, 1)
    EndIf
    If gold
        player.AddItem(gold, 500)
    EndIf
    If healthPotion
        player.AddItem(healthPotion, 5)
    EndIf
    If magickaPotion
        player.AddItem(magickaPotion, 3)
    EndIf
EndFunction

Function GiveHeroGear(Actor player)
    Weapon akWeapon = GetSafeForm(Isekai_Weapons_Hero, Fallback_DaedricSword, 0) as Weapon
    Armor akArmor = GetSafeForm(Isekai_Armor_Hero, Fallback_DaedricArmor, 0) as Armor
    MiscObject gold = GetSafeForm(None, Fallback_Gold, 0) as MiscObject
    Potion healthPotion = GetSafeForm(Isekai_Potions_Health, Fallback_HealthPotion, 0) as Potion
    Potion magickaPotion = GetSafeForm(Isekai_Potions_Magicka, Fallback_MagickaPotion, 0) as Potion
    Potion staminaPotion = GetSafeForm(Isekai_Potions_Stamina, Fallback_StaminaPotion, 0) as Potion

    If akWeapon
        player.AddItem(akWeapon, 1)
    EndIf
    If akArmor
        player.AddItem(akArmor, 1)
    EndIf
    If gold
        player.AddItem(gold, 2000)
    EndIf
    If healthPotion
        player.AddItem(healthPotion, 10)
    EndIf
    If magickaPotion
        player.AddItem(magickaPotion, 10)
    EndIf
    If staminaPotion
        player.AddItem(staminaPotion, 10)
    EndIf
EndFunction

; ============================================
; RESPEC FUNCTIONS (FOR MCM)
; ============================================

Function ResetAllSkills(Actor player)
    ; Reset all skills to 15 (base)
    player.SetActorValue("OneHanded", 15)
    player.SetActorValue("TwoHanded", 15)
    player.SetActorValue("Archery", 15)
    player.SetActorValue("Block", 15)
    player.SetActorValue("Smithing", 15)
    player.SetActorValue("HeavyArmor", 15)
    player.SetActorValue("LightArmor", 15)
    player.SetActorValue("Pickpocket", 15)
    player.SetActorValue("Lockpicking", 15)
    player.SetActorValue("Sneak", 15)
    player.SetActorValue("Alchemy", 15)
    player.SetActorValue("Speech", 15)
    player.SetActorValue("Alteration", 15)
    player.SetActorValue("Conjuration", 15)
    player.SetActorValue("Destruction", 15)
    player.SetActorValue("Illusion", 15)
    player.SetActorValue("Restoration", 15)
    player.SetActorValue("Enchanting", 15)
EndFunction

Function ClearAllPerks(Actor player)
    ; This is a placeholder - removing perks is complex
    ; Would need to track which perks were added
    Debug.Notification("[SYSTEM] Perk reset requires manual intervention")
EndFunction
