#include "System.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace Isekai {

    namespace {
        State g_state;

        // --- Co-save serialization IDs ---
        constexpr std::uint32_t kSerID = 'ISKA';    // unique plugin id
        constexpr std::uint32_t kRecState = 'STAT';  // record tag
        constexpr std::uint32_t kVersion = 2;        // bumped: origin removed from State

        // ------------------------------------------------------------------
        // Reincarnation flow (skeleton — selection menu ported next)
        // ------------------------------------------------------------------

        // ---- Reusable vanilla message box (buttons + callback w/ selected index) ----

        class ButtonCallback : public RE::IMessageBoxCallback {
        public:
            explicit ButtonCallback(std::function<void(int)> a_fn) : _fn(std::move(a_fn)) {}

            void Run(Message a_msg) override {
                if (_fn) {
                    _fn(static_cast<int>(a_msg));  // enum value == 0-based button index
                }
            }

        private:
            std::function<void(int)> _fn;
        };

        void ShowMessageBox(const std::string& a_body, std::vector<std::string> a_buttons,
                            std::function<void(int)> a_onSelect) {
            auto* factoryManager = RE::MessageDataFactoryManager::GetSingleton();
            auto* strings = RE::InterfaceStrings::GetSingleton();
            if (!factoryManager || !strings) {
                return;
            }
            auto* creator = factoryManager->GetCreator<RE::MessageBoxData>(strings->messageBoxData);
            if (!creator) {
                return;
            }
            auto* mbox = creator->Create();
            if (!mbox) {
                return;
            }

            mbox->callback = RE::make_smart<ButtonCallback>(std::move(a_onSelect));
            mbox->bodyText = RE::BSString(a_body);
            for (const auto& b : a_buttons) {
                mbox->buttonText.push_back(RE::BSString(b));
            }
            mbox->QueueMessage();
        }

        // ---- Timing helpers ----

        // Run fn on the main thread after a_ms. Game/UI calls must be on the main
        // thread, so a detached timer thread marshals back via the task interface.
        void DelayedMainThread(std::uint32_t a_ms, std::function<void()> a_fn) {
            std::thread([a_ms, fn = std::move(a_fn)]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(a_ms));
                if (auto* task = SKSE::GetTaskInterface()) {
                    task->AddTask([fn = std::move(fn)]() { fn(); });
                }
            }).detach();
        }

        void SystemMsg(const char* a_text) {
            RE::DebugNotification(a_text);
        }

        std::string RankName(PowerLevel a_power) {
            switch (a_power) {
            case PowerLevel::Hero:
                return "S-RANK";
            case PowerLevel::Ascended:
                return "MONARCH";
            default:
                return "E-RANK";
            }
        }

        // ---- Reincarnation flow (choice captured into g_state) ----

        void ApplyReincarnation() {
            // Placeholder: skill/perk/level/equipment application ported next.
            logger::info("Reincarnation applied: rank={}", RankName(g_state.power));

            const std::string body =
                "═══════════════════════════\n"
                "    REINCARNATION COMPLETE\n"
                "═══════════════════════════\n\n"
                "Awakening rank acquired: " + RankName(g_state.power) + "\n"
                "The System is now bound to your soul.\n"
                "Your new life begins.";

            ShowMessageBox(body, { "Continue" }, [](int) {});
        }

        void ShowPowerSelection() {
            ShowMessageBox(
                "═══════════════════════════\n"
                "         [ SYSTEM ]\n"
                "═══════════════════════════\n\n"
                "You have been chosen.\n"
                "Select your Awakening rank:",
                {
                    "E-RANK   -  No blessing (challenge)",
                    "S-RANK   -  Hero awakening",
                    "MONARCH  -  Ascension (godlike)",
                },
                [](int a_idx) {
                    g_state.power = static_cast<PowerLevel>(std::clamp(a_idx, 0, 2));
                    logger::info("Awakening rank selected: {} ({})", a_idx, RankName(g_state.power));
                    ApplyReincarnation();
                });
        }

        void BeginReincarnation() {
            if (g_state.reincarnated) {
                return;
            }
            g_state.reincarnated = true;  // fire exactly once per character

            logger::info("Reincarnation triggered — System boot sequence");

            // Solo-Leveling style boot: paced [SYSTEM] messages, then the rank menu.
            SystemMsg("[ SYSTEM ] Soul signature detected...");
            DelayedMainThread(1500, []() { SystemMsg("[ SYSTEM ] Analyzing dimensional residue..."); });
            DelayedMainThread(3000, []() { SystemMsg("[ SYSTEM ] Awakening protocol ready."); });
            DelayedMainThread(4500, []() { ShowPowerSelection(); });
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
