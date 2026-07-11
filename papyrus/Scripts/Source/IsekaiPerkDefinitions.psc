; IsekaiPerkDefinitions.psc
; Isekai-specific perks for the perk tree
; These are passive bonuses that fit the Isekai theme

Scriptname IsekaiPerkDefinitions extends Quest

; ============================================
; ISEKAI PERK TREE STRUCTURE
; ============================================
;
; The Isekai perk tree has 3 branches:
; 1. REINCARNATED SOUL (General bonuses)
; 2. DIMENSIONAL KNOWLEDGE (Magic/Skills)
; 3. SYSTEM PROTECTION (Combat/Defense)
;
; ============================================

; ============================================
; REINCARNATED SOUL BRANCH
; ============================================

; Tier 1: Past Life Memories
Perk Property Isekai_PastLifeMemories Auto
; Effect: +10% XP gain from all sources
; Requirement: None (first perk)
; Cost: 1 perk point

; Tier 2: Quick Learner
Perk Property Isekai_QuickLearner Auto
; Effect: +20% skill increase rate
; Requirement: Past Life Memories
; Cost: 1 perk point

; Tier 3: Prodigy
Perk Property Isekai_Prodigy Auto
; Effect: +50% XP gain, skills level 20% faster
; Requirement: Quick Learner, Level 25
; Cost: 2 perk points

; Tier 4: Transcendent Being
Perk Property Isekai_Transcendent Auto
; Effect: +100% XP gain, all skills legendary reset available
; Requirement: Prodigy, Level 50
; Cost: 3 perk points

; ============================================
; DIMENSIONAL KNOWLEDGE BRANCH
; ============================================

; Tier 1: Otherworldly Insight
Perk Property Isekai_OtherworldlyInsight Auto
; Effect: +20 Magicka, spells cost 5% less
; Requirement: None
; Cost: 1 perk point

; Tier 2: Arcane Understanding
Perk Property Isekai_ArcaneUnderstanding Auto
; Effect: +50 Magicka, spell duration +20%
; Requirement: Otherworldly Insight
; Cost: 1 perk point

; Tier 3: Dimensional Storage
Perk Property Isekai_DimensionalStorage Auto
; Effect: +100 carry weight, can access storage anywhere (future feature)
; Requirement: Arcane Understanding
; Cost: 2 perk points

; Tier 4: Reality Manipulation
Perk Property Isekai_RealityManipulation Auto
; Effect: +150 Magicka, all spells cost 25% less, dual casting 50% stronger
; Requirement: Dimensional Storage, Level 50
; Cost: 3 perk points

; ============================================
; SYSTEM PROTECTION BRANCH
; ============================================

; Tier 1: System Shield
Perk Property Isekai_SystemShield Auto
; Effect: +20 Health, 5% damage resistance
; Requirement: None
; Cost: 1 perk point

; Tier 2: Pain Suppression
Perk Property Isekai_PainSuppression Auto
; Effect: +50 Health, 10% damage resistance, stagger 50% less
; Requirement: System Shield
; Cost: 1 perk point

; Tier 3: Regeneration
Perk Property Isekai_Regeneration Auto
; Effect: Health regenerates 100% faster, +20% disease resistance
; Requirement: Pain Suppression
; Cost: 2 perk points

; Tier 4: Immortal Vessel
Perk Property Isekai_ImmortalVessel Auto
; Effect: +200 Health, 25% damage resistance, cannot be disarmed
; Requirement: Regeneration, Level 50
; Cost: 3 perk points

; ============================================
; ASCENDED EXCLUSIVE PERKS
; ============================================
; These only appear if Ascended mode was chosen

; Ultimate: World System Administrator
Perk Property Isekai_SystemAdmin Auto
; Effect: All previous perks combined + unique bonuses
; +500 Health/Magicka/Stamina
; 50% damage resistance
; 50% magic resistance
; 100% XP gain
; Can only be taken at Level 100
; Cost: 5 perk points

; ============================================
; PERK APPLICATION FUNCTIONS
; ============================================

Function ApplyIsekaiPerk(Actor player, Perk isekaiPerk)
    If !isekaiPerk
        Return
    EndIf
    
    If !player.HasPerk(isekaiPerk)
        player.AddPerk(isekaiPerk)
        Debug.Notification("[SYSTEM] New ability unlocked: " + isekaiPerk.GetName())
    EndIf
EndFunction

Function RemoveIsekaiPerk(Actor player, Perk isekaiPerk)
    If !isekaiPerk
        Return
    EndIf
    
    If player.HasPerk(isekaiPerk)
        player.RemovePerk(isekaiPerk)
    EndIf
EndFunction

; ============================================
; AUTO-GRANT PERKS BASED ON CHOICES
; ============================================

