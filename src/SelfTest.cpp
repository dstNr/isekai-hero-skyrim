#include "SelfTest.h"

#include "Config.h"
#include "CraftHooks.h"
#include "Passives.h"
#include "Plugin.h"
#include "Progression.h"
#include "Quests.h"
#include "Sounds.h"
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

        // Is IsekaiHero.esp the file this build expects? Every feature is anchored to a
        // form in it, and a mismatched or partially-edited ESP breaks them one at a time
        // rather than loudly.
        void CheckPlugin(std::vector<Result>& out) {
            const bool loaded = Plugin::IsLoaded();
            Add(out, loaded, true, "ESP loaded",
                loaded ? std::string(Plugin::kFileName)
                       : std::string(Plugin::kFileName) + " is NOT in the load order");
            if (!loaded) {
                return;  // everything below would just repeat the same failure
            }

            // The 8 ability spells. These carry every milestone passive AND every
            // Effect::kAttributes skill-tree node; a missing one silently drops whichever
            // actor value it backed.
            const auto covered = Passives::CoveredActorValues();
            constexpr std::size_t kExpectedAbilities = 9;
            Add(out, covered.size() == kExpectedAbilities, true, "ESP ability spells",
                std::to_string(covered.size()) + "/" + std::to_string(kExpectedAbilities) +
                    " resolved" +
                    (covered.size() == kExpectedAbilities
                         ? ""
                         : " — the missing ones grant nothing; see the Passives: lines above"));

            const auto [sounds, soundsTotal] = Sounds::Resolved();
            Add(out, sounds == soundsTotal, false, "ESP sound descriptors",
                std::to_string(sounds) + "/" + std::to_string(soundsTotal) + " resolved" +
                    (sounds == soundsTotal ? "" : " — those effects will be silent"));

            auto* chest = Plugin::LookupOurForm<RE::TESObjectCONT>(0x000D7A);
            Add(out, chest != nullptr, true, "ESP storage container",
                chest ? "resolved" : "0x000D7A missing — Dimensional Storage cannot work");
            auto* codex = Plugin::LookupOurForm<RE::TESBoundObject>(0x000D7F);
            Add(out, codex != nullptr, false, "ESP storage codex",
                codex ? "resolved" : "0x000D7F missing — the physical shortcut is gone");
        }

        // The 79 milestones are the mod's biggest hand-typed surface: each names a quest
        // by editor ID, and one that does not match never pays out, silently.
        void CheckMilestones(std::vector<Result>& out) {
            const auto [missing, total] = Progression::UnresolvedMilestones();
            std::string detail = std::to_string(total - missing.size()) + "/" +
                                 std::to_string(total) + " milestone quests resolve";
            if (!missing.empty()) {
                detail += " — these can never fire: ";
                for (std::size_t i = 0; i < missing.size() && i < 6; ++i) {
                    detail += (i ? ", " : "") + missing[i];
                }
                if (missing.size() > 6) {
                    detail += ", +" + std::to_string(missing.size() - 6) + " more (see the log)";
                }
            }
            Add(out, missing.empty(), true, "milestone quests", std::move(detail));

            // Same contract the skill tree's attribute nodes live under: a passive whose
            // actor value has no ability spell behind it grants nothing and says nothing.
            // Reported by title AND actor value, because which value is uncovered depends
            // entirely on what the ESP's magic effects carry — that is not knowable from
            // the source, only from a running game.
            const auto covered = Passives::CoveredActorValues();
            std::vector<std::string> dead;
            for (const auto& [title, av] : Progression::PassiveActorValues()) {
                if (av == RE::ActorValue::kNone) {
                    continue;
                }
                if (std::find(covered.begin(), covered.end(), av) == covered.end()) {
                    const std::string entry =
                        std::string(title) + " (AV " + std::to_string(static_cast<int>(av)) + ")";
                    if (std::find(dead.begin(), dead.end(), entry) == dead.end()) {
                        dead.push_back(entry);
                    }
                }
            }
            std::string passDetail =
                std::to_string(covered.size()) + " actor values have an ability";
            if (!dead.empty()) {
                passDetail += " — these titles grant NOTHING: ";
                for (std::size_t i = 0; i < dead.size(); ++i) {
                    passDetail += (i ? ", " : "") + dead[i];
                }
                passDetail += " (the ESP needs an ability spell for that actor value)";
            }
            Add(out, dead.empty(), true, "milestone passives are backed",
                std::move(passDetail));
        }

        // Dimensional Storage. Its contents are no longer granted but bought, so the
        // thing that can silently break is the sweep behind each material pack: if a
        // filter stops matching, the pack still sells and still charges, and delivers
        // nothing.
        void CheckStorage(std::vector<Result>& out) {
            Add(out, true, false, "storage available",
                Storage::Available() ? "yes" : "no (character not reincarnated)");

            auto* ref = Storage::ChestRef();
            Add(out, true, false, "storage chest",
                ref ? "exists, " + std::to_string(ref->GetInventoryCounts().size()) +
                          " distinct item(s)"
                    : "not created yet (created on first use — normal)");

            struct Cat {
                Storage::MaterialCategory cat;
                const char*               name;
            };
            const Cat cats[] = {
                { Storage::MaterialCategory::kSmithing, "smithing" },
                { Storage::MaterialCategory::kAlchemy, "alchemy" },
                { Storage::MaterialCategory::kEnchanting, "soul gems" },
            };
            std::string detail;
            bool        allFound = true;
            for (const auto& c : cats) {
                const auto n = Storage::CountCategory(c.cat);
                if (n == 0) {
                    allFound = false;
                }
                detail += (detail.empty() ? "" : ", ") + std::string(c.name) + " " +
                          std::to_string(n);
            }
            Add(out, allFound, true, "shop material sweeps",
                detail + (allFound ? " materials found"
                                   : " — a category finds NOTHING; its pack would charge "
                                     "for an empty delivery"));

            // What our OWN goods (the ESP potions) resolve to and how many the chest
            // holds. "I bought a potion and it is not in the storage" cannot be answered
            // from the outside: a form that fails to resolve, a delivery that no-ops and
            // a chest the player is not looking at all look the same. This names each
            // potion, its runtime form and its count, so the log settles it.
            std::string held;
            std::size_t missing = 0;
            for (const auto& item : Shop::OurGoods()) {
                auto* obj = item.second;
                if (!obj) {
                    ++missing;
                    held += (held.empty() ? "" : ", ") + item.first + " UNRESOLVED";
                    continue;
                }
                held += (held.empty() ? "" : ", ") + item.first + " " +
                        std::to_string(Storage::ChestCount(obj));
            }
            if (held.empty()) {
                held = "no ESP goods in the catalog yet";
            }
            Add(out, missing == 0, false, "storage: our own goods", std::move(held));
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

            // How the rail will look. A shelf can be empty for a legitimate reason — the
            // potions only resolve once their ESP records exist — so this is a report,
            // not a failure. It is what tells a bug reporter whether a whole category
            // came up blank in game.
            std::string shelves;
            for (const auto& shelf : Shop::Shelves()) {
                const auto held = std::count_if(catalog.begin(), catalog.end(),
                                                [&](const auto& it) { return it.shelf == shelf; });
                shelves += (shelves.empty() ? "" : ", ") + shelf + " " + std::to_string(held);
            }
            Add(out, true, false, "shop shelves", std::move(shelves));
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

        // The interface chrome — status buttons, blessing choice, rank insignia. Unlike
        // the tree and shop icons these are named inline rather than in a table, and the
        // rank ones are derived from a letter, so nothing at runtime would otherwise
        // notice they never deployed. A missing one draws nothing at all, which looks
        // like a button that was never built rather than a file that never arrived.
        void CheckUiIcons(std::vector<Result>& out) {
            static constexpr const char* kChrome[] = {
                "ui_skilltree.png",  "ui_storage.png",        "ui_shop.png",
                "blessing_normal.png", "blessing_hero.png",   "blessing_ascended.png",
                "blessing_full.png", "blessing_shattered.png", "blessing_dormant.png",
                "blessing_custom.png",
                "rank_e.png",        "rank_d.png",            "rank_c.png",
                "rank_b.png",        "rank_a.png",            "rank_s.png",
            };

            std::vector<std::string> missing;
            for (const char* icon : kChrome) {
                const std::string path = "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\" + std::string(icon);
                if (!std::filesystem::exists(path)) {
                    missing.emplace_back(icon);
                }
            }
            std::string detail = std::to_string(std::size(kChrome) - missing.size()) + "/" +
                                 std::to_string(std::size(kChrome)) + " interface icons on disk";
            if (!missing.empty()) {
                detail += " — MISSING: ";
                for (std::size_t i = 0; i < missing.size(); ++i) {
                    detail += (i ? ", " : "") + missing[i];
                }
            }
            Add(out, missing.empty(), false, "interface icons", std::move(detail));
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

    void Run(bool a_notify) {
        std::vector<Result> out;
        CheckPlugin(out);
        CheckEnvironment(out);
        CheckMilestones(out);
        CheckSkillTree(out);
        CheckQuests(out);
        CheckStorage(out);
        CheckShop(out);
        CheckUiIcons(out);
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

        // The in-game half of the report.
        //
        // The hotkey run gets a MESSAGE BOX, not a notification. A corner notification
        // fades in a couple of seconds and is trivially missed, which is indistinguishable
        // from the key not working at all — and that is exactly what a diagnostic key must
        // never be ambiguous about. It also carries the failures themselves, so the
        // common cases need no log at all.
        //
        // The automatic run on load draws nothing at all — see the note below it.
        if (a_notify) {
            std::string box = "[ SYSTEM ] SELF-TEST\n\n" + std::to_string(out.size() - failed) +
                              " of " + std::to_string(out.size()) + " checks passed.";
            if (failed > 0) {
                box += "\n";
                for (const auto& r : out) {
                    if (!r.pass) {
                        box += "\n" + std::string(r.fatal ? "FAIL  " : "warn  ") + r.name + ": " +
                               r.detail;
                    }
                }
            }
            box += "\n\nFull report: IsekaiHeroSKSE.log";
            RE::DebugMessageBox(box.c_str());
        }
        // The automatic run draws NOTHING on screen, not even on a failure. It fires
        // while the loading screen is still up, so anything it posts is queued by the UI
        // and flushed at whatever unrelated moment the screen next frees up — which read
        // as "the self-test runs when I press ESC". A diagnostic that surfaces at a
        // random moment is worse than one that only writes to the log.
    }

    void RunOnLoad() {
        // Off the load handler and onto the next frame: the sweeps walk every recipe and
        // every ingredient in the load order, which has no business sitting inside the
        // engine's post-load pass.
        if (auto* task = SKSE::GetTaskInterface()) {
            task->AddTask([]() { Run(/*a_notify=*/false); });
        }
    }

    void Install() {
        // Scan code 0 means "off" — registering it armed a hotkey no key can ever produce
        // and then logged "hotkey 0x00 runs the self-test", which reads as if it were
        // ready. That line is the first thing anyone checks when the key does nothing.
        const auto key = Config::SelfTestKey();
        if (key == 0) {
            logger::info("SelfTest: no hotkey (SelfTestKey = 0). The report is written "
                         "automatically after every game load anyway.");
            return;
        }
        UI::RegisterHotkey(key, []() { Run(/*a_notify=*/true); });
        logger::info("SelfTest: hotkey {:#04x} runs the self-test", key);
    }
}
