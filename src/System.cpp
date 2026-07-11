#include "System.h"

namespace Isekai {

    namespace {
        State g_state;

        // --- Co-save serialization IDs ---
        constexpr std::uint32_t kSerID = 'ISKA';    // unique plugin id
        constexpr std::uint32_t kRecState = 'STAT';  // record tag
        constexpr std::uint32_t kVersion = 1;

        // ------------------------------------------------------------------
        // Reincarnation flow (skeleton — selection menu ported next)
        // ------------------------------------------------------------------

        void BeginReincarnation() {
            if (g_state.reincarnated) {
                return;
            }
            g_state.reincarnated = true;  // mark now so the trigger fires once

            logger::info("Reincarnation triggered — player ready (start-method independent)");
            RE::DebugNotification("[SYSTEM] Soul signature detected...");
            RE::DebugNotification("[SYSTEM] World System online. Reincarnation pending.");
        }

        // True once the player is really playing: 3D loaded, not paused, past
        // character creation, not on a loading screen, and in control (so we do
        // not fire during the Helgen cart ride or any forced-walk cutscene).
        [[nodiscard]] bool IsPlayerReady() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->Is3DLoaded()) {
                return false;
            }

            auto* ui = RE::UI::GetSingleton();
            if (!ui || ui->GameIsPaused()) {
                return false;
            }
            if (ui->IsMenuOpen("RaceSex Menu"sv) || ui->IsMenuOpen("Loading Menu"sv)) {
                return false;
            }

            auto* controls = RE::ControlMap::GetSingleton();
            if (!controls || !controls->IsMovementControlsEnabled()) {
                return false;
            }

            return true;
        }

        void TryTrigger() {
            if (g_state.reincarnated || !IsPlayerReady()) {
                return;
            }
            BeginReincarnation();
        }

        // ------------------------------------------------------------------
        // Trigger: watch menu open/close (fires on the main thread). Whenever a
        // menu closes we re-check readiness; the persisted flag guarantees the
        // reincarnation runs exactly once per character.
        // ------------------------------------------------------------------

        class MenuWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            static MenuWatcher* GetSingleton() {
                static MenuWatcher singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (a_event && !a_event->opening) {
                    TryTrigger();
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            MenuWatcher() = default;
        };

        // ------------------------------------------------------------------
        // Serialization callbacks
        // ------------------------------------------------------------------

        void SaveCallback(SKSE::SerializationInterface* a_intf) {
            if (a_intf->OpenRecord(kRecState, kVersion)) {
                a_intf->WriteRecordData(&g_state, sizeof(g_state));
            }
            logger::info("State saved (reincarnated={})", g_state.reincarnated);
        }

        void LoadCallback(SKSE::SerializationInterface* a_intf) {
            std::uint32_t type = 0;
            std::uint32_t version = 0;
            std::uint32_t length = 0;
            while (a_intf->GetNextRecordInfo(type, version, length)) {
                if (type == kRecState && version == kVersion && length == sizeof(g_state)) {
                    a_intf->ReadRecordData(&g_state, sizeof(g_state));
                }
            }
            logger::info("State loaded (reincarnated={})", g_state.reincarnated);
        }

        void RevertCallback(SKSE::SerializationInterface*) {
            g_state = State{};
            logger::info("State reverted to defaults (new game / pre-load)");
        }

        // ------------------------------------------------------------------
        // SKSE lifecycle
        // ------------------------------------------------------------------

        void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg) {
            if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
                if (auto* ui = RE::UI::GetSingleton()) {
                    ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatcher::GetSingleton());
                    logger::info("Menu watcher installed — reincarnation trigger armed");
                }
            }
        }
    }

    State& GetState() {
        return g_state;
    }

    void Install() {
        auto* serial = SKSE::GetSerializationInterface();
        serial->SetUniqueID(kSerID);
        serial->SetSaveCallback(SaveCallback);
        serial->SetLoadCallback(LoadCallback);
        serial->SetRevertCallback(RevertCallback);

        if (auto* messaging = SKSE::GetMessagingInterface()) {
            messaging->RegisterListener(OnSKSEMessage);
        }

        logger::info("Isekai System installed (serialization + trigger)");
    }
}
