#include "Readiness.h"

namespace Isekai::Readiness {

    namespace {
        // The cell the player was FIRST ready in. 0 = not seen yet.
        RE::FormID g_firstReadyCell = 0;

        // The last blocker IsPlayerReady() reported, so it logs only on a change.
        Blocker g_lastBlocker = Blocker::kNone;
        bool    g_reported = false;
    }

    Blocker Check() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return Blocker::kNoPlayer;
        }
        if (!player->Is3DLoaded()) {
            return Blocker::kNo3D;
        }

        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            return Blocker::kNoUI;
        }
        if (ui->GameIsPaused()) {
            return Blocker::kPaused;
        }
        if (ui->IsMenuOpen("RaceSex Menu"sv)) {
            return Blocker::kCharGen;
        }
        if (ui->IsMenuOpen("Loading Menu"sv)) {
            return Blocker::kLoading;
        }

        auto* controls = RE::ControlMap::GetSingleton();
        if (!controls) {
            return Blocker::kNoControlMap;
        }
        if (!controls->IsMovementControlsEnabled()) {
            return Blocker::kMovementLocked;
        }

        return Blocker::kNone;
    }

    std::string_view Name(Blocker a_blocker) {
        switch (a_blocker) {
        case Blocker::kNone:
            return "ready";
        case Blocker::kNoPlayer:
            return "no player";
        case Blocker::kNo3D:
            return "3D not loaded";
        case Blocker::kNoUI:
            return "no UI";
        case Blocker::kPaused:
            return "game paused";
        case Blocker::kCharGen:
            return "character creation open";
        case Blocker::kLoading:
            return "loading screen";
        case Blocker::kNoControlMap:
            return "no control map";
        case Blocker::kMovementLocked:
            return "movement controls locked";
        }
        return "unknown";
    }

    bool IsPlayerReady() {
        const auto blocker = Check();
        if (!g_reported || blocker != g_lastBlocker) {
            g_reported = true;
            g_lastBlocker = blocker;
            if (blocker == Blocker::kNone) {
                logger::info("Readiness: ready");
            } else if (blocker == Blocker::kMovementLocked) {
                // The raw mask is what shows whether a VR-only control bit is the one
                // holding the awakening back.
                auto* controls = RE::ControlMap::GetSingleton();
                const std::uint32_t mask =
                    controls ? controls->GetRuntimeData().enabledControls.underlying() : 0u;
                logger::info("Readiness: not ready — {} (enabledControls={:#x})", Name(blocker), mask);
            } else {
                logger::info("Readiness: not ready — {}", Name(blocker));
            }
        }
        return blocker == Blocker::kNone;
    }

    bool InGameWorld() {
        // Character-creation start rooms (NYA, Alternate Start, ...) hand the player full
        // control while they are still picking a face — the System fired the moment the
        // room spawned. The tell: those rooms are interiors, and the real start arrives
        // via a teleport out of them. So: if the first cell the player is ready in is an
        // exterior, we are in the world right away (vanilla starts, coc); if it is an
        // interior, hold until the player reaches a DIFFERENT cell.
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* cell = player ? player->GetParentCell() : nullptr;
        if (!cell) {
            return false;
        }

        if (g_firstReadyCell == 0) {
            g_firstReadyCell = cell->GetFormID();
            if (cell->IsInteriorCell()) {
                logger::info(
                    "Reincarnation armed, holding: first ready in interior {:#x} "
                    "(likely a chargen room) — waiting for a cell change",
                    g_firstReadyCell);
                return false;
            }
        } else if (cell->GetFormID() == g_firstReadyCell && cell->IsInteriorCell()) {
            return false;  // still in the start room
        }

        return true;
    }

    void Reset() {
        g_firstReadyCell = 0;  // the next game gets a fresh chargen-room detection
        g_reported = false;    // ...and logs its first blocker again
        g_lastBlocker = Blocker::kNone;
    }
}
