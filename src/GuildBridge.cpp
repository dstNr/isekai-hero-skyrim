#include "GuildBridge.h"

#include "Config.h"
#include "Loc.h"
#include "System.h"
#include "UI/Hud.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace Isekai::GuildBridge {

    namespace {

        constexpr const char* kToastKey = "guild";
        constexpr char        kLetters[] = "EDCBAS";

        // Main thread only (GrantSystemPoints and the toast). a_weight is per rank step:
        // an E-rank job is worth 1, an S-rank one 6, a promotion three times its rank.
        void Pay(std::string a_event, std::string a_what, int a_rank, int a_weight) {
            if (!GetState().reincarnated) {
                return;  // no System bound yet; the guild's work is the guild's
            }
            const int  rank = std::clamp(a_rank, 0, 5);
            const auto points = static_cast<std::int32_t>(std::lround(
                (rank + 1) * a_weight * static_cast<float>(Config::AdventurersGuildPoints()) *
                RewardScale()));
            if (points <= 0) {
                return;
            }
            GrantSystemPoints(points);
            logger::info("GuildBridge: {} '{}' rank {} -> +{} SP", a_event, a_what, kLetters[rank],
                         points);
            UI::ShowToast(LF("guild.payout", "[ SYSTEM ]  Guild work recognised  —  +{} SP", points),
                          kToastKey);
        }

        class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
        public:
            static Sink* GetSingleton() {
                static Sink singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
                                                  RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
                if (!a_event) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const std::string name(a_event->eventName.c_str());
                int weight = 0;
                if (name == "AG_MissiveCompleted" || name == "AG_DungeonCleared") {
                    weight = 1;
                } else if (name == "AG_RankChanged") {
                    weight = 3;
                } else {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const std::string what(a_event->strArg.c_str());
                const int         rank = static_cast<int>(a_event->numArg);
                // The guild sends from wherever its own work runs; the System's state is
                // touched on the main thread only.
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([name, what, rank, weight]() { Pay(name, what, rank, weight); });
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            Sink() = default;
        };
    }

    void Install() {
        if (Config::AdventurersGuildPoints() == 0) {
            logger::info("GuildBridge: off (AdventurersGuildPoints = 0)");
            return;
        }
        auto* source = SKSE::GetModCallbackEventSource();
        if (!source) {
            logger::error("GuildBridge: no mod event source — Adventurers Guild work will not pay");
            return;
        }
        source->AddEventSink(Sink::GetSingleton());
        logger::info("GuildBridge: listening for Adventurers Guild events (x{} per rank step)",
                     Config::AdventurersGuildPoints());
    }
}
