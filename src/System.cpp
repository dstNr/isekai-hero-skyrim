#include "System.h"

#include "Passives.h"
#include "Plugin.h"
#include "Progression.h"
#include "Sounds.h"
#include "Storage.h"
#include "UI/LevelUpEffect.h"
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
        constexpr std::uint32_t kVersion = 4;        // bumped: storage chest ref added

        void SystemMsg(const char* a_text) {
            RE::DebugNotification(a_text);
        }

        // ---- Reincarnation flow (choice captured into g_state) ----

        // perkCount is a signed 8-bit field, so this is as many as the game can hold.
        constexpr std::int32_t kMaxPerkPoints = 127;

        // What each blessing grants. 0 = leave that stat untouched.
        struct Blessing {
            std::uint16_t skillLevel;    // set all 18 skills to this
            std::uint16_t playerLevel;   // set character level
            std::int32_t  perkPoints;    // add to available perk points
            std::int32_t  gold;          // add to inventory
            std::int32_t  dragonSouls;   // add to the unspent soul pool
        };

        Blessing BlessingFor(PowerLevel a_power) {
            switch (a_power) {
            case PowerLevel::Hero:
                return { 50, 25, 10, 2000, 3 };
            case PowerLevel::Ascended:
                return { 100, 150, kMaxPerkPoints, 25000, 20 };
            default:  // Normal — pure challenge, no boosts
                return { 0, 0, 0, 0, 0 };
            }
        }

        // Health/Magicka/Stamina are not a separate knob: they follow the granted
        // level. A character who levelled to N by hand would have banked (N-1) * 10
        // attribute points; we spread them evenly across the big three. A flat +300
        // used to make ASCENDED a paper giant — level 150 with level-30 stats.
        [[nodiscard]] float AttributeBonusPerStat(std::uint16_t a_playerLevel) {
            if (a_playerLevel <= 1) {
                return 0.0f;
            }
            return std::round(static_cast<float>(a_playerLevel - 1) * 10.0f / 3.0f);
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

            // Attributes: what (playerLevel - 1) level-ups would have paid out,
            // spread evenly, on top of the current base.
            const float attrBonus = AttributeBonusPerStat(b.playerLevel);
            if (attrBonus > 0.0f && avOwner) {
                for (auto av : { RE::ActorValue::kHealth, RE::ActorValue::kMagicka,
                                 RE::ActorValue::kStamina }) {
                    avOwner->SetBaseActorValue(av, avOwner->GetBaseActorValue(av) + attrBonus);
                }
            }

            // Character level: for the player this lives on the ActorBase (TESNPC).
            if (b.playerLevel > 0) {
                if (auto* base = player->GetActorBase()) {
                    base->actorData.level = b.playerLevel;
                }
            }

            GrantPerkPoints(b.perkPoints);
            GrantDragonSouls(b.dragonSouls);
            Storage::EnsureToken();  // HERO and ASCENDED get the storage; NORMAL does not

            // Gold (Gold001 = 0x0000000F).
            if (b.gold > 0) {
                if (auto* gold = RE::TESForm::LookupByID<RE::TESObjectMISC>(0x0000000F)) {
                    player->AddObjectToContainer(gold, nullptr, b.gold, nullptr);
                }
            }

            logger::info(
                "Reincarnation applied: power={} skills={} level={} perks=+{} attr=+{} gold={} souls=+{}",
                PowerName(g_state.power), b.skillLevel, b.playerLevel, b.perkPoints, attrBonus,
                b.gold, b.dragonSouls);

            const std::string body =
                "REINCARNATION COMPLETE\n"
                "\n"
                "  Power level   " + PowerName(g_state.power) + "\n"
                "\n"
                "The System is now bound to your soul.\n"
                "Your new life begins.";

            // The biggest moment the mod has: let the flourish land before the panel.
            Sounds::Play(Sounds::Sfx::LevelUp);
            UI::PlayLevelUpEffect("AWAKENED", PowerName(g_state.power));

            DelayedMainThread(1600, [body]() {
                UI::ShowSystemWindow("[ SYSTEM ]", body, { "CONTINUE" }, [](int) {});
            });
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

        // Written field by field, not as one memcpy of the struct: State now holds a
        // vector, whose bytes are a heap pointer, not the data. Blitting the struct
        // would write a pointer into the save and read it back as garbage.
        void SaveCallback(SKSE::SerializationInterface* a_intf) {
            if (!a_intf->OpenRecord(kRecState, kVersion)) {
                logger::error("Could not open the save record — state not written");
                return;
            }

            a_intf->WriteRecordData(g_state.reincarnated);
            a_intf->WriteRecordData(g_state.power);
            a_intf->WriteRecordData(g_state.skills);
            a_intf->WriteRecordData(g_state.storageChest);

            const auto count = static_cast<std::uint32_t>(g_state.grantedMilestones.size());
            a_intf->WriteRecordData(count);
            for (const auto key : g_state.grantedMilestones) {
                a_intf->WriteRecordData(key);
            }

            logger::info("State saved (reincarnated={}, milestones={})", g_state.reincarnated,
                         count);
        }

        void LoadCallback(SKSE::SerializationInterface* a_intf) {
            std::uint32_t type = 0;
            std::uint32_t version = 0;
            std::uint32_t length = 0;

            while (a_intf->GetNextRecordInfo(type, version, length)) {
                if (type != kRecState) {
                    continue;
                }
                // v3 is still readable (it simply predates the storage chest); anything
                // else is skipped rather than misread. The player then loses the
                // System's memory of past milestones, not their character.
                if (version != kVersion && version != 3) {
                    logger::warn("Save holds state version {} but we speak {} — ignoring it",
                                 version, kVersion);
                    continue;
                }

                a_intf->ReadRecordData(g_state.reincarnated);
                a_intf->ReadRecordData(g_state.power);
                a_intf->ReadRecordData(g_state.skills);

                if (version >= 4) {
                    a_intf->ReadRecordData(g_state.storageChest);
                    // A created reference keeps its FormID inside one save, but SKSE
                    // may still remap across load-order changes — resolve, don't assume.
                    RE::FormID resolved = 0;
                    if (g_state.storageChest != 0 &&
                        a_intf->ResolveFormID(g_state.storageChest, resolved)) {
                        g_state.storageChest = resolved;
                    }
                }

                std::uint32_t count = 0;
                a_intf->ReadRecordData(count);
                g_state.grantedMilestones.clear();
                g_state.grantedMilestones.reserve(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    std::uint32_t key = 0;
                    a_intf->ReadRecordData(key);
                    g_state.grantedMilestones.push_back(key);
                }
            }

            logger::info("State loaded (reincarnated={}, milestones={})", g_state.reincarnated,
                         g_state.grantedMilestones.size());
        }

        void RevertCallback(SKSE::SerializationInterface*) {
            g_state = State{};
            logger::info("State reverted to defaults (new game / pre-load)");
        }

        // ------------------------------------------------------------------
        // SKSE lifecycle
        // ------------------------------------------------------------------

        void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg) {
            switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                Plugin::DumpForms();
                Passives::Install();
                Sounds::Install();
                Storage::Install();
                UI::Install();
                Progression::Install();

                if (auto* ui = RE::UI::GetSingleton()) {
                    ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatcher::GetSingleton());
                    logger::info("Menu watcher installed — reincarnation trigger armed");
                }
                break;

            // Both fire after the co-save has been read back, so grantedMilestones is
            // populated and the catch-up knows what it already owes.
            case SKSE::MessagingInterface::kPostLoadGame:
            case SKSE::MessagingInterface::kNewGame:
                Progression::CatchUpOnLoad();
                // Ability magnitudes live in the plugin, not the save, so they come back
                // as whatever the ESP says (zero) on every load. Rebuild them from the
                // milestones the save *does* remember.
                Passives::Refresh();
                // Characters blessed before the token existed receive theirs now.
                Storage::EnsureToken();
                break;

            default:
                break;
            }
        }
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

    void DelayedMainThread(std::uint32_t a_ms, std::function<void()> a_fn) {
        std::thread([a_ms, fn = std::move(a_fn)]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(a_ms));
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([fn = std::move(fn)]() { fn(); });
            }
        }).detach();
    }

    State& GetState() {
        return g_state;
    }

    float RewardScale() {
        switch (g_state.power) {
        case PowerLevel::Hero:
            return 2.0f;
        case PowerLevel::Ascended:
            return 4.0f;
        default:  // Normal is the baseline the table is written against
            return 1.0f;
        }
    }

    std::int32_t MilestonePerkPoints() {
        // The same amount the blessing itself paid: the choice made at the start keeps
        // paying out at every endpoint. NORMAL took no blessing, so it earns no perk
        // points here either — it grows through passives alone.
        return BlessingFor(g_state.power).perkPoints;
    }

    void GrantPerkPoints(std::int32_t a_points) {
        if (a_points <= 0) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }
        auto&      stats = player->GetGameStatsData();
        const auto total = static_cast<std::int32_t>(stats.perkCount) + a_points;
        stats.perkCount = static_cast<std::int8_t>(std::min(total, kMaxPerkPoints));
    }

    void GrantDragonSouls(std::int32_t a_souls) {
        if (a_souls <= 0) {
            return;
        }
        // The unspent soul pool is just an actor value — no special API, and no cap
        // to fight, unlike perk points.
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* avOwner = player ? player->AsActorValueOwner() : nullptr;
        if (!avOwner) {
            return;
        }
        const float have = avOwner->GetBaseActorValue(RE::ActorValue::kDragonSouls);
        avOwner->SetBaseActorValue(RE::ActorValue::kDragonSouls,
                                   have + static_cast<float>(a_souls));
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
