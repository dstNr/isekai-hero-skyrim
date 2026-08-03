#include "SelfTest.h"

#include "Config.h"
#include "CraftHooks.h"
#include "Plugin.h"
#include "Quests.h"
#include "Shop.h"
#include "Storage.h"
#include "System.h"
#include "UI/Input.h"
#include "UI/Prisma.h"

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
