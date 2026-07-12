#include "System.h"

#include "UI/Overlay.h"
#include "UI/SystemWindow.h"

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

        std::string PowerName(PowerLevel a_power) {
            switch (a_power) {
            case PowerLevel::Hero:
                return "HERO";
            case PowerLevel::Ascended:
                return "ASCENDED";
            default:
                return "NORMAL";
            }
        }

        // ---- Reincarnation flow (choice captured into g_state) ----

        // perkCount is a signed 8-bit field, so this is as many as the game can hold.
        constexpr std::int32_t kMaxPerkPoints = 127;

        // What each blessing grants. 0 = leave that stat untouched.
        struct Blessing {
            std::uint16_t skillLevel;   // set all 18 skills to this
            std::uint16_t playerLevel;  // set character level
            std::int32_t  perkPoints;   // add to available perk points
            float         attrBonus;    // add to base Health/Magicka/Stamina
            std::int32_t  gold;         // add to inventory
        };

        Blessing BlessingFor(PowerLevel a_power) {
            switch (a_power) {
            case PowerLevel::Hero:
                return { 50, 25, 10, 100.0f, 2000 };
            case PowerLevel::Ascended:
                return { 100, 150, kMaxPerkPoints, 300.0f, 25000 };
            default:  // Normal — pure challenge, no boosts
                return { 0, 0, 0, 0.0f, 0 };
            }
        }

        void ApplyReincarnation() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }

            const Blessing b = BlessingFor(g_state.power);
            auto* avOwner = player->AsActorValueOwner();

            // Skills: the 18 skill actor values are contiguous (kOneHanded..kEnchanting).
            if (b.skillLevel > 0 && avOwner) {
                for (int av = static_cast<int>(RE::ActorValue::kOneHanded);
                     av <= static_cast<int>(RE::ActorValue::kEnchanting); ++av) {
                    avOwner->SetBaseActorValue(static_cast<RE::ActorValue>(av),
                                               static_cast<float>(b.skillLevel));
                }
            }

            // Attributes: add a flat bonus on top of the current base.
            if (b.attrBonus > 0.0f && avOwner) {
                for (auto av : { RE::ActorValue::kHealth, RE::ActorValue::kMagicka,
                                 RE::ActorValue::kStamina }) {
                    avOwner->SetBaseActorValue(av, avOwner->GetBaseActorValue(av) + b.attrBonus);
                }
            }

            // Character level: for the player this lives on the ActorBase (TESNPC).
            if (b.playerLevel > 0) {
                if (auto* base = player->GetActorBase()) {
                    base->actorData.level = b.playerLevel;
                }
            }

            if (b.perkPoints > 0) {
                auto& stats = player->GetGameStatsData();
                const int total = static_cast<int>(stats.perkCount) + b.perkPoints;
                stats.perkCount = static_cast<std::int8_t>(std::min(total, kMaxPerkPoints));
            }

            // Gold (Gold001 = 0x0000000F).
            if (b.gold > 0) {
                if (auto* gold = RE::TESForm::LookupByID<RE::TESObjectMISC>(0x0000000F)) {
                    player->AddObjectToContainer(gold, nullptr, b.gold, nullptr);
                }
            }

            logger::info("Reincarnation applied: power={} skills={} level={} perks=+{} attr=+{} gold={}",
                         PowerName(g_state.power), b.skillLevel, b.playerLevel, b.perkPoints,
                         b.attrBonus, b.gold);

            const std::string body =
                "REINCARNATION COMPLETE\n"
                "\n"
                "  Power level   " + PowerName(g_state.power) + "\n"
                "\n"
                "The System is now bound to your soul.\n"
                "Your new life begins.";

            UI::ShowSystemWindow("[ SYSTEM ]", body, { "CONTINUE" }, [](int) {});
        }

        void ShowPowerSelection() {
            UI::ShowSystemWindow(
                "[ SYSTEM ]",
                "You have been reincarnated.\n"
                "The System offers you a blessing.\n"
                "\n"
                "  NORMAL    No blessing. Pure challenge.\n"
                "  HERO      Awakened power.\n"
                "  ASCENDED  Transcend mortal limits.\n"
                "\n"
                "Choose your path:",
                { "NORMAL", "HERO", "ASCENDED" },
                [](int a_idx) {
                    g_state.power = static_cast<PowerLevel>(std::clamp(a_idx, 0, 2));
                    logger::info("Power level selected: {} ({})", a_idx, PowerName(g_state.power));
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
                UI::Install();

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
