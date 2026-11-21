#pragma once

// When the System's voice may speak, with every engine read taken out. Voice.cpp feeds
// these from the live game; tests/voice_logic_tests.cpp feeds them by hand. Nothing here
// may include CommonLibSSE. Times are seconds on a steady clock.

#include <limits>

namespace Isekai::Voice::Logic {

    // Least time between two lines of the same or lower priority.
    inline constexpr double kMinGapSeconds = 4.0;
    // The "lethal threat" warning speaks at most once in this window.
    inline constexpr double kLethalCooldownSeconds = 180.0;

    // A line speaks once the gap has passed, or at once when it outranks the last one.
    [[nodiscard]] constexpr bool ShouldSpeak(double a_now, double a_lastAt, int a_lastPriority,
                                             int a_priority, double a_minGap) {
        return a_now - a_lastAt >= a_minGap || a_priority > a_lastPriority;
    }

    // prev < 0 means "unknown" (just loaded or a new game): the first rank seen is not a rise.
    [[nodiscard]] constexpr bool RankRose(int a_prev, int a_now) {
        return a_prev >= 0 && a_now > a_prev;
    }

    [[nodiscard]] constexpr bool LethalShouldSpeak(double a_now, double a_lastLethalAt,
                                                   double a_cooldown) {
        return a_now - a_lastLethalAt >= a_cooldown;
    }

    // The ranks with a line of their own (RankD .. RankS).
    inline constexpr int kRankD = 1;
    inline constexpr int kRankS = 5;

    // Reincarnation, blessings, reboot, lost cosave: one-off lines heard in sequence.
    inline constexpr int kTopPriority = 3;

    // What a line that is due does about the one already on the speakers.
    enum class Handoff {
        kStart,      // nothing running
        kInterrupt,  // stop the running line: this one matters more
        kWait,       // same weight: let the running line finish, then speak
        kDrop,       // the running line matters more
    };

    [[nodiscard]] constexpr Handoff HandoffFor(bool a_playing, int a_runningPriority,
                                               int a_priority) {
        if (!a_playing) {
            return Handoff::kStart;
        }
        return a_priority > a_runningPriority    ? Handoff::kInterrupt
               : a_priority == a_runningPriority ? Handoff::kWait
                                                 : Handoff::kDrop;
    }

    // The voice's memory between lines, reset on load and new game.
    struct Gate {
        double lastAt = -std::numeric_limits<double>::infinity();
        int lastPriority = 0;
        double lastLethalAt = -std::numeric_limits<double>::infinity();
        int lastRank = -1;  // unknown

        // May a line of a_priority speak now? Records it if so. Top-priority lines pass
        // each other inside the gap (they queue at the speakers instead, see kWait).
        bool Accept(double a_now, int a_priority) {
            const bool topAfterTop = a_priority == kTopPriority && lastPriority == kTopPriority;
            if (!topAfterTop && !ShouldSpeak(a_now, lastAt, lastPriority, a_priority, kMinGapSeconds)) {
                return false;
            }
            lastAt = a_now;
            lastPriority = a_priority;
            return true;
        }

        // The lethal warning: its cooldown starts only when the line is actually accepted,
        // so a warning the gap swallowed does not silence the next one for 180 s.
        bool AcceptLethal(double a_now, int a_priority) {
            if (!LethalShouldSpeak(a_now, lastLethalAt, kLethalCooldownSeconds) ||
                !Accept(a_now, a_priority)) {
                return false;
            }
            lastLethalAt = a_now;
            return true;
        }

        // Records the rank (IsekaiHeroAPI::Rank: E = 0, D = 1 .. S = 5); true when it rose
        // from a known one to a rank that has a line. Anything else is recorded, not spoken.
        bool RankAnnounce(int a_rank) {
            const bool rose = RankRose(lastRank, a_rank);
            lastRank = a_rank;
            return rose && a_rank >= kRankD && a_rank <= kRankS;
        }

        void Reset() { *this = Gate{}; }
    };
}
