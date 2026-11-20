// Unit tests for src/EnchantLogic.h: the engine-free rules of the System enchantments
// (chain target pick, reflect cap, phoenix rescue gate, strongest magnitude, co-save bound),
// without Skyrim. Run with tools/run-unit-tests.ps1.

#include "../src/EnchantLogic.h"

#include <cstdio>
#include <iterator>
#include <limits>
#include <vector>

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

    using namespace Isekai::Enchant::Logic;

    void Chain() {
        std::vector<Candidate> c = {
            {300.f, true,  true,  false, true },   // the first target: never again
            {500.f, true,  true,  false, false},   // ok, 2nd nearest
            {200.f, true,  true,  false, false},   // ok, nearest
            {100.f, false, true,  false, false},   // follower: not hostile to the attacker
            {150.f, true,  false, false, false},   // dead
            {50.f,  true,  true,  true,  false},   // the attacker himself
            {900.f, true,  true,  false, false},   // out of range
            {600.f, true,  true,  false, false},   // ok, 3rd
            {650.f, true,  true,  false, false},   // ok but over the jump limit
        };
        const auto picked = PickChainTargets(c, 3, 800.f);
        EXPECT((picked == std::vector<std::size_t>{2, 1, 7}));
        EXPECT(PickChainTargets(c, 0, 800.f).empty());
        EXPECT(PickChainTargets({}, 3, 800.f).empty());
    }
    void Reflect() {
        EXPECT(ReflectFraction(30.f) == 0.30f);
        EXPECT(ReflectFraction(250.f) == 0.90f);   // helper clamp only (plugin fallback, not wired); the shipped cap is the ESP base 13
        EXPECT(ReflectFraction(-5.f) == 0.0f);
    }
    void Phoenix() {
        EXPECT(PhoenixShouldFire(0.20f, 100.0, 50.0));
        EXPECT(!PhoenixShouldFire(0.30f, 100.0, 50.0));   // not low enough
        EXPECT(!PhoenixShouldFire(0.20f, 40.0, 50.0));    // still cooling down
        EXPECT(!PhoenixShouldFire(0.0f, 100.0, 50.0));    // already dead
    }
    void Strongest() {
        const float two[] = {40.f, 55.f};
        EXPECT(StrongestMagnitude(two) == 55.f);          // two items: one effect, the stronger
        EXPECT(!StrongestMagnitude({}).has_value());
    }
    void CooldownLiveTest() {
        EXPECT(CooldownLive(100.0, 99.0));
        EXPECT(!CooldownLive(100.0, 100.0));
        EXPECT(!CooldownLive(0.0, 5.0));
    }
    void ReadyAtClamp() {
        EXPECT(ClampReadyAt(130.0, 100.0) == 130.0);                                       // a running cooldown stays
        EXPECT(ClampReadyAt(std::numeric_limits<double>::infinity(), 100.0) == 160.0);     // corrupt: one cooldown at most
        EXPECT(!CooldownLive(ClampReadyAt(std::numeric_limits<double>::quiet_NaN(), 100.0), 100.0));
    }
    void CooldownBound() {
        EXPECT(CooldownCountAcceptable(0));
        EXPECT(CooldownCountAcceptable(4096));
        EXPECT(!CooldownCountAcceptable(4097));
    }
}

int main() {
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "Chain", Chain },
        { "Reflect", Reflect },
        { "Phoenix", Phoenix },
        { "Strongest", Strongest },
        { "CooldownBound", CooldownBound },
        { "CooldownLive", CooldownLiveTest },
        { "ReadyAtClamp", ReadyAtClamp },
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
