#pragma once

// The decisions behind Readiness and the automatic reincarnation trigger, with every
// engine read taken out. Readiness.cpp and System.cpp feed these from the live game;
// tests/readiness_tests.cpp feeds them by hand. Nothing here may include CommonLibSSE.

#include <cstdint>
#include <string_view>

namespace Isekai::Readiness {

    // Why the player is not ready, in the order the tests run. kNone = ready.
    enum class Blocker : std::uint8_t {
        kNone,
        kNoPlayer,
        kNo3D,
        kNoUI,
        kPaused,
        kCharGen,
        kLoading,
        kNoControlMap,
        kMovementLocked
    };

    // One look at the engine, as plain flags. A "has" flag that is false short-circuits
    // everything after it, exactly as a null singleton does in the live check.
    struct Snapshot {
        bool hasPlayer = true;
        bool player3D = true;
        bool hasUI = true;
        bool paused = false;
        bool charGenOpen = false;
        bool loadingOpen = false;
        bool hasControlMap = true;
        bool movementEnabled = true;
    };

    [[nodiscard]] constexpr Blocker Evaluate(const Snapshot& a_s) noexcept {
        if (!a_s.hasPlayer) {
            return Blocker::kNoPlayer;
        }
        if (!a_s.player3D) {
            return Blocker::kNo3D;
        }
        if (!a_s.hasUI) {
            return Blocker::kNoUI;
        }
        if (a_s.paused) {
            return Blocker::kPaused;
        }
        if (a_s.charGenOpen) {
            return Blocker::kCharGen;
        }
        if (a_s.loadingOpen) {
            return Blocker::kLoading;
        }
        if (!a_s.hasControlMap) {
            return Blocker::kNoControlMap;
        }
        if (!a_s.movementEnabled) {
            return Blocker::kMovementLocked;
        }
        return Blocker::kNone;
    }

    [[nodiscard]] constexpr std::string_view Name(Blocker a_blocker) noexcept {
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

    // Logs a blocker once each time it changes, so a player stuck on one condition
    // leaves exactly one line instead of one per menu close.
    class ChangeReporter {
    public:
        // True when a_blocker should be logged now.
        [[nodiscard]] constexpr bool ShouldReport(Blocker a_blocker) noexcept {
            if (m_reported && a_blocker == m_last) {
                return false;
            }
            m_reported = true;
            m_last = a_blocker;
            return true;
        }

        constexpr void Reset() noexcept {
            m_reported = false;
            m_last = Blocker::kNone;
        }

    private:
        Blocker m_last = Blocker::kNone;
        bool    m_reported = false;
    };

    // "Ready" alone is not "in the game world": character-creation start rooms (NYA,
    // Alternate Start, ...) hand the player full control while they are still picking a
    // face. Those rooms are interiors and the real start arrives via a teleport out of
    // them. So: if the first cell the player is ready in is an exterior, we are in the
    // world right away (vanilla starts, coc); if it is an interior, hold until the player
    // reaches a DIFFERENT cell.
    class WorldGate {
    public:
        enum class Step : std::uint8_t {
            kInWorld,
            kNoCell,
            kHoldFirstInterior,  // just armed in an interior: the caller logs this once
            kHoldStillThere,
        };

        [[nodiscard]] constexpr Step Update(std::uint32_t a_cell, bool a_interior) noexcept {
            if (a_cell == 0) {
                return Step::kNoCell;
            }
            if (m_firstCell == 0) {
                m_firstCell = a_cell;
                return a_interior ? Step::kHoldFirstInterior : Step::kInWorld;
            }
            if (a_cell == m_firstCell && a_interior) {
                return Step::kHoldStillThere;
            }
            return Step::kInWorld;
        }

        [[nodiscard]] constexpr std::uint32_t FirstCell() const noexcept { return m_firstCell; }
        constexpr void Reset() noexcept { m_firstCell = 0; }

    private:
        std::uint32_t m_firstCell = 0;
    };

    // What the automatic trigger (System's TryTrigger) does on one menu close.
    enum class TriggerStep : std::uint8_t {
        kAlreadyReincarnated,  // once per character: readiness is not even asked (#36)
        kNotReady,
        kHeldNoAutoStart,      // AutoStart = 0: the System hotkey starts it instead
        kHeldNotInWorld,
        kBegin,
    };

    // The order matters and is what the tests pin down. An already reincarnated
    // character returns before a_isReady runs, so a loaded save that is past its
    // reincarnation logs at most the one blocker seen during the pre-load revert and
    // never "Readiness: ready". a_inWorld runs only once the player is ready and
    // AutoStart is on, because its first call fixes the start cell.
    template <class ReadyFn, class InWorldFn>
    [[nodiscard]] TriggerStep DecideTrigger(bool a_reincarnated, ReadyFn&& a_isReady,
                                            bool a_autoStart, InWorldFn&& a_inWorld) {
        if (a_reincarnated) {
            return TriggerStep::kAlreadyReincarnated;
        }
        if (!a_isReady()) {
            return TriggerStep::kNotReady;
        }
        if (!a_autoStart) {
            return TriggerStep::kHeldNoAutoStart;
        }
        if (!a_inWorld()) {
            return TriggerStep::kHeldNotInWorld;
        }
        return TriggerStep::kBegin;
    }
}
