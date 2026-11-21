// Unit tests for src/VoiceLogic.h: when the System's voice may speak. Run with
// tools/run-unit-tests.ps1. No framework on purpose, same as the other tests here.

#include "../src/VoiceLogic.h"

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

    using namespace Isekai::Voice::Logic;

    void GapBlocksEqualPriority() {
        EXPECT(!ShouldSpeak(102.0, 100.0, 1, 1, kMinGapSeconds));
        EXPECT(!ShouldSpeak(103.9, 100.0, 2, 2, kMinGapSeconds));
    }

    void HigherPriorityPassesInsideGap() {
        EXPECT(ShouldSpeak(101.0, 100.0, 1, 3, kMinGapSeconds));
        EXPECT(!ShouldSpeak(101.0, 100.0, 3, 1, kMinGapSeconds));
    }

    void AfterGapAnyPriority() {
        EXPECT(ShouldSpeak(104.0, 100.0, 3, 1, kMinGapSeconds));
        EXPECT(ShouldSpeak(200.0, 100.0, 1, 1, kMinGapSeconds));
    }

    void RankRoseNeedsKnownPrevious() {
        EXPECT(RankRose(-1, 3) == false);
        EXPECT(RankRose(2, 3) == true);
        EXPECT(RankRose(3, 2) == false);
        EXPECT(RankRose(3, 3) == false);
    }

    void LethalCooldown() {
        EXPECT(!LethalShouldSpeak(100.0 + 179.9, 100.0, kLethalCooldownSeconds));
        EXPECT(LethalShouldSpeak(100.0 + 180.0, 100.0, kLethalCooldownSeconds));
    }

    void TopPriorityFollowsTopPriorityInsideGap() {
        // Reboot, then a blessing picked within 4 s: the blessing line must not be dropped.
        Gate g;
        EXPECT(g.Accept(100.0, kTopPriority));
        EXPECT(g.Accept(101.0, kTopPriority));
        EXPECT(!g.Accept(101.5, 2));
        EXPECT(!g.Accept(101.5, 1));
    }

    void GateKeepsTheGapBelowTop() {
        Gate g;
        EXPECT(g.Accept(100.0, 2));
        EXPECT(!g.Accept(101.0, 2));
        EXPECT(g.Accept(101.0, 3));
        EXPECT(g.Accept(105.0, 1));
    }

    void OnlyAStrictlyHigherLineInterrupts() {
        EXPECT(HandoffFor(false, 3, 1) == Handoff::kStart);
        EXPECT(HandoffFor(true, 2, 3) == Handoff::kInterrupt);
        EXPECT(HandoffFor(true, 3, 3) == Handoff::kWait);
        EXPECT(HandoffFor(true, 3, 1) == Handoff::kDrop);
    }

    void LethalCooldownSpentOnlyWhenAccepted() {
        Gate g;
        EXPECT(g.Accept(100.0, 1));             // some other line just spoke
        EXPECT(!g.AcceptLethal(101.0, 1));      // dropped by the gap...
        EXPECT(g.AcceptLethal(104.0, 1));       // ...so the cooldown was not spent
        EXPECT(!g.AcceptLethal(104.0 + 179.0, 1));
        EXPECT(g.AcceptLethal(104.0 + 180.0, 1));
    }

    void RankRiseOnlyFromAKnownRank() {
        Gate g;
        EXPECT(!g.RankAnnounce(3));  // first rank seen after a load: not a rise
        EXPECT(g.RankAnnounce(4));
        EXPECT(!g.RankAnnounce(4));
        EXPECT(!g.RankAnnounce(2));
        g.Reset();
        EXPECT(!g.RankAnnounce(5));  // loading a save with a higher rank says nothing
        EXPECT(!g.RankAnnounce(9));  // out of range: recorded, no line
        EXPECT(!g.RankAnnounce(5));
    }

    void ResetForgetsLethalAndGap() {
        Gate g;
        EXPECT(g.AcceptLethal(100.0, 1));
        g.Reset();
        EXPECT(g.AcceptLethal(101.0, 1));
    }
}

int main() {
    struct Test {
        const char* name;
        void (*fn)();
    };
    const Test tests[] = {
        { "GapBlocksEqualPriority", GapBlocksEqualPriority },
        { "HigherPriorityPassesInsideGap", HigherPriorityPassesInsideGap },
        { "AfterGapAnyPriority", AfterGapAnyPriority },
        { "RankRoseNeedsKnownPrevious", RankRoseNeedsKnownPrevious },
        { "LethalCooldown", LethalCooldown },
        { "TopPriorityFollowsTopPriorityInsideGap", TopPriorityFollowsTopPriorityInsideGap },
        { "GateKeepsTheGapBelowTop", GateKeepsTheGapBelowTop },
        { "OnlyAStrictlyHigherLineInterrupts", OnlyAStrictlyHigherLineInterrupts },
        { "LethalCooldownSpentOnlyWhenAccepted", LethalCooldownSpentOnlyWhenAccepted },
        { "RankRiseOnlyFromAKnownRank", RankRiseOnlyFromAKnownRank },
        { "ResetForgetsLethalAndGap", ResetForgetsLethalAndGap },
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