Function GrantStartingPerks(Actor player, Int powerLevel, Int skillFocus)
    ; Everyone gets Past Life Memories
    If Isekai_PastLifeMemories
        ApplyIsekaiPerk(player, Isekai_PastLifeMemories)
    EndIf
    
    ; Skill focus determines second perk
    If skillFocus == 2 ; Mage
        If Isekai_OtherworldlyInsight
            ApplyIsekaiPerk(player, Isekai_OtherworldlyInsight)
        EndIf
    ElseIf skillFocus == 1 || skillFocus == 3 ; Warrior or Thief
        If Isekai_SystemShield
            ApplyIsekaiPerk(player, Isekai_SystemShield)
        EndIf
    Else ; Balanced or Custom - let them choose
        ; Give both tier 1 perks from other branches
        If Isekai_OtherworldlyInsight
            ApplyIsekaiPerk(player, Isekai_OtherworldlyInsight)
        EndIf
        If Isekai_SystemShield
            ApplyIsekaiPerk(player, Isekai_SystemShield)
        EndIf
    EndIf
    
    ; Ascended gets bonus perks
    If powerLevel == 2
        If Isekai_QuickLearner
            ApplyIsekaiPerk(player, Isekai_QuickLearner)
        EndIf
        If Isekai_ArcaneUnderstanding
            ApplyIsekaiPerk(player, Isekai_ArcaneUnderstanding)
        EndIf
        If Isekai_PainSuppression
            ApplyIsekaiPerk(player, Isekai_PainSuppression)
        EndIf
        
        Debug.Notification("[SYSTEM] Ascended perks granted!")
    EndIf
EndFunction

; ============================================
; PERK DESCRIPTIONS (FOR MCM/INFO)
; ============================================

String Function GetPerkDescription(Perk isekaiPerk)
    If isekaiPerk == Isekai_PastLifeMemories
        Return "Memories from your past life accelerate learning.\n+10% XP gain"
    ElseIf isekaiPerk == Isekai_QuickLearner
        Return "Your soul adapts quickly to new skills.\n+20% skill increase rate"
    ElseIf isekaiPerk == Isekai_Prodigy
        Return "You are a prodigy among mortals.\n+50% XP gain, skills level 20% faster"
    ElseIf isekaiPerk == Isekai_Transcendent
        Return "You have transcended mortal limits.\n+100% XP gain, legendary skills"
    ElseIf isekaiPerk == Isekai_OtherworldlyInsight
        Return "Knowledge from another world enhances magic.\n+20 Magicka, -5% spell cost"
    ElseIf isekaiPerk == Isekai_ArcaneUnderstanding
        Return "Arcane secrets from your past life.\n+50 Magicka, +20% spell duration"
    ElseIf isekaiPerk == Isekai_DimensionalStorage
        Return "Access to a pocket dimension.\n+100 carry weight"
    ElseIf isekaiPerk == Isekai_RealityManipulation
        Return "You can bend reality itself.\n+150 Magicka, -25% spell cost, +50% dual cast"
    ElseIf isekaiPerk == Isekai_SystemShield
        Return "The System protects its chosen.\n+20 Health, 5% damage resistance"
    ElseIf isekaiPerk == Isekai_PainSuppression
        Return "Pain is just data to be ignored.\n+50 Health, 10% resistance, -50% stagger"
    ElseIf isekaiPerk == Isekai_Regeneration
        Return "Your body regenerates at supernatural speeds.\n+100% health regen, +20% disease resist"
    ElseIf isekaiPerk == Isekai_ImmortalVessel
        Return "Your vessel is nearly immortal.\n+200 Health, 25% resistance, cannot be disarmed"
    ElseIf isekaiPerk == Isekai_SystemAdmin
        Return "You are the Administrator of this world.\nUltimate power unlocked"
    EndIf
    
    Return "Unknown perk"
EndFunction

; ============================================
; REQUIRED PERK POINTS
; ============================================

Int Function GetPerkCost(Perk isekaiPerk)
    If isekaiPerk == Isekai_PastLifeMemories || isekaiPerk == Isekai_OtherworldlyInsight || isekaiPerk == Isekai_SystemShield
        Return 1
    ElseIf isekaiPerk == Isekai_QuickLearner || isekaiPerk == Isekai_ArcaneUnderstanding || isekaiPerk == Isekai_PainSuppression
        Return 1
    ElseIf isekaiPerk == Isekai_Prodigy || isekaiPerk == Isekai_DimensionalStorage || isekaiPerk == Isekai_Regeneration
        Return 2
    ElseIf isekaiPerk == Isekai_Transcendent || isekaiPerk == Isekai_RealityManipulation || isekaiPerk == Isekai_ImmortalVessel
        Return 3
    ElseIf isekaiPerk == Isekai_SystemAdmin
        Return 5
    EndIf
    Return 1
EndFunction
