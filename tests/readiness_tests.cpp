// Unit tests for src/ReadinessLogic.h: the readiness gate and the automatic
// reincarnation trigger, without Skyrim. Run with tools/run-unit-tests.ps1.
//
// No framework on purpose: the plugin build pulls everything through vcpkg and
// CommonLibSSE, and these tests must build in seconds from a bare MSVC toolchain.

#include "../src/ReadinessLogic.h"

#include <cstdio>
#include <string_view>

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

    using namespace Isekai::Readiness;

    // --- Evaluate: each blocker, and the order they are tested in ---------------

    void EvaluateEachBlocker() {
        EXPECT(Evaluate(Snapshot{}) == Blocker::kNone);

        Snapshot s;
        s.hasPlayer = false;
        EXPECT(Evaluate(s) == Blocker::kNoPlayer);
        s = {};
        s.player3D = false;
        EXPECT(Evaluate(s) == Blocker::kNo3D);
        s = {};
        s.hasUI = false;
        EXPECT(Evaluate(s) == Blocker::kNoUI);
        s = {};
        s.paused = true;
        EXPECT(Evaluate(s) == Blocker::kPaused);
        s = {};
        s.charGenOpen = true;
        EXPECT(Evaluate(s) == Blocker::kCharGen);
        s = {};
        s.loadingOpen = true;
        EXPECT(Evaluate(s) == Blocker::kLoading);
        s = {};
        s.hasControlMap = false;
        EXPECT(Evaluate(s) == Blocker::kNoControlMap);
        s = {};
        s.movementEnabled = false;
        EXPECT(Evaluate(s) == Blocker::kMovementLocked);
    }

    void EvaluateFirstBlockerWins() {
        // Run 24, start 1: the revert before a load sees a player without 3D while the
        // loading screen is up. The log must name the 3D, the earlier test.
        Snapshot s;
        s.player3D = false;
        s.loadingOpen = true;
        s.movementEnabled = false;
        EXPECT(Evaluate(s) == Blocker::kNo3D);

        // A missing player hides everything after it.
        s = {};
        s.hasPlayer = false;
        s.paused = true;
        EXPECT(Evaluate(s) == Blocker::kNoPlayer);

        s = {};
        s.paused = true;
        s.charGenOpen = true;
        EXPECT(Evaluate(s) == Blocker::kPaused);
    }

    void NamesMatchTheLog() {
        // These strings are what log checks and bug reports grep for.
        EXPECT(Name(Blocker::kNone) == "ready");
        EXPECT(Name(Blocker::kNo3D) == "3D not loaded");
        EXPECT(Name(Blocker::kMovementLocked) == "movement controls locked");
        EXPECT(Name(Blocker::kCharGen) == "character creation open");
    }

    // --- ChangeReporter: one line per change -----------------------------------

    void ReporterLogsOnlyChanges() {
        ChangeReporter r;
        EXPECT(r.ShouldReport(Blocker::kNo3D));            // first ever: logged
        EXPECT(!r.ShouldReport(Blocker::kNo3D));           // same again: quiet
        EXPECT(r.ShouldReport(Blocker::kMovementLocked));  // changed: logged
        EXPECT(r.ShouldReport(Blocker::kNone));            // ready: logged once
        EXPECT(!r.ShouldReport(Blocker::kNone));
    }

    void ReporterFirstReadyIsLoggedToo() {
        // kNone is also the initial "last" value; the first report must still log.
        ChangeReporter r;
        EXPECT(r.ShouldReport(Blocker::kNone));
        EXPECT(!r.ShouldReport(Blocker::kNone));
    }

    void ReporterResetLogsAgain() {
        ChangeReporter r;
        EXPECT(r.ShouldReport(Blocker::kNo3D));
        r.Reset();  // new game / pre-load revert
        EXPECT(r.ShouldReport(Blocker::kNo3D));
    }

    // --- WorldGate: the chargen start room ---------------------------------------

    void WorldGateExteriorStartIsInWorld() {
        WorldGate g;
        EXPECT(g.Update(0x1A26F, false) == WorldGate::Step::kInWorld);  // coc to an exterior
        EXPECT(g.FirstCell() == 0x1A26F);
    }

    void WorldGateHoldsInStartRoom() {
        WorldGate g;
        EXPECT(g.Update(0x100, true) == WorldGate::Step::kHoldFirstInterior);
        EXPECT(g.Update(0x100, true) == WorldGate::Step::kHoldStillThere);
        EXPECT(g.Update(0x200, true) == WorldGate::Step::kInWorld);   // another interior
        EXPECT(g.Update(0x100, true) == WorldGate::Step::kHoldStillThere);
        EXPECT(g.Update(0x300, false) == WorldGate::Step::kInWorld);  // outside
    }

    void WorldGateNoCell() {
        WorldGate g;
        EXPECT(g.Update(0, false) == WorldGate::Step::kNoCell);
        EXPECT(g.FirstCell() == 0);  // a missing cell must not arm the gate
        EXPECT(g.Update(0x100, true) == WorldGate::Step::kHoldFirstInterior);
        g.Reset();
        EXPECT(g.Update(0x100, false) == WorldGate::Step::kInWorld);
    }

    // --- DecideTrigger: the gates and their order ---------------------------------

    struct Calls {
        int ready = 0;
        int world = 0;
    };

    TriggerStep Decide(Calls& c, bool reincarnated, bool ready, bool autoStart, bool inWorld) {
        return DecideTrigger(
            reincarnated, [&] { ++c.ready; return ready; }, autoStart,
            [&] { ++c.world; return inWorld; });
    }

    void ReincarnatedNeverAsksReadiness() {
        // #36 / run 24 LG7: a loaded save that is already reincarnated must not poll
        // readiness at all, so its log never gets a "Readiness: ready" line.
        for (const bool ready : { false, true }) {
            for (const bool autoStart : { false, true }) {
                Calls c;
                EXPECT(Decide(c, true, ready, autoStart, true) == TriggerStep::kAlreadyReincarnated);
                EXPECT(c.ready == 0);
                EXPECT(c.world == 0);
            }
        }
    }

    void NotReadyStopsBeforeWorldGate() {
        Calls c;
        EXPECT(Decide(c, false, false, true, true) == TriggerStep::kNotReady);
        EXPECT(c.ready == 1);
        EXPECT(c.world == 0);  // the start cell must not be fixed while not ready
    }

    void AutoStartOffHoldsWithoutWorldGate() {
        Calls c;
        EXPECT(Decide(c, false, true, false, true) == TriggerStep::kHeldNoAutoStart);
        EXPECT(c.ready == 1);
        EXPECT(c.world == 0);
    }

    void NotInWorldHolds() {
        Calls c;
        EXPECT(Decide(c, false, true, true, false) == TriggerStep::kHeldNotInWorld);
        EXPECT(c.ready == 1);
        EXPECT(c.world == 1);
    }

    void AllGatesOpenBegins() {
        Calls c;
        EXPECT(Decide(c, false, true, true, true) == TriggerStep::kBegin);
        EXPECT(c.ready == 1);
        EXPECT(c.world == 1);
    }

    void LoadOfReincarnatedSaveSequence() {
        // The whole run-24 start-1 sequence: the revert resets state to "not
        // reincarnated", one menu close polls (no 3D yet), then the save's state loads
        // with reincarnated = true and every later menu close stops at the first gate.
        ChangeReporter reporter;
        int            logged = 0;
        bool           loggedReady = false;
        bool           reincarnated = false;

        auto close = [&](Blocker live) {
            return DecideTrigger(
                reincarnated,
                [&] {
                    if (reporter.ShouldReport(live)) {
                        ++logged;
                        loggedReady = loggedReady || live == Blocker::kNone;
                    }
                    return live == Blocker::kNone;
                },
                true, [] { return true; });
        };

        reporter.Reset();
        EXPECT(close(Blocker::kNo3D) == TriggerStep::kNotReady);
        reincarnated = true;  // "State loaded (reincarnated=true, ...)"
        for (int i = 0; i < 10; ++i) {
            EXPECT(close(Blocker::kNone) == TriggerStep::kAlreadyReincarnated);
        }
        EXPECT(logged == 1);
        EXPECT(!loggedReady);
    }

    void NewCharacterSequence() {
        // Run 24, start 5 (coc from the main menu, new character): not ready, then
        // ready, and the trigger begins in the exterior.
        ChangeReporter reporter;
        WorldGate      gate;
        int            logged = 0;

        auto close = [&](Blocker live) {
            return DecideTrigger(
                false,
                [&] {
                    if (reporter.ShouldReport(live)) {
                        ++logged;
                    }
                    return live == Blocker::kNone;
                },
                true, [&] { return gate.Update(0x1A26F, false) == WorldGate::Step::kInWorld; });
        };

        EXPECT(close(Blocker::kNo3D) == TriggerStep::kNotReady);
        EXPECT(close(Blocker::kNo3D) == TriggerStep::kNotReady);
        EXPECT(close(Blocker::kNone) == TriggerStep::kBegin);
        EXPECT(logged == 2);  // "not ready — 3D not loaded", then "ready"
    }

    struct Test {
        const char* name;
        void (*fn)();
    };
}

