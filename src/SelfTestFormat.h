#pragma once

// The machine-readable half of the self-test, with no engine dependency so
// tests/selftest_format_tests.cpp can pin it down.
//
// Test runners (the SotN test bridge among them) read these lines from the log:
//
//   [SELFTEST] isekai.<test-id> PASS [detail]
//   [SELFTEST] isekai.<test-id> FAIL <detail>
//   [SELFTEST] ENDE
//
// A block always ends with the ENDE line, and a runner treats a test line after the
// last ENDE as an unfinished run. So every line written outside the full self-test
// (the storage chest on open and close, the hotkey rebind, the token return) is a
// block of its own: the test line, then ENDE.

#include <cctype>
#include <string>
#include <string_view>

namespace Isekai::SelfTest::Format {

    inline constexpr std::string_view kPrefix = "[SELFTEST] ";
    inline constexpr std::string_view kEnd = "[SELFTEST] ENDE";
    inline constexpr std::string_view kIdPrefix = "isekai.";

    // "storage: our own goods" -> "storage-our-own-goods". Lower case ASCII letters and
    // digits; every other run of characters becomes one '-', none at either end.
    [[nodiscard]] inline std::string Slug(std::string_view a_name) {
        std::string out;
        out.reserve(a_name.size());
        bool dash = false;
        for (const char c : a_name) {
            const auto u = static_cast<unsigned char>(c);
            if (u < 0x80 && std::isalnum(u)) {
                if (dash && !out.empty()) {
                    out += '-';
                }
                dash = false;
                out += static_cast<char>(std::tolower(u));
            } else {
                dash = true;
            }
        }
        return out.empty() ? std::string("unnamed") : out;
    }

    // One test line. a_id is slugged and prefixed; the detail is kept on one line.
    // A FAIL without a detail would tell nobody anything, so it gets one.
    [[nodiscard]] inline std::string Line(std::string_view a_id, bool a_pass,
                                          std::string_view a_detail) {
        std::string line(kPrefix);
        line += kIdPrefix;
        line += Slug(a_id);
        line += a_pass ? " PASS" : " FAIL";

        std::string detail;
        detail.reserve(a_detail.size());
        for (const char c : a_detail) {
            detail += (c == '\r' || c == '\n' || c == '\t') ? ' ' : c;
        }
        while (!detail.empty() && detail.back() == ' ') {
            detail.pop_back();
        }
        if (detail.empty() && !a_pass) {
            detail = "no detail given";
        }
        if (!detail.empty()) {
            line += ' ';
            line += detail;
        }
        return line;
    }

    // #38: Open() buries the storage chest 3000 units under the player. A chest in the
    // player's cell more than 100 units above that depth is standing in the world.
    inline constexpr float kSinkDepth = 3000.0f;
    inline constexpr float kSinkTolerance = 100.0f;

    [[nodiscard]] constexpr float AboveSinkTarget(float a_chestZ, float a_playerZ) noexcept {
        return a_chestZ - (a_playerZ - kSinkDepth);
    }

    [[nodiscard]] constexpr bool ChestIsSunk(bool a_inPlayerCell, float a_chestZ,
                                             float a_playerZ) noexcept {
        return !a_inPlayerCell || AboveSinkTarget(a_chestZ, a_playerZ) <= kSinkTolerance;
    }
}
