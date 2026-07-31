#include "Shop.h"

#include "Plugin.h"
#include "Sounds.h"
#include "Storage.h"
#include "System.h"
#include "UI/SystemWindow.h"

#include <string>
#include <vector>

namespace Isekai::Shop {

    namespace {
        constexpr RE::FormID kGold = 0x0000000F;

        // One representative filled soul gem per grade from the official masters — same
        // dedup idiom SkillTree's UnlockAllEnchantments uses (lowest FormID wins, so the
        // pick is deterministic across loads). Resolved once at Install(), not on every
        // catalog open.
        RE::TESSoulGem* g_grand = nullptr;
        RE::TESSoulGem* g_common = nullptr;

        [[nodiscard]] RE::TESSoulGem* FindSoulGem(RE::SOUL_LEVEL a_grade) {
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data) {
                return nullptr;
            }
            RE::TESSoulGem* best = nullptr;
            for (auto* gem : data->GetFormArray<RE::TESSoulGem>()) {
                if (!gem || !Plugin::IsOfficialMaster(gem) || gem->GetContainedSoul() != a_grade) {
                    continue;
                }
                if (!best || gem->GetFormID() < best->GetFormID()) {
                    best = gem;
                }
            }
            return best;
        }

        // Spend a_cost System Points and hand a_obj/a_count to the storage chest. Declines
        // quietly (a notification, not a crash) if the player can't afford it or the chest
        // is not ready — both realistic states a stale, re-opened catalog can hit.
        void Purchase(std::int32_t a_cost, RE::TESBoundObject* a_obj, std::int32_t a_count) {
            auto& state = GetState();
            if (state.systemPoints < a_cost) {
                RE::DebugNotification("[ SYSTEM ] Not enough System Points.");
                return;
            }
            auto* chest = Storage::ChestRef();
            if (!chest || !a_obj) {
                RE::DebugNotification("[ SYSTEM ] The Dimensional Storage is not ready yet.");
                return;
            }
            state.systemPoints -= a_cost;
            chest->AddObjectToContainer(a_obj, nullptr, a_count, nullptr);
            Sounds::Play(Sounds::Sfx::ButtonClick);
            logger::info("Shop: bought {}x {} for {} System Point(s)", a_count, a_obj->GetName(),
                         a_cost);
        }
    }

    void Install() {
        g_grand = FindSoulGem(RE::SOUL_LEVEL::kGrand);
        g_common = FindSoulGem(RE::SOUL_LEVEL::kCommon);
        logger::info("Shop: catalog resolved (grand={}, common={})", g_grand != nullptr,
                     g_common != nullptr);
    }

    bool Available() {
        return Storage::Available();
    }

    void Open() {
        // Same guard Storage::Open() uses: reached as a status-panel action, so the
        // panel that led here has already dismissed itself by the time this runs.
        if (UI::IsSystemWindowOpen()) {
            return;
        }
        if (!Available()) {
            RE::DebugNotification("[ SYSTEM ] ACCESS DENIED — the System is not yet bound to you.");
            return;
        }

        const std::string body =
            "SYSTEM SHOP\n"
            "\n"
            "System Points   " + std::to_string(GetState().systemPoints) + "\n"
            "\n"
            "Spend what the tree no longer needs. Delivered straight into your\n"
            "Dimensional Storage.";

        std::vector<UI::Choice>            choices;
        std::vector<std::function<void()>> actions;

        if (g_grand) {
            choices.push_back({ "Grand Soul Gem x1  —  25 SP", {} });
            actions.emplace_back([]() {
                Purchase(25, g_grand, 1);
                Open();  // refresh with the new balance, so buying several is one click each
            });
        }
        if (g_common) {
            choices.push_back({ "Common Soul Gem x5  —  15 SP", {} });
            actions.emplace_back([]() {
                Purchase(15, g_common, 5);
                Open();
            });
        }
        choices.push_back({ "Gold x1000  —  10 SP", {} });
        actions.emplace_back([]() {
            Purchase(10, RE::TESForm::LookupByID<RE::TESBoundObject>(kGold), 1000);
            Open();
        });
        choices.push_back({ "CLOSE", {} });
        actions.emplace_back([]() {});

        UI::ShowSystemWindow(
            "[ SYSTEM ]", body, std::move(choices),
            [actions = std::move(actions)](int a_idx) {
                if (a_idx >= 0 && static_cast<std::size_t>(a_idx) < actions.size()) {
                    actions[static_cast<std::size_t>(a_idx)]();
                }
            },
            100000.0f);
    }
}
