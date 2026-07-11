#include "System.h"

namespace Isekai {

    namespace {
        State g_state;

        // Runs the reincarnation intro. For now this is the entry hook — the
        // full origin/power selection flow will be ported here step by step.
        void BeginReincarnation() {
            logger::info("New game — Isekai reincarnation protocol pending");

            // UI/game calls must run on the main thread.
            SKSE::GetTaskInterface()->AddTask([]() {
                RE::DebugNotification("[SYSTEM] Soul signature detected...");
                RE::DebugNotification("[SYSTEM] World System v3.0 online. Reincarnation pending.");
            });
        }

        void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg) {
            switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                logger::info("All data loaded — World System online");
                break;

            case SKSE::MessagingInterface::kNewGame:
                g_state = State{};  // fresh state for a new character
                BeginReincarnation();
                break;

            case SKSE::MessagingInterface::kPostLoadGame:
                logger::info("Save game loaded (reincarnated={})", g_state.reincarnated);
                break;

            default:
                break;
            }
        }
    }

    State& GetState() {
        return g_state;
    }

    void RegisterMessageListener() {
        const auto messaging = SKSE::GetMessagingInterface();
        if (messaging && messaging->RegisterListener(OnSKSEMessage)) {
            logger::info("SKSE message listener registered");
        } else {
            logger::error("Failed to register SKSE message listener");
        }
    }
}
