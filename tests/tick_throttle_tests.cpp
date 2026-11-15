// Unit tests for src/TickThrottle.h: the once-a-period gate the main-thread frame tick
// uses to run the quest clock and the mod API publish. Run with tools/run-unit-tests.ps1.
//
// No framework on purpose, same as the other tests here: they must build in seconds
// from a bare MSVC toolchain.

#include "../src/TickThrottle.h"

#include <chrono>
#include <cstdio>
#include <iterator>

namespace {
    int g_failed = 0;
    int g_checks = 0;

    void Expect(bool a_ok, const char* a_what, int a_line) {
        ++g_checks;
        if (!a_ok) {
            ++g_failed;
            std::printf("  FAIL line %d: %s\n", a_line, a_what);
        }
    }
#define EXPECT(cond) Expect((cond), #cond, __LINE__)

    using namespace std::chrono_literals;
    using Isekai::TickThrottle;
    using time_point = std::chrono::steady_clock::time_point;

    // An arbitrary, non-zero start, so "never ran" cannot hide behind a zero epoch.
    const time_point t0 = time_point{} + 1000h;

    void FirstCallIsDue() {
        TickThrottle throttle{ 1s };
        EXPECT(throttle.Due(t0));
    }

    void AtMostOncePerPeriod() {
        TickThrottle throttle{ 1s };
        EXPECT(throttle.Due(t0));
        EXPECT(!throttle.Due(t0 + 999ms));
        EXPECT(throttle.Due(t0 + 1000ms));
        EXPECT(!throttle.Due(t0 + 1500ms));
    }

    void SameInstantTwiceIsNotDue() {
        TickThrottle throttle{ 1s };
        EXPECT(throttle.Due(t0));
        EXPECT(!throttle.Due(t0));
    }

    void BackwardsClockDoesNotWedge() {
        TickThrottle throttle{ 1s };
        EXPECT(throttle.Due(t0));
        EXPECT(!throttle.Due(t0 - 5s));
        EXPECT(throttle.Due(t0 + 1000ms));
    }

    void LongGapFiresOnceNotACatchUp() {
        // A save loaded after a long pause must not replay the missed seconds.
        TickThrottle throttle{ 1s };
        EXPECT(throttle.Due(t0));
        EXPECT(throttle.Due(t0 + 60s));
        EXPECT(!throttle.Due(t0 + 60s + 500ms));
        EXPECT(throttle.Due(t0 + 61s));
    }
}

int main() {
    struct Test {
        const char* name;
        void (*fn)();
    };
    const Test tests[] = {
        { "FirstCallIsDue", FirstCallIsDue },
        { "AtMostOncePerPeriod", AtMostOncePerPeriod },
        { "SameInstantTwiceIsNotDue", SameInstantTwiceIsNotDue },
        { "BackwardsClockDoesNotWedge", BackwardsClockDoesNotWedge },
        { "LongGapFiresOnceNotACatchUp", LongGapFiresOnceNotACatchUp },
    };

    int failedTests = 0;
    for (const auto& t : tests) {
        const int before = g_failed;
        t.fn();
        const bool ok = g_failed == before;
        failedTests += ok ? 0 : 1;
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", t.name);
    }
    std::printf("%d of %d tests passed (%d checks, %d failed)\n",
                static_cast<int>(std::size(tests)) - failedTests, static_cast<int>(std::size(tests)),
                g_checks, g_failed);
    return failedTests == 0 ? 0 : 1;
}
