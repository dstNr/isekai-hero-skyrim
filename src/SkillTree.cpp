#include "SkillTree.h"

#include "Passives.h"
#include "Sounds.h"
#include "System.h"

#include <algorithm>
#include <mutex>
#include <string>

namespace Isekai::SkillTree {

    namespace {
        constexpr const char* kIconDir = "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\";

        using AV = RE::ActorValue;

        // Layout: hub top-centre, three branches fanning out (Kraft left, Arkana
        // right, Schatten centre-down), capstone at the bottom. Coordinates are the
        // node centres in the tree canvas, designed at 1080p and scaled on draw.
        // minPower column staggers the tree by rebirth tier (see Node::minPower):
        //   NORMAL   — the whole self-made half: stats, resists, perk synthesis, Thu'um cd
        //   HERO     — + the four Omniscience gifts (shouts / enchants / ingredients / spells)
        //   ASCENDED — + the World Tree capstone
        constexpr auto kN = PowerLevel::Normal;
        constexpr auto kH = PowerLevel::Hero;
        constexpr auto kA = PowerLevel::Ascended;

        constexpr Node kNodes[] = {
            // --- Hub ---
            { 1, "System Core", "The System takes root.\n+25 Health, Magicka and Stamina.",
              "spells_01_frame.png", 550.0f, 110.0f, 5, kN, { 0, 0 }, Effect::kAttributes,
              { { AV::kHealth, 25.0f }, { AV::kMagicka, 25.0f }, { AV::kStamina, 25.0f } } },
            { 2, "Dragon's Voice", "Your Thu'um recovers faster.\n-20% shout cooldown.",
              "spells_10_frame.png", 780.0f, 110.0f, 15, kN, { 1, 0 }, Effect::kShoutCooldown, {} },
            { 14, "Perk Synthesis",
              "Condense a System Point into raw potential.\n+5 perk points per purchase. REPEATABLE.",
              "spells_21_frame.png", 320.0f, 110.0f, 1, kN, { 1, 0 }, Effect::kPerkPoint, {} },

            // --- Kraft (left) ---
            { 3, "Vital Surge", "+100 Health.",
              "spells_25_frame.png", 250.0f, 260.0f, 10, kN, { 1, 0 }, Effect::kAttributes,
              { { AV::kHealth, 100.0f } } },
            { 4, "Thu'um Omniscience",
              "The System pours every dragon's voice into you.\nAll shouts and words of power unlocked.",
              "spells_39_frame.png", 160.0f, 410.0f, 25, kH, { 3, 0 }, Effect::kAllShouts, {} },
            { 5, "Emberguard", "+25% Fire Resist.",
              "spells_12_frame.png", 340.0f, 410.0f, 15, kN, { 3, 0 }, Effect::kAttributes,
              { { AV::kResistFire, 25.0f } } },

            // --- Arkana (right) ---
            { 6, "Mana Well", "+100 Magicka.",
              "spells_15_frame.png", 850.0f, 260.0f, 10, kN, { 1, 0 }, Effect::kAttributes,
              { { AV::kMagicka, 100.0f } } },
            { 7, "Arcane Omniscience",
              "Every enchantment laid bare.\nAll enchantments known without disenchanting.",
              "spells_36_frame.png", 760.0f, 410.0f, 25, kH, { 6, 0 }, Effect::kAllEnchantments, {} },
            { 8, "Frostguard", "+25% Frost Resist.",
              "spells_16_frame.png", 940.0f, 410.0f, 15, kN, { 6, 0 }, Effect::kAttributes,
              { { AV::kResistFrost, 25.0f } } },
            { 9, "Spell Omniscience",
              "The System reads every tome ever written.\nAll spells with a spell tome learned.",
              "spells_37_frame.png", 850.0f, 555.0f, 40, kH, { 7, 0 }, Effect::kAllSpells, {} },

            // --- Schatten (centre-down) ---
            { 10, "Swift Blood", "+100 Stamina.",
              "spells_32_frame.png", 550.0f, 300.0f, 10, kN, { 1, 0 }, Effect::kAttributes,
              { { AV::kStamina, 100.0f } } },
            { 11, "Alchemical Insight",
              "Every ingredient gives up its secrets.\nAll ingredient effects known.",
              "spells_31_frame.png", 460.0f, 450.0f, 25, kH, { 10, 0 }, Effect::kAllIngredients, {} },
            { 12, "Plagueward", "+25% Disease Resist.",
              "spells_34_frame.png", 640.0f, 450.0f, 15, kN, { 10, 0 }, Effect::kAttributes,
              { { AV::kResistDisease, 25.0f } } },

            // --- Capstone ---
            // Rooted in the Schatten children directly above it, not in the far-away
            // branch entries: their long diagonals crossed the whole middle field and
            // grazed every node on the way. Short V-lines, zero crossings — and a
            // deeper gate for the capstone as a side effect. ASCENDED-only.
            { 13, "World Tree", "The System blossoms through your soul.\n+100 Health, Magicka and Stamina.",
              "spells_09_frame.png", 550.0f, 600.0f, 50, kA, { 11, 12 }, Effect::kAttributes,
              { { AV::kHealth, 100.0f }, { AV::kMagicka, 100.0f }, { AV::kStamina, 100.0f } } },
        };

