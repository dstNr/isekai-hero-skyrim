#include "Storage.h"

#include "Plugin.h"
#include "Sounds.h"
#include "System.h"
#include "UI/SystemWindow.h"

#include <map>

namespace Isekai::Storage {

    namespace {
        // The container base object in IsekaiHero.esp ("Dimensional Storage",
        // read out of the plugin file itself).
        constexpr RE::FormID kContainerBase = 0x000D7A;

        RE::TESObjectCONT* g_base = nullptr;

        // What the crafting menu borrowed from storage, per base object. Never
        // persisted: the crafting menu pauses the game and cannot outlive a session.
        std::map<RE::TESBoundObject*, std::int32_t> g_craftLoan;

        // Storage is part of the blessing. NORMAL chose the pure challenge — the
        // System stays closed to them.
        [[nodiscard]] bool IsEligible() {
            const auto& state = GetState();
            return state.reincarnated && state.power != PowerLevel::Normal;
        }

        // The one chest reference, created on first use.
        //
        // There is no reference in the ESP on purpose: Skyrim's CK cannot mark a
        // reference persistent, and a non-persistent one only exists while its cell
        // is loaded — unreachable from anywhere else. PlaceObjectAtMe with
        // forcePersist creates a reference the save system tracks properly; its
        // FormID lives in our co-save (State::storageChest).
        [[nodiscard]] RE::TESObjectREFR* ResolveChest() {
            auto& state = GetState();
            if (state.storageChest == 0) {
                return nullptr;
            }
            auto* chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(state.storageChest);
            if (!chest) {
                // The save lost it (mangled by a save cleaner, most likely). Recreate
                // rather than dangle — the contents are gone either way, but the
                // feature keeps working.
                logger::warn("Storage: chest {:#x} vanished from the save — starting a new one",
                             state.storageChest);
                state.storageChest = 0;
            }
            return chest;
        }

        [[nodiscard]] RE::TESObjectREFR* GetOrCreateChest(RE::PlayerCharacter* a_player) {
            if (auto* chest = ResolveChest()) {
                return chest;
            }
            if (!g_base) {
                return nullptr;
            }

            const auto chest = a_player->PlaceObjectAtMe(g_base, /*forcePersist=*/true);
            if (!chest) {
                logger::error("Storage: PlaceObjectAtMe failed");
                return nullptr;
            }

            GetState().storageChest = chest->GetFormID();
            logger::info("Storage: chest created ({:#x})", chest->GetFormID());
            return chest.get();
        }

        // ------------------------------------------------------------------
        // Crafting: lend the storage to the player while a crafting menu is open.
        // Vanilla crafting only ever looks at the player's inventory, so the
        // materials walk over for the duration and the leftovers walk back.
        // ------------------------------------------------------------------

        void LendToPlayer() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* chest = ResolveChest();
            if (!player || !chest || !IsEligible()) {
                return;
            }

            g_craftLoan.clear();
            for (const auto& [obj, count] : chest->GetInventoryCounts()) {
                if (!obj || count <= 0) {
                    continue;
                }
                g_craftLoan[obj] = count;
                chest->RemoveItem(obj, count, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr,
                                  player);
            }

            if (!g_craftLoan.empty()) {
                logger::info("Storage: lent {} stack(s) to the crafting menu", g_craftLoan.size());
            }
        }

        // Return min(borrowed, still held): whatever crafting consumed stays
        // consumed, whatever it produced stays with the player, and the player's
        // own pre-existing materials never get swept into the chest.
        void TakeBack() {
            if (g_craftLoan.empty()) {
                return;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* chest = ResolveChest();
            if (!player || !chest) {
                g_craftLoan.clear();
                return;
            }

            const auto held = player->GetInventoryCounts();
            for (const auto& [obj, borrowed] : g_craftLoan) {
                const auto it = held.find(obj);
                const auto stillHeld = it != held.end() ? it->second : 0;
                const auto giveBack = std::min(borrowed, stillHeld);
                if (giveBack > 0) {
                    player->RemoveItem(obj, giveBack, RE::ITEM_REMOVE_REASON::kStoreInContainer,
                                       nullptr, chest);
                }
            }
            g_craftLoan.clear();
        }

        class CraftWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            static CraftWatcher* GetSingleton() {
                static CraftWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (a_event && a_event->menuName == RE::CraftingMenu::MENU_NAME) {
                    if (a_event->opening) {
                        LendToPlayer();
                    } else {
                        TakeBack();
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            CraftWatcher() = default;
        };
    }

    bool Available() {
        return g_base != nullptr && IsEligible();
    }

    void Open() {
        if (UI::IsSystemWindowOpen()) {
            return;
        }
        if (!IsEligible()) {
            RE::DebugNotification("[ SYSTEM ] ACCESS DENIED — dimensional storage requires a blessing.");
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) {
            return;
        }

        auto* chest = GetOrCreateChest(player);
        if (!chest) {
            return;
        }

        // Keep the chest in the player's cell (so it is loaded and activatable), but
        // far below the floor, where its model can never be seen. The activation is a
        // direct call, not a look-at, so where it sits makes no difference.
        chest->MoveTo(player);
        const auto pos = player->GetPosition();
        chest->SetPosition(pos.x, pos.y, pos.z - 3000.0f);

        Sounds::Play(Sounds::Sfx::WindowOpen);
        chest->ActivateRef(player, 0, nullptr, 1, false);
    }

    void Install() {
        if (!Plugin::IsLoaded()) {
            logger::warn("Storage: {} not loaded — dimensional storage is off",
                         Plugin::kFileName);
            return;
        }

        auto* data = RE::TESDataHandler::GetSingleton();
        g_base = data ? data->LookupForm<RE::TESObjectCONT>(kContainerBase, Plugin::kFileName)
                      : nullptr;
        if (!g_base) {
            logger::error("Storage: no container {:#08x} in {}", kContainerBase,
                          Plugin::kFileName);
            return;
        }

        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(CraftWatcher::GetSingleton());
        }
        logger::info("Storage: reachable via the System panel; crafting borrows its inventory");
    }
}
