#include "SkillTree.h"

#include "Passives.h"
#include "Sounds.h"
#include "System.h"
#include "UI/LevelUpEffect.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

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

        // Layout note: x/y are node centres in the 1120x760 design canvas, and they are
        // grouped into three vertical bands that the renderers frame and label as zones
        // (Zone::kMight / kShadow / kArcana). Keeping a node inside its band's x-range is
        // what makes the zone framing line up — moving one across bands without changing
        // its Zone will draw it outside its own header.
        //   MIGHT  x 210..390    SHADOW  x 530..700    ARCANA  x 845..1015
        //
        // The gaps BETWEEN bands (and between the CORE row and the branch rows) are not
        // cosmetic: a zone frame is its nodes' bounding box grown by a padding, so two
        // bands whose frames would collide have to be pulled apart in the data. The
        // renderers pad in design units scaled by the same fit factor as the positions,
        // so these clearances hold at every window size:
        //   horizontal, band to band : > 130  (have 140 / 145)
        //   vertical, row to row      : > 125  (have 140 throughout)
        // Shrink either below that and the frames start overlapping again.
        //
        // What sets those numbers is the NAME, not the tile. Every node carries an
        // always-on name below it, and the name is wider than the tile it belongs to —
        // so a node's real footprint is the tile UNION its name box. Framing the tiles
        // alone left the outer names hanging over their zone border, put every link in
        // the corridor its own parent's name occupies, and left the CORE column with
        // 6px between "System Core" and the tile beneath it. The renderers now measure
        // from that union (see nodeBox / NodeBox), and tools/check.mjs asserts the
        // clearances above, which is why the rows are a uniform 140 apart: 104 was not
        // enough for a tile, a name and another tile.
        //
        // One more constraint the numbers encode: nothing may sit on a line between two
        // other nodes. System Analysis used to share the hub's column with the whole
        // Shadow band, so the hub's link to Swift Blood ran straight through it and the
        // tree appeared to claim a gate that does not exist. It now hangs below Dragon's
        // Voice instead — out of all three fan-out corridors. The renderers can bow a
        // link around an obstacle, but a line through a tile's centre cannot be bowed
        // into something that reads as a detour, so the data has to keep the lanes clear.
        constexpr Node kNodes[] = {
            // --- Hub (Zone::kCore) ---
            // The hub sits centred above all three bands; its two satellites flank it.
            { 1, Zone::kCore, "System Core", "The System takes root.\n+25 Health, Magicka and Stamina.",
              "spells_01_frame.png", 615.0f, 96.0f, 5, kN, { 0, 0 }, Effect::kAttributes,
              { { AV::kHealth, 25.0f }, { AV::kMagicka, 25.0f }, { AV::kStamina, 25.0f } },
              false, 0, 0.0f, 1.4f },
            { 2, Zone::kCore, "Dragon's Voice", "Your Thu'um recovers faster.\n-20% shout cooldown.",
              "spells_10_frame.png", 880.0f, 96.0f, 15, kN, { 1, 0 }, Effect::kShoutCooldown, {} },
            { 14, Zone::kCore, "Perk Synthesis",
              "Condense a System Point into raw potential.\n+5 perk points per purchase. REPEATABLE.",
              "spells_21_frame.png", 350.0f, 96.0f, 1, kN, { 1, 0 }, Effect::kPerkPoint, {} },
            // A pure capability gate — no stat bonus (empty bonus array), so nothing
            // needs to change in ApplyEffect/AccumulateBonuses. IsUnlocked(kAnalyzeNodeKey)
            // is read directly by src/Analyze.cpp to decide whether the hotkey does
            // anything (see the kAnalyzeNodeKey comment in the header for why the key is
            // a named constant rather than duplicated as a magic number in both files).
            { kAnalyzeNodeKey, Zone::kCore, "System Analysis",
              "Unlocks the System's analytical eye.\nPress the Analyze hotkey to appraise whatever you are looking at.",
              "spells_02_frame.png", 880.0f, 236.0f, 10, kN, { 1, 0 }, Effect::kAttributes, {} },

            // --- Might (left band) ---
            { 3, Zone::kMight, "Vital Surge", "+100 Health.",
              "spells_25_frame.png", 300.0f, 400.0f, 10, kN, { 1, 0 }, Effect::kAttributes,
              { { AV::kHealth, 100.0f } } },
            { 4, Zone::kMight, "Thu'um Omniscience",
              "The System pours every dragon's voice into you.\nAll shouts and words of power unlocked.",
              "spells_39_frame.png", 210.0f, 540.0f, 25, kH, { 3, 0 }, Effect::kAllShouts, {},
              false, 0, 0.0f, 1.2f },
            { 5, Zone::kMight, "Emberguard", "+25% Fire Resist.",
              "spells_12_frame.png", 390.0f, 540.0f, 15, kN, { 3, 0 }, Effect::kAttributes,
              { { AV::kResistFire, 25.0f } } },

            // --- Arcana (right band) ---
            { 6, Zone::kArcana, "Mana Well", "+100 Magicka.",
              "spells_15_frame.png", 930.0f, 400.0f, 10, kN, { 1, 0 }, Effect::kAttributes,
              { { AV::kMagicka, 100.0f } } },
            { 7, Zone::kArcana, "Arcane Omniscience",
              "Every enchantment laid bare.\nAll enchantments known without disenchanting.",
              "spells_36_frame.png", 845.0f, 540.0f, 25, kH, { 6, 0 }, Effect::kAllEnchantments, {},
              false, 0, 0.0f, 1.2f },
            { 8, Zone::kArcana, "Frostguard", "+25% Frost Resist.",
              "spells_16_frame.png", 1015.0f, 540.0f, 15, kN, { 6, 0 }, Effect::kAttributes,
              { { AV::kResistFrost, 25.0f } } },
            { 9, Zone::kArcana, "Spell Omniscience",
              "The System reads every tome ever written.\nAll spells with a spell tome learned.",
              "spells_37_frame.png", 845.0f, 680.0f, 40, kH, { 7, 0 }, Effect::kAllSpells, {},
              false, 0, 0.0f, 1.2f },

            // --- Shadow (centre band) ---
            { 10, Zone::kShadow, "Swift Blood", "+100 Stamina.",
              "spells_32_frame.png", 615.0f, 400.0f, 10, kN, { 1, 0 }, Effect::kAttributes,
              { { AV::kStamina, 100.0f } } },
            { 11, Zone::kShadow, "Alchemical Insight",
              "Every ingredient gives up its secrets.\nAll ingredient effects known.",
              "spells_31_frame.png", 530.0f, 540.0f, 25, kH, { 10, 0 }, Effect::kAllIngredients, {},
              false, 0, 0.0f, 1.2f },
            { 12, Zone::kShadow, "Plagueward", "+25% Disease Resist.",
              "spells_34_frame.png", 700.0f, 540.0f, 15, kN, { 10, 0 }, Effect::kAttributes,
              { { AV::kResistDisease, 25.0f } } },

            // --- Capstone (bottom of the Shadow band) ---
            // Rooted in the Shadow children directly above it, not in the far-away
            // branch entries: their long diagonals crossed the whole middle field and
            // grazed every node on the way. Short V-lines, zero crossings — and a
            // deeper gate for the capstone as a side effect. ASCENDED-only.
            { 13, Zone::kShadow, "World Tree", "The System blossoms through your soul.\n+100 Health, Magicka and Stamina.",
              "spells_09_frame.png", 615.0f, 690.0f, 50, kA, { 11, 12 }, Effect::kAttributes,
              { { AV::kHealth, 100.0f }, { AV::kMagicka, 100.0f }, { AV::kStamina, 100.0f } },
              false, 0, 0.0f, 1.45f },

            // --- Utility (left margin, REPEATABLE MASTERY nodes) ---
            // Incremental stats that are otherwise fiddly to raise, and give NORMAL a
            // reason to keep spending. No prerequisites — always open, tier NORMAL, so
            // every rebirth can grind them. Deliberately NOT attack speed (a well-known
            // source of animation/mod conflicts).
            //
            // Every node below is capped at maxRank=10 — a MASTERY node (see IsMasteryNode):
            // 5 named tiers of 2 ranks each (Novice..Grandmaster), escalating cost per
            // tier (see NextRankCost). Earlier builds left these uncapped ("the SP economy
            // is the brake, not a rank limit") — feedback was that buying the same node
            // forever felt like a shop, not a skill tree; a real ceiling with a named,
            // celebrated finish line (the Grandmaster flourish in TryUnlock) reads as
            // actual progression instead. Fleet of Foot already had this cap from the
            // start — the rest now match it, magnitudes untouched.
            { 16, Zone::kMastery, "Beast of Burden", "The System shoulders your load.\n+25 Carry Weight per rank.",
              "spells_22_frame.png", 95.0f, 150.0f, 2, kN, { 0, 0 }, Effect::kAttributes,
              { { AV::kCarryWeight, 25.0f } }, true, 10 },
            { 15, Zone::kMastery, "Fleet of Foot", "The System quickens your stride.\n+3% movement speed per rank.",
              "spells_28_frame.png", 95.0f, 221.0f, 3, kN, { 0, 0 }, Effect::kDirectStat,
              { { AV::kSpeedMult, 3.0f } }, true, 10, 100.0f },
            { 17, Zone::kMastery, "Enduring Vigor", "The System deepens your reserves.\n+25 Health, Magicka and Stamina per rank.",
              "spells_06_frame.png", 95.0f, 292.0f, 4, kN, { 0, 0 }, Effect::kAttributes,
              { { AV::kHealth, 25.0f }, { AV::kMagicka, 25.0f }, { AV::kStamina, 25.0f } }, true, 10 },

            // --- Utility, continued: Tier-1 resistance/regen batch (user feedback —
            // "more skills to spend points on"; noted fire/frost resist exist but shock
            // doesn't). All kDirectStat: none of these actor values have an ESP ability
            // spell to fortify (unlike Health/Magicka/Fire/Frost/Disease resist above),
            // so they are set directly, exactly like Fleet of Foot's move speed.
            { 18, Zone::kMastery, "Storm Ward", "The System turns aside the lightning.\n+5% Shock Resist per rank.",
              "spells_18_frame.png", 95.0f, 363.0f, 3, kN, { 0, 0 }, Effect::kDirectStat,
              { { AV::kResistShock, 5.0f } }, true, 10, 0.0f },
            { 19, Zone::kMastery, "Warded Mind", "The System shields your soul from magic.\n+5% Magic Resist per rank.",
              "spells_19_frame.png", 95.0f, 434.0f, 4, kN, { 0, 0 }, Effect::kDirectStat,
              { { AV::kResistMagic, 5.0f } }, true, 10, 0.0f },
            { 20, Zone::kMastery, "Arcane Absorption", "The System drinks the spells cast against you.\n+4% Spell Absorption per rank.",
              "spells_20_frame.png", 95.0f, 505.0f, 5, kN, { 0, 0 }, Effect::kDirectStat,
              { { AV::kAbsorbChance, 4.0f } }, true, 10, 0.0f },
            { 21, Zone::kMastery, "Iron Skin", "The System hardens your hide.\n+10 Armor Rating per rank.",
              "spells_23_frame.png", 95.0f, 576.0f, 4, kN, { 0, 0 }, Effect::kDirectStat,
              { { AV::kDamageResist, 10.0f } }, true, 10, 0.0f },
            { 22, Zone::kMastery, "Rapid Recovery", "The System accelerates your body's grace.\n+10% Health, Magicka and Stamina regeneration per rank.",
              "spells_24_frame.png", 95.0f, 647.0f, 5, kN, { 0, 0 }, Effect::kDirectStat,
              { { AV::kHealRateMult, 10.0f }, { AV::kMagickaRateMult, 10.0f }, { AV::kStaminaRateMult, 10.0f } },
              true, 10, 100.0f },
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

        // Purchase count of a repeatable node (State::nodeRanks). Caller holds g_mutex.
        [[nodiscard]] std::int32_t RankNoLock(std::uint32_t a_key) {
            for (const auto& [key, rank] : GetState().nodeRanks) {
                if (key == a_key) {
                    return rank;
                }
            }
            return 0;
        }

        void AddRankNoLock(std::uint32_t a_key) {
            for (auto& [key, rank] : GetState().nodeRanks) {
                if (key == a_key) {
                    ++rank;
                    return;
                }
            }
            GetState().nodeRanks.emplace_back(a_key, 1);
        }

        void ClearRankNoLock(std::uint32_t a_key) {
            auto& ranks = GetState().nodeRanks;
            ranks.erase(std::remove_if(ranks.begin(), ranks.end(),
                                       [a_key](const auto& e) { return e.first == a_key; }),
                        ranks.end());
        }

        // A MASTERY node is a capped repeatable — Beast of Burden, Storm Ward and the
        // rest of the Tier-1 batch. Perk Synthesis is the only other repeatable
        // (maxRank 0, uncapped): it converts SP into perk points rather than growing a
        // stat, so it stays a flat, uncapped exchange rather than a tiered mastery.
        [[nodiscard]] bool IsMasteryNode(const Node& a_node) {
            return a_node.repeatable && a_node.maxRank > 0;
        }

        constexpr std::int32_t kTierCount = 5;      // Novice..Grandmaster
        constexpr std::int32_t kRanksPerTier = 2;    // -> maxRank 10 across every mastery node
        constexpr const char*  kTierNames[kTierCount] = { "Novice", "Adept", "Expert", "Master",
                                                           "Grandmaster" };

        // 1-based tier a given rank (1..maxRank) falls in; 0 for rank <= 0.
        [[nodiscard]] std::int32_t TierOfRank(std::int32_t a_rank) {
            if (a_rank <= 0) {
                return 0;
            }
            return std::min(kTierCount, (a_rank - 1) / kRanksPerTier + 1);
        }

        // What buying rank a_rank of a mastery node costs: base cost x that rank's
        // tier, so Novice ranks are cheap and Grandmaster ranks cost 5x. Flat cost for
        // everything else.
        [[nodiscard]] std::int32_t RankCost(const Node& a_node, std::int32_t a_rank) {
            return IsMasteryNode(a_node) ? a_node.cost * TierOfRank(a_rank) : a_node.cost;
        }

        // Total SP sunk into a mastery node at a_rank — the sum of every tiered
        // purchase so far, not a flat cost*rank guess. TotalInvested/RespecRefund need
        // this so a respec gives back exactly what was paid.
        [[nodiscard]] std::int32_t MasteryCostToRank(const Node& a_node, std::int32_t a_rank) {
            std::int32_t total = 0;
            for (std::int32_t i = 1; i <= a_rank; ++i) {
                total += RankCost(a_node, i);
            }
            return total;
        }

        // What a repeatable node's purchase so far (any kind) actually cost in total —
        // the one place TotalInvested/RespecRefund/Respec go for the "cost * rank"
        // figure, so mastery's tiered pricing and Perk Synthesis's flat pricing don't
        // need separate call sites.
        [[nodiscard]] std::int32_t RepeatableCostToRank(const Node& a_node, std::int32_t a_rank) {
            return IsMasteryNode(a_node) ? MasteryCostToRank(a_node, a_rank) : a_node.cost * a_rank;
        }

        // Only nodes whose effect can be fully undone may be refunded — see the note on
        // RespecRefund in the header for why knowledge and Perk Synthesis are excluded.
        [[nodiscard]] bool IsRefundable(Effect a_effect) {
            switch (a_effect) {
            case Effect::kAttributes:   // flows through Passives::Refresh
            case Effect::kShoutCooldown:  // a single actor value we can set back
            case Effect::kDirectStat:   // recomputed from the rank, so rank 0 = baseline
                return true;
            default:
                return false;
            }
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
            // What counts as a real player dragon shout, learned from the ground truth of
            // a full modlist's shout dump (see git history — the diagnostic that found it):
            //
            //  * Has a description. The description-less forms are dragon-AI / NPC copies
            //    and the "Fire Breath (for DRAGONS only)" oddities.
            //  * Exactly three words. Every player dragon shout (vanilla + DLC) carries all
            //    three. The one-word "shouts" are racial and beast GREATER POWERS reusing
            //    the shout mechanic — Battle Cry, Voice of the Emperor, Beast Tongue, the
            //    werewolf howls, Benthic Scream — not Thu'um the player learns at walls.
            //
            // Tiebreak among same-named forms: lowest FormID (the original; an override
            // reuses the FormID, so a second same-named record is a distinct variant).
            std::map<std::string, RE::TESShout*> byName;
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
                if (description.empty()) {
                    continue;
                }

                int wordCount = 0;
                for (const auto& v : shout->variations) {
                    if (v.word) {
                        ++wordCount;
                    }
                }
                if (wordCount < 3) {
                    continue;
                }

                auto [it, fresh] = byName.try_emplace(name, shout);
                if (!fresh && shout->GetFormID() < it->second->GetFormID()) {
                    it->second = shout;
                }
            }

            // Drop the dragon-AI duplicates: "Dragon Unrelenting Force" when a bare
            // "Unrelenting Force" is also present (same shout, the beast's copy). The real
            // DLC shout "Dragon Aspect" survives because there is no bare "Aspect".
            {
                const std::string prefix = "Dragon ";
                for (auto it = byName.begin(); it != byName.end();) {
                    const std::string& n = it->first;
                    if (n.size() > prefix.size() && n.compare(0, prefix.size(), prefix) == 0 &&
                        byName.find(n.substr(prefix.size())) != byName.end()) {
                        it = byName.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            std::size_t knownShouts = 0;
            std::size_t words = 0;
            for (const auto& [name, shout] : byName) {
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
            // being pointed at.
            //
            // Dedup by display name, same lesson as the shouts: several distinct base
            // forms can share a name ("Fortify Destruction" at different magnitudes),
            // and marking them all known lists the effect several times at the table.
            // Keep one per name (lowest FormID, the original) known and clear the flag
            // on the same-named siblings. Harmless — the applied magnitude scales with
            // the player's skill, not the base form, so one per name is all you need.
            std::map<std::string, RE::EnchantmentItem*> byName;
            for (auto* ench : all) {
                if (!ench || !ench->data.baseEnchantment) {
                    continue;
                }
                auto* base = ench->data.baseEnchantment;
                const char* name = base->GetName();
                if (!name || !*name) {
                    continue;
                }
                auto [it, fresh] = byName.try_emplace(name, base);
                if (!fresh && base->GetFormID() < it->second->GetFormID()) {
                    it->second = base;
                }
            }
            using EnchFlag = RE::TESForm::RecordFlags;
            for (auto* ench : all) {
                if (!ench || !ench->data.baseEnchantment) {
                    continue;
                }
                auto* base = ench->data.baseEnchantment;
                const char* name = base->GetName();
                if (!name || !*name) {
                    continue;
                }
                if (byName[name] == base) {
                    base->formFlags |= EnchFlag::kKnown;
                } else {
                    base->formFlags &= ~static_cast<std::uint32_t>(EnchFlag::kKnown);
                }
            }
            logger::info("SkillTree: {} unique enchantments marked known (deduped by name)",
                         byName.size());
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
            case Effect::kDirectStat: {
                // No fortify ability exists for these actor values, so set each one's
                // base directly to baseline + step*rank. Absolute and recomputed from
                // the rank, so re-applying on load or after another purchase is
                // idempotent — never stacks or drifts. Loops every non-empty bonus slot,
                // so one node (Rapid Recovery) can drive several actor values at once.
                if (auto* avOwner = player->AsActorValueOwner()) {
                    const float rank = static_cast<float>(Rank(a_node.key));
                    for (const auto& b : a_node.bonus) {
                        if (b.av == AV::kNone) {
                            continue;
                        }
                        avOwner->SetBaseActorValue(b.av, a_node.baseline + b.amount * rank);
                    }
                }
                break;
            }
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
        // Gated by treeTier, the dedicated "how deep the tree opens" axis — the same as
        // the reward tier (power) for every preset, but separable on a CUSTOM build (a
        // deep tree on a modest reward pace). PowerLevel is ordered Normal < Hero <
        // Ascended, so a numeric compare is the whole gate.
        return static_cast<int>(GetState().treeTier) >= static_cast<int>(node->minPower);
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
        if (!node) {
            return false;
        }
        // A one-shot node locks out once owned; a repeatable one never does.
        if (!node->repeatable && IsUnlocked(a_key)) {
            return false;
        }
        if (!PrereqsMet(a_key) || !TierMet(a_key)) {
            return false;
        }
        // Repeatable nodes may carry a rank cap.
        if (node->repeatable && node->maxRank > 0 && Rank(a_key) >= node->maxRank) {
            return false;
        }

        auto&              state = GetState();
        const std::int32_t rankBefore = node->repeatable ? Rank(a_key) : 0;
        const std::int32_t purchaseCost = RankCost(*node, rankBefore + 1);
        if (state.systemPoints < purchaseCost) {
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
            state.systemPoints -= purchaseCost;
            GrantPerkPoints(grant);
            Sounds::Play(Sounds::Sfx::ButtonClick);
            logger::info("SkillTree: synthesised {} perk point(s) for {} SP", grant, purchaseCost);
            return true;
        }

        state.systemPoints -= purchaseCost;

        std::int32_t rankAfter = 0;
        {
            std::scoped_lock lock(g_mutex);
            if (node->repeatable) {
                AddRankNoLock(a_key);  // stays buyable; the rank drives the bonus
                rankAfter = RankNoLock(a_key);
            } else {
                state.unlockedNodes.push_back(a_key);
            }
        }

        ApplyEffect(*node);  // move speed reads the fresh rank; attribute nodes are no-ops
        Passives::Refresh();
        Sounds::Play(Sounds::Sfx::LevelUp);

        // Reaching Grandmaster on a mastery node is a real finish line — worth the same
        // flourish (rings, title punch, sound) a milestone gets, not just a badge tick.
        if (IsMasteryNode(*node) && rankAfter == node->maxRank) {
            UI::PlayLevelUpEffect("GRANDMASTER", node->name);
        }

        logger::info("SkillTree: unlocked '{}'{} for {} point(s)", node->name,
                     node->repeatable ? (" -> rank " + std::to_string(rankAfter)) : std::string{},
                     purchaseCost);
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
        // Repeatable nodes live in nodeRanks, not unlockedNodes. Re-assert theirs too:
        // move speed is a direct actor-value set a fresh load would otherwise lose (the
        // attribute repeatables ride Passives::Refresh via AccumulateBonuses, so their
        // ApplyEffect is a harmless no-op).
        for (const auto& node : kNodes) {
            if (node.repeatable && Rank(node.key) > 0) {
                ApplyEffect(node);
            }
        }
        if (!unlocked.empty()) {
            logger::info("SkillTree: re-applied {} node(s) from the save", unlocked.size());
        }
    }

    void AccumulateBonuses(std::map<RE::ActorValue, float>& a_totals) {
        std::scoped_lock lock(g_mutex);
        for (const auto& node : kNodes) {
            if (node.effect != Effect::kAttributes) {
                continue;
            }
            // Repeatable nodes contribute once per rank; one-shot nodes once if owned.
            const std::int32_t times =
                node.repeatable ? RankNoLock(node.key) : (IsUnlockedNoLock(node.key) ? 1 : 0);
            if (times <= 0) {
                continue;
            }
            for (const auto& bonus : node.bonus) {
                if (bonus.av != AV::kNone) {
                    a_totals[bonus.av] += bonus.amount * static_cast<float>(times);
                }
            }
        }
    }

    std::int32_t Rank(std::uint32_t a_key) {
        std::scoped_lock lock(g_mutex);
        return RankNoLock(a_key);
    }

    std::int32_t NextCost(std::uint32_t a_key) {
        const auto* node = Find(a_key);
        if (!node) {
            return 0;
        }
        return RankCost(*node, (node->repeatable ? Rank(a_key) : 0) + 1);
    }

    std::int32_t Tier(std::uint32_t a_key) {
        const auto* node = Find(a_key);
        if (!node || !IsMasteryNode(*node)) {
            return 0;
        }
        return TierOfRank(Rank(a_key));
    }

    const char* TierName(std::int32_t a_tier) {
        return (a_tier >= 1 && a_tier <= kTierCount) ? kTierNames[a_tier - 1] : "";
    }

    const char* ZoneName(Zone a_zone) {
        switch (a_zone) {
        case Zone::kCore:    return "CORE";
        case Zone::kMight:   return "MIGHT";
        case Zone::kArcana:  return "ARCANA";
        case Zone::kShadow:  return "SHADOW";
        case Zone::kMastery: return "MASTERY";
        default:             return "";
        }
    }

    std::int32_t TotalInvested() {
        std::scoped_lock lock(g_mutex);
        std::int32_t total = 0;
        for (const auto& node : kNodes) {
            total += node.repeatable ? RepeatableCostToRank(node, RankNoLock(node.key))
                                     : (IsUnlockedNoLock(node.key) ? node.cost : 0);
        }
        return total;
    }

    std::int32_t RespecRefund() {
        std::scoped_lock lock(g_mutex);
        std::int32_t total = 0;
        for (const auto& node : kNodes) {
            if (!IsRefundable(node.effect)) {
                continue;
            }
            total += node.repeatable ? RepeatableCostToRank(node, RankNoLock(node.key))
                                     : (IsUnlockedNoLock(node.key) ? node.cost : 0);
        }
        return total;
    }

    bool Respec() {
        std::int32_t              refund = 0;
        std::vector<const Node*>  cleared;
        {
            std::scoped_lock lock(g_mutex);
            auto&            unlocked = GetState().unlockedNodes;
            for (const auto& node : kNodes) {
                if (!IsRefundable(node.effect)) {
                    continue;
                }
                if (node.repeatable) {
                    const std::int32_t rank = RankNoLock(node.key);
                    if (rank <= 0) {
                        continue;
                    }
                    refund += RepeatableCostToRank(node, rank);
                    ClearRankNoLock(node.key);
                } else {
                    if (!IsUnlockedNoLock(node.key)) {
                        continue;
                    }
                    refund += node.cost;
                    unlocked.erase(std::remove(unlocked.begin(), unlocked.end(), node.key),
                                   unlocked.end());
                }
                cleared.push_back(&node);
            }
        }
        if (refund <= 0) {
            return false;
        }

        GetState().systemPoints += refund;

        // Undo the direct actor-value writes. Attribute nodes need nothing here — they
        // are re-derived from the (now shorter) unlocked list by Passives::Refresh.
        // ApplyEffect must run outside the lock: it reads Rank(), which takes g_mutex.
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            if (auto* avOwner = player->AsActorValueOwner()) {
                for (const auto* node : cleared) {
                    if (node->effect == Effect::kShoutCooldown) {
                        avOwner->SetBaseActorValue(AV::kShoutRecoveryMult, 1.0f);
                    } else if (node->effect == Effect::kDirectStat) {
                        ApplyEffect(*node);  // rank is 0 now → back to each AV's baseline
                    }
                }
            }
        }
        Passives::Refresh();
        Sounds::Play(Sounds::Sfx::LevelUp);

        logger::info("SkillTree: respec refunded {} point(s) across {} node(s)", refund,
                     cleared.size());
        return true;
    }
}
