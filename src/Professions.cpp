#include "Professions.h"

#include "Config.h"
#include "Loc.h"
#include "System.h"
#include "UI/Toast.h"

namespace Isekai::Professions {

    namespace {

        constexpr const char* kToastKey = "profession";

        // One place decides what an action is worth, so the two sinks below cannot drift
        // apart. Returns true when the action paid out, which is only ever used for the
        // log line - the points themselves are granted here.
        void CountAction(const char* a_what) {
            const auto per = Config::ProfessionActionsPerPoint();
            if (per == 0) {
                return;  // professions switched off
            }

            auto& state = GetState();
            if (!state.reincarnated) {
                return;  // no System bound yet; nothing is watching
            }

            const auto count = ++state.professionActions;
            if (count % static_cast<std::int32_t>(per) != 0) {
                return;
            }

            GrantSystemPoints(1);
            logger::info("Professions: {} actions ({}) — System Point paid", count, a_what);
            UI::ShowToast(L("profession.payout", "[ SYSTEM ]  Craft recognised  —  +1 SP"),
                          kToastKey);
        }

        class HarvestWatcher : public RE::BSTEventSink<RE::TESHarvestedEvent::ItemHarvested> {
        public:
            static HarvestWatcher* GetSingleton() {
                static HarvestWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESHarvestedEvent::ItemHarvested* a_event,
                RE::BSTEventSource<RE::TESHarvestedEvent::ItemHarvested>*) override {
                if (a_event && a_event->harvester == RE::PlayerCharacter::GetSingleton()) {
                    CountAction("harvest");
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            HarvestWatcher() = default;
        };

        // There is no event for "the player crafted something": BGSCraftItemEvent exists as
        // a type but CommonLibSSE-NG exposes no source to register against. So it is read
        // off the inventory instead, and the discriminator is oldContainer.
        //
        // An item that arrives with NO old container was created from nothing, which is
        // what crafting does. Taking ore out of the Dimensional Storage at the same forge
        // carries the chest as its old container, so it does not count — which matters
        // here more than anywhere, because this mod routes storage THROUGH those stations.
        class CraftWatcher : public RE::BSTEventSink<RE::TESContainerChangedEvent> {
        public:
            static CraftWatcher* GetSingleton() {
                static CraftWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESContainerChangedEvent*  a_event,
                RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
                if (!a_event || a_event->oldContainer != 0) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player || a_event->newContainer != player->GetFormID()) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                // Only at a station. Without this, every quest reward and every console
                // additem would read as a craft.
                auto* ui = RE::UI::GetSingleton();
                if (!ui || !ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME)) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                CountAction("craft");
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            CraftWatcher() = default;
        };
    }

    void Install() {
        if (Config::ProfessionActionsPerPoint() == 0) {
            logger::info("Professions: off (ProfessionActionsPerPoint = 0)");
            return;
        }

        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (!holder) {
            logger::error("Professions: no event source holder — crafting will not pay");
            return;
        }
        holder->AddEventSink<RE::TESContainerChangedEvent>(CraftWatcher::GetSingleton());

        if (auto* source = RE::TESHarvestedEvent::GetEventSource()) {
            source->AddEventSink(HarvestWatcher::GetSingleton());
        } else {
            logger::error("Professions: no harvest event source — harvesting will not pay");
        }

        logger::info("Professions: watching harvest and craft ({} actions per System Point)",
                     Config::ProfessionActionsPerPoint());
    }
}
