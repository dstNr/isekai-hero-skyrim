#pragma once

#include "System.h"  // PowerLevel

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
        kPerkPoint,        // +5 perk points — REPEATABLE, never enters unlockedNodes
        kMoveSpeed,        // +N% movement speed per rank (kSpeedMult, set directly —
                           // no fortify ability exists for it) — REPEATABLE
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
        std::int32_t  cost;  // System Points
        PowerLevel    minPower;   // minimum rebirth tier that may unlock this node.
                                  // The System's gifts deepen with the blessing:
                                  // NORMAL = self-made stats, HERO += omniscience,
                                  // ASCENDED += the capstone. Higher tiers include all
                                  // lower ones (PowerLevel is ordered Normal<Hero<Ascended).
        std::uint32_t prereq[2];  // node keys; 0 = none. All listed must be unlocked.
        Effect        effect;
        Bonus         bonus[3];

        // REPEATABLE nodes can be bought again and again: they never enter
        // unlockedNodes (so they keep pulsing as buyable) — their purchase count lives
        // in State::nodeRanks and drives the accumulated bonus. maxRank caps it (0 =
        // uncapped). One-shot nodes leave both at their defaults.
        bool         repeatable = false;
        std::int32_t maxRank = 0;
    };

    // The whole tree, for the window to draw.
    [[nodiscard]] const Node* Nodes(std::size_t& a_count);

    [[nodiscard]] bool IsUnlocked(std::uint32_t a_key);
    [[nodiscard]] bool PrereqsMet(std::uint32_t a_key);

    // How many times a REPEATABLE node has been bought (0 for one-shot or unbought).
    // Drives the hover readout ("RANK 3 / 10") and the accumulated bonus.
    [[nodiscard]] std::int32_t Rank(std::uint32_t a_key);

    // Is the player's rebirth tier high enough to unlock this node? Gates the strong
    // "gift" nodes (omniscience for HERO+, capstone for ASCENDED) behind the blessing.
    [[nodiscard]] bool TierMet(std::uint32_t a_key);

    // The minimum rebirth tier a node needs — for the "requires HERO rebirth" hint.
    [[nodiscard]] PowerLevel RequiredPower(std::uint32_t a_key);

    // The tree's currency: System Points (docs/IDEAS.md) — paid by milestones,
    // seeded by the blessing, uncapped. Dragon souls stay out of the tree entirely.
    [[nodiscard]] std::int32_t Points();

    // The player's unspent perk points (engine-capped at 255, unsigned) — shown in the tree
    // header because the Perk Synthesis node feeds this pool.
    [[nodiscard]] std::int32_t PerkPool();

    // Spend points and unlock. Main thread only. False if locked/unaffordable/unknown.
    bool TryUnlock(std::uint32_t a_key);

    // How many System Points a respec would hand back: the cost of every node whose
    // effect can actually be undone. 0 when there is nothing to refund.
    //
    // Deliberately NOT refundable, and left untouched by Respec():
    //   * the four Omniscience unlocks — they write knowledge the player keeps (known
    //     flags on shouts/enchantments/ingredients, learned spells). Taking that back
    //     would also erase what they learned legitimately.
    //   * Perk Synthesis — its points already became perk points, most likely spent.
    [[nodiscard]] std::int32_t RespecRefund();

    // Refund those nodes and clear them, reverting their effects. Main thread only.
    // False if there was nothing to give back.
    bool Respec();

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
