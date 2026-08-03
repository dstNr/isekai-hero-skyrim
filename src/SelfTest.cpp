#include "SelfTest.h"

#include "Config.h"
#include "CraftHooks.h"
#include "Passives.h"
#include "Plugin.h"
#include "Quests.h"
#include "Shop.h"
#include "SkillTree.h"
#include "Storage.h"
#include "System.h"
#include "UI/Input.h"
#include "UI/Prisma.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace Isekai::SelfTest {

    namespace {
        // One line of the report. `fatal` marks a check whose failure means a feature is
        // silently dead rather than merely unavailable — those are the ones worth chasing.
        struct Result {
            bool        pass;
            bool        fatal;
            std::string name;
            std::string detail;
        };

        void Add(std::vector<Result>& a_out, bool a_pass, bool a_fatal, std::string a_name,
                 std::string a_detail) {
            a_out.push_back({ a_pass, a_fatal, std::move(a_name), std::move(a_detail) });
        }

        // Does a keyword with this editor ID exist anywhere in the load order?
        // BGSKeyword keeps its editor ID as a real member (unlike most form types), so
        // this is a genuine lookup rather than a guess.
        [[nodiscard]] bool KeywordExists(const char* a_editorID) {
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data || !a_editorID) {
                return false;
            }
            for (auto* kw : data->GetFormArray<RE::BGSKeyword>()) {
                if (kw) {
                    const char* id = kw->GetFormEditorID();
                    if (id && std::string_view{ id } == a_editorID) {
                        return true;
                    }
                }
            }
            return false;
        }

        // --- the checks -------------------------------------------------------

        void CheckPlugin(std::vector<Result>& out) {
            const bool loaded = Plugin::IsLoaded();
            Add(out, loaded, true, "ESP loaded",
                loaded ? std::string(Plugin::kFileName)
                       : std::string(Plugin::kFileName) + " is NOT in the load order");
            if (!loaded) {
                return;
            }
            // The two forms the code hard-depends on. Their local IDs live in Storage.cpp;
            // resolving them here proves the ESP is the one the code was built against.
            auto* chest = Plugin::LookupOurForm<RE::TESObjectCONT>(0x000D7A);
            Add(out, chest != nullptr, true, "storage container form",
                chest ? "resolved" : "0x000D7A missing — Dimensional Storage cannot work");
            auto* codex = Plugin::LookupOurForm<RE::TESBoundObject>(0x000D7F);
            Add(out, codex != nullptr, false, "storage codex form",
                codex ? "resolved" : "0x000D7F missing — the physical shortcut is gone");
        }

        void CheckQuests(std::vector<Result>& out) {
            const auto quarries = Quests::QuarryKeywords();
            std::vector<std::string> bad;
            for (const auto& [name, keyword] : quarries) {
                if (!KeywordExists(keyword)) {
                    bad.emplace_back(std::string(keyword) + " (" + name + ")");
                }
            }
            std::string detail =
                std::to_string(quarries.size() - bad.size()) + "/" +
                std::to_string(quarries.size()) + " keywords exist";
            if (!bad.empty()) {
                detail += " — MISSING: ";
                for (std::size_t i = 0; i < bad.size(); ++i) {
                    detail += (i ? ", " : "") + bad[i];
                }
                detail += " (those objectives can never be completed)";
            }
            Add(out, bad.empty(), true, "quest target keywords", std::move(detail));

            if (Quests::Active()) {
                Add(out, true, false, "active objective",
                    Quests::Text() + "  " + std::to_string(Quests::Progress()) + "/" +
                        std::to_string(Quests::Target()) + "  (+" +
                        std::to_string(Quests::Reward()) + " SP)");
            } else {
                // Not a failure: an un-reincarnated character is given no work.
                Add(out, true, false, "active objective",
                    GetState().reincarnated ? "none — EnsureObjective did not run?"
                                            : "none (character not reincarnated yet)");
            }
        }

        void CheckShop(std::vector<Result>& out) {
            const auto catalog = Shop::Catalog();
            Add(out, !catalog.empty(), true, "shop catalog",
                std::to_string(catalog.size()) +
                    " entries offered (potions appear here once their ESP records exist)");

            // Every advertised icon must be on disk, or the card renders blank. The static
            // check covers this too, but only for the entries it can see — this one covers
            // whatever the running catalog actually decided to offer.
            std::vector<std::string> missing;
            for (const auto& item : catalog) {
                const std::string path =
                    "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\" + item.icon;
                if (!std::filesystem::exists(path)) {
                    missing.push_back(item.icon);
                }
            }
            std::string detail = std::to_string(catalog.size() - missing.size()) + "/" +
                                 std::to_string(catalog.size()) + " icons on disk";
            if (!missing.empty()) {
                detail += " — MISSING: ";
                for (std::size_t i = 0; i < missing.size(); ++i) {
                    detail += (i ? ", " : "") + missing[i];
                }
            }
            Add(out, missing.empty(), false, "shop card icons", std::move(detail));
        }

        // The skill tree's runtime contracts. Its DATA (coordinates, keys, geometry) is
        // checked statically by tools/check.mjs; what only a running game can answer is
        // whether the things the table names actually exist and actually work.
        void CheckSkillTree(std::vector<Result>& out) {
            std::size_t count = 0;
            const auto* nodes = SkillTree::Nodes(count);
            Add(out, count > 0, true, "skill tree loaded",
                std::to_string(count) + " nodes");
            if (count == 0) {
                return;
            }

            // THE important one. Effect::kAttributes routes its bonus through the ESP's
            // ability spells (Passives), and only the actor values those spells cover can
            // be granted. A node naming any other actor value is a no-op that reports
            // nothing — you buy it, you pay for it, and nothing happens. This has bitten
            // once already, during the Tier-1 resistance batch.
            const auto covered = Passives::CoveredActorValues();
            std::vector<std::string> dead;
            for (std::size_t i = 0; i < count; ++i) {
                const auto& n = nodes[i];
                if (n.effect != SkillTree::Effect::kAttributes) {
                    continue;  // kDirectStat writes the actor value itself, no ability needed
                }
                for (const auto& b : n.bonus) {
                    if (b.av == RE::ActorValue::kNone) {
                        continue;
                    }
                    if (std::find(covered.begin(), covered.end(), b.av) == covered.end()) {
                        dead.emplace_back(std::string(n.name) + " (AV " +
                                          std::to_string(static_cast<int>(b.av)) + ")");
                    }
                }
            }
            std::string detail = std::to_string(covered.size()) + " actor values have an ability";
            if (!dead.empty()) {
                detail += " — these nodes grant NOTHING: ";
                for (std::size_t i = 0; i < dead.size(); ++i) {
                    detail += (i ? ", " : "") + dead[i];
                }
            }
            Add(out, dead.empty(), true, "attribute nodes are backed", std::move(detail));

            // Icons. A missing file is a blank tile, which reads as a broken tree rather
            // than a missing asset. Only meaningful where the built-in UI actually draws.
            if (!REL::Module::IsVR()) {
                std::vector<std::string> missingIcons;
                for (std::size_t i = 0; i < count; ++i) {
                    const std::string path =
                        "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\" + std::string(nodes[i].icon);
                    if (!std::filesystem::exists(path)) {
                        missingIcons.emplace_back(nodes[i].icon);
                    }
                }
                std::string iconDetail = std::to_string(count - missingIcons.size()) + "/" +
                                         std::to_string(count) + " node icons on disk";
                if (!missingIcons.empty()) {
                    iconDetail += " — MISSING: ";
                    for (std::size_t i = 0; i < missingIcons.size(); ++i) {
                        iconDetail += (i ? ", " : "") + missingIcons[i];
                    }
                }
                Add(out, missingIcons.empty(), false, "skill tree icons", std::move(iconDetail));
            }

            // Anything the save remembers must still be a real node. A key that no longer
            // exists means points were spent on something this build cannot honour.
            const auto& st = GetState();
            const auto known = [&](std::uint32_t key) {
                for (std::size_t i = 0; i < count; ++i) {
                    if (nodes[i].key == key) {
                        return true;
                    }
                }
                return false;
            };
            std::vector<std::string> orphans;
            for (const auto key : st.unlockedNodes) {
                if (!known(key)) {
                    orphans.emplace_back("unlocked #" + std::to_string(key));
                }
            }
            for (const auto& [key, rank] : st.nodeRanks) {
                if (!known(key)) {
                    orphans.emplace_back("rank #" + std::to_string(key));
                }
            }
            std::string saveDetail = std::to_string(st.unlockedNodes.size()) + " unlocked, " +
                                     std::to_string(st.nodeRanks.size()) + " ranked";
            if (!orphans.empty()) {
                saveDetail += " — save refers to nodes this build does not have: ";
                for (std::size_t i = 0; i < orphans.size(); ++i) {
                    saveDetail += (i ? ", " : "") + orphans[i];
                }
            }
            Add(out, orphans.empty(), true, "saved nodes still exist", std::move(saveDetail));

            // The Analyze hotkey is gated on one specific node key; if that constant and
            // the table ever part ways, the hotkey can never be unlocked.
            Add(out, known(SkillTree::kAnalyzeNodeKey), true, "analyze node key",
                known(SkillTree::kAnalyzeNodeKey)
                    ? "#" + std::to_string(SkillTree::kAnalyzeNodeKey) + " present"
                    : "kAnalyzeNodeKey names no node — the Analyze hotkey is unreachable");
        }

        void CheckCrafting(std::vector<Result>& out) {
            const bool item = CraftHooks::ItemCraftingHooksActive();
            const bool iter = CraftHooks::IterationHooksActive();
            Add(out, item, false, "crafting count/consume hooks",
                item ? "installed" : "NOT installed — storage materials will not be usable "
                                     "at a workbench in place");
            Add(out, iter, false, "crafting iteration hooks",
                iter ? "installed" : "NOT installed — alchemy/enchanting menus will not list "
                                     "stored materials");
        }

        void CheckEnvironment(std::vector<Result>& out) {
            const bool vr = REL::Module::IsVR();
            const bool prisma = UI::Prisma::Active();
            Add(out, true, false, "runtime", vr ? "Skyrim VR" : "Skyrim SE/AE");
            // In VR a missing PrismaUI means there is no UI at all — that IS a failure.
            Add(out, !vr || prisma, true, "PrismaUI view",
                prisma ? "active"
                       : (vr ? "inactive — in VR this leaves NO usable UI (needs the "
                               "PrismaUI patch + PrismaUI 1.5.0 VR)"
                             : "inactive (built-in ImGui UI is in use — normal)"));

            const auto& st = GetState();
            Add(out, true, false, "character",
                st.reincarnated
                    ? "reincarnated, " + PowerName(st.power) + ", " +
                          std::to_string(st.systemPoints) + " SP"
                    : "not reincarnated yet");
            Add(out, true, false, "dimensional storage",
                Storage::Available() ? "available" : "unavailable (not reincarnated)");
        }
    }

    void Run() {
        std::vector<Result> out;
        CheckPlugin(out);
        CheckEnvironment(out);
        CheckSkillTree(out);
        CheckQuests(out);
        CheckShop(out);
        CheckCrafting(out);

        std::size_t failed = 0;
        std::size_t fatal = 0;
        for (const auto& r : out) {
            if (!r.pass) {
                ++failed;
                if (r.fatal) {
                    ++fatal;
                }
            }
        }

        logger::info("================ ISEKAI HERO SELF-TEST ================");
        for (const auto& r : out) {
            logger::info("  [{}] {:<28} {}", r.pass ? "PASS" : (r.fatal ? "FAIL" : "warn"),
                         r.name, r.detail);
        }
        logger::info("  {} of {} checks passed{}", out.size() - failed, out.size(),
                     fatal > 0 ? " — SOMETHING IS BROKEN, see FAIL above" : "");
        logger::info("=======================================================");

        // The in-game half of the report: enough to know whether to go read the log.
        const std::string msg =
            failed == 0
                ? "[ SYSTEM ] Self-test: all " + std::to_string(out.size()) + " checks passed."
                : "[ SYSTEM ] Self-test: " + std::to_string(failed) + " of " +
                      std::to_string(out.size()) + " failed — see IsekaiHeroSKSE.log.";
        RE::DebugNotification(msg.c_str());
    }

    void Install() {
        UI::RegisterHotkey(Config::SelfTestKey(), Run);
        logger::info("SelfTest: hotkey {:#04x} runs the self-test", Config::SelfTestKey());
    }
}
