#pragma once

// When the System's voice may speak, with every engine read taken out. Voice.cpp feeds
// these from the live game; tests/voice_logic_tests.cpp feeds them by hand. Nothing here
// may include CommonLibSSE. Times are seconds on a steady clock.

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
}
