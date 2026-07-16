#include "Storage.h"

#include "Plugin.h"
#include "Sounds.h"
#include "System.h"
#include "UI/SystemWindow.h"

#include <map>

namespace Isekai::Storage {

    namespace {
        // Forms in IsekaiHero.esp, read out of the plugin file itself.
        constexpr RE::FormID kContainerBase = 0x000D7A;  // CONT "Dimensional Storage"
        constexpr RE::FormID kTokenID = 0x000000;        // ALCH token — 0 until created

        RE::TESObjectCONT* g_base = nullptr;
        RE::AlchemyItem*   g_token = nullptr;

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

        // ------------------------------------------------------------------
        // The token: an effect-less ALCH item, "used" from the inventory like a
        // potion. Consuming it fires TESEquipEvent; the token is handed back
        // immediately, so it never actually runs out.
        //
        // Deliberately not a ring or any other wearable: equipping touches biped
        // slots, and every slot is contested territory between mods (cloaks,
        // bandoliers, ...). Consumption touches no slot at all — zero conflict
        // surface. The record's name and model are still free, so it does not have
        // to look like a potion; only the inventory category says so.
        // ------------------------------------------------------------------

        class TokenWatcher : public RE::BSTEventSink<RE::TESEquipEvent> {
        public:
            static TokenWatcher* GetSingleton() {
                static TokenWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESEquipEvent*                a_event,
                RE::BSTEventSource<RE::TESEquipEvent>*) override {
                if (!a_event || !g_token || !a_event->equipped ||
                    a_event->baseObject != g_token->GetFormID() ||
                    a_event->actor.get() != RE::PlayerCharacter::GetSingleton()) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (auto* task = SKSE::GetTaskInterface()) {
                    task->AddTask([]() {
                        // Hand the consumed token straight back before opening.
                        if (auto* player = RE::PlayerCharacter::GetSingleton(); player && g_token) {
                            player->AddObjectToContainer(g_token, nullptr, 1, nullptr);
                        }
                        Open();
                    });
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            TokenWatcher() = default;
        };
    }

    void Open() {
        auto* ui = RE::UI::GetSingleton();
        if (UI::IsSystemWindowOpen() || !ui) {
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

    void EnsureToken() {
        if (!g_token || !IsEligible()) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        const auto held = player->GetInventoryCounts();
        const auto it = held.find(g_token);
        if (it == held.end() || it->second <= 0) {
            player->AddObjectToContainer(g_token, nullptr, 1, nullptr);
            logger::info("Storage: token handed to the player");
        }
    }

    void Install() {
        if (!Plugin::IsLoaded()) {
            logger::warn("Storage: {} not loaded — dimensional storage is off",
                         Plugin::kFileName);
            return;
        }

        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            return;
        }

        g_base = data->LookupForm<RE::TESObjectCONT>(kContainerBase, Plugin::kFileName);
        if (!g_base) {
            logger::error("Storage: no container {:#08x} in {}", kContainerBase,
                          Plugin::kFileName);
            return;
        }

        if (kTokenID != 0) {
            g_token = data->LookupForm<RE::AlchemyItem>(kTokenID, Plugin::kFileName);
        }
        if (g_token) {
            if (auto* events = RE::ScriptEventSourceHolder::GetSingleton()) {
                events->AddEventSink<RE::TESEquipEvent>(TokenWatcher::GetSingleton());
            }
            logger::info("Storage: token wired — usable from the inventory");
        } else {
            logger::warn("Storage: token record missing — storage unreachable until it exists");
        }

        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(CraftWatcher::GetSingleton());
            logger::info("Storage: crafting menus now borrow the storage inventory");
        }
    }
}
