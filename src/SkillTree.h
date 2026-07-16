#pragma once

#include <cstdint>
#include <map>

namespace Isekai::SkillTree {

    // What a node does when unlocked.
    enum class Effect {
        kAttributes,       // flat actor value bonuses, fed into Passives::Refresh
        kShoutCooldown,    // -20% shout recovery (kShoutRecoveryMult 1.0 -> 0.8)
        kAllShouts,        // every vanilla/DLC shout + all words of power
        kAllEnchantments,  // every enchantment known, no disenchanting needed
        kAllIngredients,   // all four effects of every ingredient known
        kAllSpells,        // every spell that has a tome
        kPerkPoint,        // +1 perk point — REPEATABLE, never enters unlockedNodes
    };

    struct Bonus {
        RE::ActorValue av = RE::ActorValue::kNone;  // kNone = unused slot
        float          amount = 0.0f;
    };

    struct Node {
        std::uint32_t key;   // stable across releases; never reuse a value
        const char*   name;
        const char*   desc;
        const char*   icon;  // file name under Data\SKSE\Plugins\IsekaiHero\icons
        float         x, y;  // layout coords in the tree canvas (1080p design space)
        std::int32_t  cost;  // dragon souls
        std::uint32_t prereq[2];  // node keys; 0 = none. All listed must be unlocked.
        Effect        effect;
        Bonus         bonus[3];
    };

    // The whole tree, for the window to draw.
    [[nodiscard]] const Node* Nodes(std::size_t& a_count);

    [[nodiscard]] bool IsUnlocked(std::uint32_t a_key);
    [[nodiscard]] bool PrereqsMet(std::uint32_t a_key);

    // The tree's currency: System Points (docs/IDEAS.md) — paid by milestones,
    // seeded by the blessing, uncapped. Dragon souls stay out of the tree entirely.
    [[nodiscard]] std::int32_t Points();

    // The player's unspent perk points (engine-capped at 127) — shown in the tree
    // header because the Perk Synthesis node feeds this pool.
    [[nodiscard]] std::int32_t PerkPool();

    // Spend points and unlock. Main thread only. False if locked/unaffordable/unknown.
    bool TryUnlock(std::uint32_t a_key);

    // Re-assert everything the unlocked nodes promise. Knowledge unlocks and the
    // shout-cooldown value do not live in the save the way items do — like the
    // ability magnitudes, they are re-derived from the node list on every load.
    void ApplyOnLoad();

    // Flat attribute totals from unlocked nodes, added on top of the milestone
    // passives by Passives::Refresh. Deliberately NOT scaled by the blessing:
    // ASCENDED already earns souls four times faster — scaling the payout too
    // would double-dip.
    void AccumulateBonuses(std::map<RE::ActorValue, float>& a_totals);
}