        // Unlock state lives in State::unlockedNodes (co-save). The tree window reads
        // on the render thread while unlocks happen on the main thread — everything
        // that touches the vector goes through this lock.
        std::mutex g_mutex;

        [[nodiscard]] const Node* Find(std::uint32_t a_key) {
            for (const auto& node : kNodes) {
                if (node.key == a_key) {
                    return &node;
                }
            }
            return nullptr;
        }

        [[nodiscard]] bool IsUnlockedNoLock(std::uint32_t a_key) {
            const auto& unlocked = GetState().unlockedNodes;
            return std::find(unlocked.begin(), unlocked.end(), a_key) != unlocked.end();
        }

        // All knowledge unlocks deliberately span EVERY loaded plugin, mods included:
        // in a modded setup, "the System knows everything" should mean everything the
        // setup knows. The filters are semantic, not plugin lists — "has a name",
        // "has a tome", "has words" — so mod content qualifies by the same rules as
        // vanilla. This is safe where item-shuttling was not: setting a known-flag or
        // teaching a shout fires no container events into listening quest scripts.

        void UnlockAllShouts(RE::PlayerCharacter* a_player) {
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data) {
                return;
            }
            // One form per display name. Among several forms sharing a name we PREFER
            // the one with a description — that reliably picks the real player shout over
            // the NPC/dragon copies (those carry no description). But a description is no
            // longer a hard requirement: racial/special shouts like "Battle Cry" have no
            // description yet are perfectly real, and the old filter dropped them
            // entirely. So a name with only description-less forms still gets its lowest
            // FormID kept.
            //
            // Tiebreak is lowest FormID (the oldest, original definition). Why lowest and
            // not highest ("mods override by load order")? An override never creates a
            // second form: it keeps the original FormID, the engine resolves the conflict
            // at data load, and the ONE form we see already carries the winning mod's
            // data. Highest-vs-lowest only decides between genuinely DISTINCT records
            // sharing a name — and a same-named extra record is nearly always a special
            // variant (NPC/quest copy), not a replacement.
            struct Pick {
                RE::TESShout* shout;
                bool          described;
            };
            std::map<std::string, Pick> byName;
            for (auto* shout : data->GetFormArray<RE::TESShout>()) {
                if (!shout) {
                    continue;
                }
                const char* name = shout->GetName();
                if (!name || !*name) {
                    continue;
                }

                RE::BSString description;
                shout->GetDescription(description, nullptr);
                const bool described = !description.empty();

                auto [it, fresh] = byName.try_emplace(name, Pick{ shout, described });
                if (fresh) {
                    continue;
                }
                Pick&      cur = it->second;
                const bool better = (described && !cur.described) ||
                                    (described == cur.described &&
                                     shout->GetFormID() < cur.shout->GetFormID());
                if (better) {
                    cur = { shout, described };
                }
            }

