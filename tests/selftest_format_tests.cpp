// Unit tests for src/SelfTestFormat.h: the [SELFTEST] lines test runners parse, and the
// storage-chest sink rule (#38). Run with tools/run-unit-tests.ps1.

#include "../src/SelfTestFormat.h"

#include <cstdio>
#include <regex>
#include <string>

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

    using namespace Isekai::SelfTest::Format;

    // The SotN test bridge's own patterns (server/src/sotn_bridge_mcp/checks.py), so a
    // line this mod writes is a line that runner reads.
    const std::regex kRunnerTest(R"(\[SELFTEST\]\s+(\S+)\s+(PASS|FAIL)\b\s*(.*?)\s*$)");
    const std::regex kRunnerEnd(R"(\[SELFTEST\]\s+ENDE\s*$)");

    void SlugIsOneWord() {
        EXPECT(Slug("ESP loaded") == "esp-loaded");
        EXPECT(Slug("storage: our own goods") == "storage-our-own-goods");
        EXPECT(Slug("  crafting count/consume hooks ") == "crafting-count-consume-hooks");
        EXPECT(Slug("storage-chest-at close") == "storage-chest-at-close");
        EXPECT(Slug("Mod API state") == "mod-api-state");
        EXPECT(Slug("") == "unnamed");
        EXPECT(Slug("—") == "unnamed");  // non-ASCII only
    }

    void PassLineParses() {
        const auto  line = Line("ESP loaded", true, "IsekaiHero.esp");
        std::smatch m;
        EXPECT(line == "[SELFTEST] isekai.esp-loaded PASS IsekaiHero.esp");
        EXPECT(std::regex_search(line, m, kRunnerTest));
        EXPECT(m[1] == "isekai.esp-loaded");
        EXPECT(m[2] == "PASS");
        EXPECT(m[3] == "IsekaiHero.esp");
    }

    void PassWithoutDetail() {
        EXPECT(Line("readiness", true, "") == "[SELFTEST] isekai.readiness PASS");
        EXPECT(Line("readiness", true, "  ") == "[SELFTEST] isekai.readiness PASS");
    }

    void FailAlwaysHasDetail() {
        const auto line = Line("hotkeys armed", false, "");
        EXPECT(line == "[SELFTEST] isekai.hotkeys-armed FAIL no detail given");
        std::smatch m;
        EXPECT(std::regex_search(line, m, kRunnerTest));
        EXPECT(m[2] == "FAIL");
    }

    void DetailStaysOnOneLine() {
        const auto line = Line("shop catalog", false, "a\nb\r\nc\td");
        EXPECT(line.find('\n') == std::string::npos);
        EXPECT(line.find('\r') == std::string::npos);
        EXPECT(line == "[SELFTEST] isekai.shop-catalog FAIL a b  c d");
    }

    void LoggerPrefixIsAllowed() {
        // spdlog puts a timestamp and level before every line.
        const std::string logged = "[16:23:33.306] [info] " + Line("readiness", true, "x");
        EXPECT(std::regex_search(logged, kRunnerTest));
        const std::string end = "[16:23:33.306] [info] " + std::string(kEnd);
        EXPECT(std::regex_search(end, kRunnerEnd));
        EXPECT(!std::regex_search(end, kRunnerTest));  // ENDE is not a test named ENDE
    }

    void ChestSinkRule() {
        // Sunk: exactly at, or within 100 units above, 3000 under the player.
        EXPECT(ChestIsSunk(true, 1000.0f - 3000.0f, 1000.0f));
        EXPECT(ChestIsSunk(true, 1000.0f - 2900.0f, 1000.0f));
        EXPECT(!ChestIsSunk(true, 1000.0f - 2899.0f, 1000.0f));
        // #38's report: the chest standing next to the player.
        EXPECT(!ChestIsSunk(true, 1000.0f, 1000.0f));
        EXPECT(AboveSinkTarget(1000.0f, 1000.0f) == 3000.0f);
        // Another cell: not standing in the player's world, whatever its height.
        EXPECT(ChestIsSunk(false, 1000.0f, 1000.0f));
    }

    struct Test {
        const char* name;
        void (*fn)();
    };
}

int main() {
    const Test tests[] = {
        { "SlugIsOneWord", SlugIsOneWord },
        { "PassLineParses", PassLineParses },
        { "PassWithoutDetail", PassWithoutDetail },
        { "FailAlwaysHasDetail", FailAlwaysHasDetail },
        { "DetailStaysOnOneLine", DetailStaysOnOneLine },
        { "LoggerPrefixIsAllowed", LoggerPrefixIsAllowed },
        { "ChestSinkRule", ChestSinkRule },
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
