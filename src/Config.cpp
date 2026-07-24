#include "Config.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>

namespace Isekai::Config {

    namespace {
        // Relative to the game root (where SkyrimSE.exe runs), the standard SKSE layout.
        constexpr const char* kIniPath = "Data\\SKSE\\Plugins\\IsekaiHero.ini";

        bool          g_hideSealedNodes = false;
        std::uint32_t g_systemMenuKey = 0x1F;       // DIK_S
        std::uint32_t g_systemMenuModifier = 0x36;  // DIK_RSHIFT
        std::uint16_t g_dormantHeroLevel = 25;
        std::uint16_t g_dormantAscendedLevel = 80;

        [[nodiscard]] std::string Trim(std::string a_s) {
            const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
            a_s.erase(a_s.begin(), std::find_if(a_s.begin(), a_s.end(), notSpace));
            a_s.erase(std::find_if(a_s.rbegin(), a_s.rend(), notSpace).base(), a_s.end());
            return a_s;
        }

        [[nodiscard]] std::string Lower(std::string a_s) {
            std::transform(a_s.begin(), a_s.end(), a_s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return a_s;
        }

        [[nodiscard]] bool AsBool(const std::string& a_val) {
            const std::string v = Lower(a_val);
            return v == "1" || v == "true" || v == "yes" || v == "on";
        }

        // A DirectInput scan code, hex ("0x1F") or decimal ("31"). Base 0 lets the same
        // parse handle both. On garbage, keep whatever default was passed in.
        [[nodiscard]] std::uint32_t AsScanCode(const std::string& a_val, std::uint32_t a_def) {
            try {
                return static_cast<std::uint32_t>(std::stoul(a_val, nullptr, 0));
            } catch (...) {
                return a_def;
            }
        }

        // A character level, 1..1000. Garbage or an out-of-range number keeps the
        // default rather than producing a threshold that can never be reached.
        [[nodiscard]] std::uint16_t AsLevel(const std::string& a_val, std::uint16_t a_def) {
            try {
                const auto n = std::stoul(a_val, nullptr, 10);
                return (n >= 1 && n <= 1000) ? static_cast<std::uint16_t>(n) : a_def;
            } catch (...) {
                return a_def;
            }
        }
    }

    void Load() {
        // Defaults, overwritten only by an explicit key below.
        g_hideSealedNodes = false;
        g_systemMenuKey = 0x1F;       // DIK_S
        g_systemMenuModifier = 0x36;  // DIK_RSHIFT
        g_dormantHeroLevel = 25;
        g_dormantAscendedLevel = 80;

        std::ifstream in(kIniPath);
        if (!in) {
            logger::info("Config: no ini at {} — using defaults", kIniPath);
            return;
        }

        // Deliberately section-agnostic: a flat key=value scan is enough for our handful
        // of settings and keeps the parser trivial. Lines starting ';' or '#' (or an
        // inline trailer of one) are comments.
        std::string line;
        while (std::getline(in, line)) {
            if (const auto cut = line.find_first_of(";#"); cut != std::string::npos) {
                line.erase(cut);
            }
            const auto eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            const std::string key = Lower(Trim(line.substr(0, eq)));
            const std::string val = Trim(line.substr(eq + 1));
            if (key == "hidesealednodes") {
                g_hideSealedNodes = AsBool(val);
            } else if (key == "systemmenukey") {
                g_systemMenuKey = AsScanCode(val, g_systemMenuKey);
            } else if (key == "systemmenumodifier") {
                g_systemMenuModifier = AsScanCode(val, g_systemMenuModifier);
            } else if (key == "dormantherolevel") {
                g_dormantHeroLevel = AsLevel(val, g_dormantHeroLevel);
            } else if (key == "dormantascendedlevel") {
                g_dormantAscendedLevel = AsLevel(val, g_dormantAscendedLevel);
            }
        }

        // The ladder only makes sense upwards. Swapped thresholds would otherwise
        // hand out ASCENDED first and then never fire HERO at all.
        if (g_dormantAscendedLevel <= g_dormantHeroLevel) {
            logger::warn("Config: DormantAscendedLevel ({}) must be above DormantHeroLevel ({}) — "
                         "raising it to {}",
                         g_dormantAscendedLevel, g_dormantHeroLevel, g_dormantHeroLevel + 1);
            g_dormantAscendedLevel = static_cast<std::uint16_t>(g_dormantHeroLevel + 1);
        }

        logger::info("Config: HideSealedNodes={}, SystemMenuKey={:#x}, SystemMenuModifier={:#x}, "
                     "DormantHeroLevel={}, DormantAscendedLevel={}",
                     g_hideSealedNodes, g_systemMenuKey, g_systemMenuModifier, g_dormantHeroLevel,
                     g_dormantAscendedLevel);
    }

    bool HideSealedNodes() {
        return g_hideSealedNodes;
    }

    std::uint32_t SystemMenuKey() {
        return g_systemMenuKey;
    }

    std::uint32_t SystemMenuModifier() {
        return g_systemMenuModifier;
    }

    std::uint16_t DormantHeroLevel() {
        return g_dormantHeroLevel;
    }

    std::uint16_t DormantAscendedLevel() {
        return g_dormantAscendedLevel;
    }
}
