#pragma once

// A once-a-period gate for work driven from a per-frame hook. FrameTick feeds it the
// steady clock every frame; tests/tick_throttle_tests.cpp feeds it by hand. Nothing
// here may include CommonLibSSE.

#include <chrono>
#include <optional>

namespace Isekai {

    class TickThrottle {
    public:
        explicit TickThrottle(std::chrono::milliseconds a_period) : _period(a_period) {}

        // True on the first call, then at most once per period. A due call restarts the
        // period from `now`, so a long gap fires once rather than catching up. A clock
        // that reads earlier than the last due call is not due and leaves the gate as it
        // was, so it can never wedge waiting for a period measured from a bad reading.
        [[nodiscard]] bool Due(std::chrono::steady_clock::time_point a_now) {
            if (_last && (a_now < *_last || a_now - *_last < _period)) {
                return false;
            }
            _last = a_now;
            return true;
        }

    private:
        std::chrono::milliseconds                             _period;
        std::optional<std::chrono::steady_clock::time_point> _last;
    };
}