            std::size_t knownShouts = 0;
            std::size_t words = 0;
            for (const auto& [name, pick] : byName) {
                auto* shout = pick.shout;
                a_player->AddShout(shout);
                for (const auto& variation : shout->variations) {
                    if (!variation.word) {
                        continue;
                    }
                    // Two-pronged, because UnlockWord (Actor vfunc 0xD0) turned out to
                    // be a no-op on the test setup — the words stayed "found but locked"
                    // and could not even be soul-unlocked, i.e. the game never saw them
                    // as KNOWN. So we also set the word's kKnown form flag directly, the
                    // exact mechanism that already works for enchantments above.
                    a_player->UnlockWord(variation.word);
                    variation.word->formFlags |= RE::TESForm::RecordFlags::kKnown;
                    ++words;
                }
                if (shout->GetKnown()) {
                    ++knownShouts;
                }
            }
            logger::info("SkillTree: {} unique shouts unlocked ({} known after AddShout, {} words marked)",
                         byName.size(), knownShouts, words);
        }

        void UnlockAllEnchantments() {
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data) {
                return;
            }
            const auto& all = data->GetFormArray<RE::EnchantmentItem>();

            // Learnable = referenced as some variant's baseEnchantment. The form list
            // is packed with levelled-loot tiers ("Fortify Health" x6) and NPC-only
            // enchantments; a name alone does not make something the enchanting table
            // should offer. What disenchanting actually teaches is the BASE a variant
            // points to — so the set of legitimate bases is exactly the set of forms
            // being pointed at. Same duplicate lesson as the shouts, different marker.
            std::size_t known = 0;
            for (auto* ench : all) {
                if (!ench || !ench->data.baseEnchantment) {
                    continue;
                }
                auto* base = ench->data.baseEnchantment;
                if (const char* name = base->GetName(); !name || !*name) {
                    continue;
                }
                if ((base->formFlags & RE::TESForm::RecordFlags::kKnown) == 0) {
                    base->formFlags |= RE::TESForm::RecordFlags::kKnown;
                    ++known;
                }
            }
            logger::info("SkillTree: {} base enchantments marked known", known);
        }

        void UnlockAllIngredients() {
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data) {
                return;
            }
            std::size_t learned = 0;
            for (auto* ingredient : data->GetFormArray<RE::IngredientItem>()) {
                if (!ingredient) {
                    continue;
                }
                if (ingredient->gamedata.knownEffectFlags != 0x000F) {
                    ingredient->gamedata.knownEffectFlags = 0x000F;
                    ++learned;
                }
            }
            logger::info("SkillTree: {} ingredients fully known", learned);
        }

        void UnlockAllSpells(RE::PlayerCharacter* a_player) {
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data) {
                return;
            }
            // "Has a spell tome" is the filter that separates player spells from the
            // thousands of NPC-only and test spells — if it can be bought and read,
            // the System can teach it.
            std::size_t spells = 0;
            for (auto* book : data->GetFormArray<RE::TESObjectBOOK>()) {
                if (!book || !book->TeachesSpell()) {
                    continue;
                }
                auto* spell = book->GetSpell();
                if (!spell) {
                    continue;
                }
                if (a_player->AddSpell(spell)) {
                    ++spells;
                }
            }
            logger::info("SkillTree: {} spells learned from tomes", spells);
        }

        // Make one node's effect real. Attribute nodes need no action here — their
        // numbers flow through Passives::Refresh via AccumulateBonuses.
        void ApplyEffect(const Node& a_node) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }
            switch (a_node.effect) {
            case Effect::kShoutCooldown:
                if (auto* avOwner = player->AsActorValueOwner()) {
                    avOwner->SetBaseActorValue(AV::kShoutRecoveryMult, 0.8f);
                }
                break;
            case Effect::kAllShouts:
                UnlockAllShouts(player);
                break;
            case Effect::kAllEnchantments:
                UnlockAllEnchantments();
                break;
            case Effect::kAllIngredients:
                UnlockAllIngredients();
                break;
            case Effect::kAllSpells:
                UnlockAllSpells(player);
                break;
            default:
                break;
            }
        }
    }

    const Node* Nodes(std::size_t& a_count) {
        a_count = std::size(kNodes);
        return kNodes;
    }

    bool IsUnlocked(std::uint32_t a_key) {
        std::scoped_lock lock(g_mutex);
        return IsUnlockedNoLock(a_key);
    }

    bool PrereqsMet(std::uint32_t a_key) {
        const auto* node = Find(a_key);
        if (!node) {
            return false;
        }
        std::scoped_lock lock(g_mutex);
        for (const auto prereq : node->prereq) {
            if (prereq != 0 && !IsUnlockedNoLock(prereq)) {
                return false;
            }
        }
        return true;
    }

    PowerLevel RequiredPower(std::uint32_t a_key) {
        const auto* node = Find(a_key);
        return node ? node->minPower : PowerLevel::Normal;
    }

    bool TierMet(std::uint32_t a_key) {
        const auto* node = Find(a_key);
        if (!node) {
            return false;
        }
        // PowerLevel is ordered Normal < Hero < Ascended, so a numeric compare is the
        // whole gate: a higher rebirth tier unlocks everything a lower one could.
        return static_cast<int>(GetState().power) >= static_cast<int>(node->minPower);
    }

    std::int32_t Points() {
        return GetState().systemPoints;
    }

    std::int32_t PerkPool() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        // Unsigned read — see kMaxPerkPoints in System.h for the -54 story.
        return player ? static_cast<std::int32_t>(
                            static_cast<std::uint8_t>(player->GetGameStatsData().perkCount))
                      : 0;
    }

    bool TryUnlock(std::uint32_t a_key) {
        const auto* node = Find(a_key);
        if (!node || IsUnlocked(a_key) || !PrereqsMet(a_key) || !TierMet(a_key)) {
            return false;
        }

        auto& state = GetState();
        if (state.systemPoints < node->cost) {
            return false;
        }

        // Perk Synthesis is repeatable: it never enters unlockedNodes (IsUnlocked
        // stays false, so it keeps pulsing as buyable), it just spends and pays out.
        // The payout shrinks near the engine cap instead of burning value — the last
        // purchase before a full pool grants whatever still fits.
        if (node->effect == Effect::kPerkPoint) {
            constexpr std::int32_t kPerksPerPoint = 5;
            const auto grant = std::min(kPerksPerPoint, kMaxPerkPoints - PerkPool());
            if (grant <= 0) {
                return false;
            }
            state.systemPoints -= node->cost;
            GrantPerkPoints(grant);
            Sounds::Play(Sounds::Sfx::ButtonClick);
            logger::info("SkillTree: synthesised {} perk point(s) for {} SP", grant, node->cost);
            return true;
        }

        state.systemPoints -= node->cost;

        {
            std::scoped_lock lock(g_mutex);
            state.unlockedNodes.push_back(a_key);
        }

        ApplyEffect(*node);
        Passives::Refresh();
        Sounds::Play(Sounds::Sfx::LevelUp);

        logger::info("SkillTree: unlocked '{}' for {} point(s)", node->name, node->cost);
        return true;
    }

    void ApplyOnLoad() {
        std::vector<std::uint32_t> unlocked;
        {
            std::scoped_lock lock(g_mutex);
            unlocked = GetState().unlockedNodes;
        }
        for (const auto key : unlocked) {
            if (const auto* node = Find(key)) {
                ApplyEffect(*node);
            }
        }
        if (!unlocked.empty()) {
            logger::info("SkillTree: re-applied {} node(s) from the save", unlocked.size());
        }
    }

    void AccumulateBonuses(std::map<RE::ActorValue, float>& a_totals) {
        std::scoped_lock lock(g_mutex);
        for (const auto& node : kNodes) {
            if (node.effect != Effect::kAttributes || !IsUnlockedNoLock(node.key)) {
                continue;
            }
            for (const auto& bonus : node.bonus) {
                if (bonus.av != AV::kNone) {
                    a_totals[bonus.av] += bonus.amount;
                }
            }
        }
    }
}