int main() {
    const Test tests[] = {
        { "EvaluateEachBlocker", EvaluateEachBlocker },
        { "EvaluateFirstBlockerWins", EvaluateFirstBlockerWins },
        { "NamesMatchTheLog", NamesMatchTheLog },
        { "ReporterLogsOnlyChanges", ReporterLogsOnlyChanges },
        { "ReporterFirstReadyIsLoggedToo", ReporterFirstReadyIsLoggedToo },
        { "ReporterResetLogsAgain", ReporterResetLogsAgain },
        { "WorldGateExteriorStartIsInWorld", WorldGateExteriorStartIsInWorld },
        { "WorldGateHoldsInStartRoom", WorldGateHoldsInStartRoom },
        { "WorldGateNoCell", WorldGateNoCell },
        { "ReincarnatedNeverAsksReadiness", ReincarnatedNeverAsksReadiness },
        { "NotReadyStopsBeforeWorldGate", NotReadyStopsBeforeWorldGate },
        { "AutoStartOffHoldsWithoutWorldGate", AutoStartOffHoldsWithoutWorldGate },
        { "NotInWorldHolds", NotInWorldHolds },
        { "AllGatesOpenBegins", AllGatesOpenBegins },
        { "LoadOfReincarnatedSaveSequence", LoadOfReincarnatedSaveSequence },
        { "NewCharacterSequence", NewCharacterSequence },
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
