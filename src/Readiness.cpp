#include "Readiness.h"

namespace Isekai::Readiness {

    namespace {
        // The cell the player was FIRST ready in, and the chargen-room hold built on it.
        WorldGate g_world;

        // The last blocker IsPlayerReady() reported, so it logs only on a change.
        ChangeReporter g_reporter;
    }

    Blocker Check() {
        // Filled in the order Evaluate() tests it: a missing singleton stops the reads
        // there, and Evaluate() never looks at the fields after it.
        Snapshot s;
        auto*    player = RE::PlayerCharacter::GetSingleton();
        s.hasPlayer = player != nullptr;
        if (s.hasPlayer) {
            s.player3D = player->Is3DLoaded();
            auto* ui = RE::UI::GetSingleton();
            s.hasUI = ui != nullptr;
            if (ui) {
                s.paused = ui->GameIsPaused();
                s.charGenOpen = ui->IsMenuOpen("RaceSex Menu"sv);
                s.loadingOpen = ui->IsMenuOpen("Loading Menu"sv);
            }
            auto* controls = RE::ControlMap::GetSingleton();
            s.hasControlMap = controls != nullptr;
            if (controls) {
                s.movementEnabled = controls->IsMovementControlsEnabled();
            }
        }
        return Evaluate(s);
    }

    bool IsPlayerReady() {
        const auto blocker = Check();
        if (g_reporter.ShouldReport(blocker)) {
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
        // See WorldGate: an interior first-ready cell is taken for a chargen start room
        // (NYA, Alternate Start, ...) and held until the player reaches a different cell.
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* cell = player ? player->GetParentCell() : nullptr;
        const auto step = cell ? g_world.Update(cell->GetFormID(), cell->IsInteriorCell())
                               : WorldGate::Step::kNoCell;
        if (step == WorldGate::Step::kHoldFirstInterior) {
            logger::info(
                "Reincarnation armed, holding: first ready in interior {:#x} "
                "(likely a chargen room) — waiting for a cell change",
                g_world.FirstCell());
        }
        return step == WorldGate::Step::kInWorld;
    }

    void Reset() {
        g_world.Reset();     // the next game gets a fresh chargen-room detection
        g_reporter.Reset();  // ...and logs its first blocker again
    }
}
