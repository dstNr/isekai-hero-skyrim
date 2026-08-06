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
        // Off until the codex actually works: it is handed out, it carries the
        // container's name, and drinking it does not open anything. An unfinished
        // feature should not be leaving items in people's inventories.
        bool          g_storageCodex = false;
        bool          g_skyrimNetIntegration = true;
        bool          g_threatLabels = true;
        ThreatTargets g_threatTargets = ThreatTargets::kAggro;
        std::uint32_t g_threatRange = 4000;
        std::uint32_t g_threatKey = 0x44;  // DIK_F10
        std::uint32_t g_questFirstTaskHours = 12;
        std::uint32_t g_questIntervalHours = 24;
        bool          g_questRerollButton = false;
        // 0 = off. Off by default: the self-test is a diagnostic for bug reports, not a
        // feature, and a stray key that pops a notification would just be noise.
        std::uint32_t g_selfTestKey = 0;
        bool          g_logInputDiag = false;

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

        // Key names, so a hotkey can be written as "F11" instead of "0x57".
        //
        // This exists because the numeric-only version had a trap in it. Every other
        // setting in this file is a 0/1 switch, and "SelfTestKey = 0" looks exactly like
        // one — so turning the self-test on by writing "1" is the natural move. Scan code
        // 1 is ESCAPE. The result was a self-test that fired on ESC and a key that "did
        // nothing", with nothing anywhere saying why.
        struct KeyEntry {
            const char*   name;
            std::uint32_t code;
        };
        constexpr KeyEntry kKeyNames[] = {
            { "esc", 0x01 },      { "escape", 0x01 },   { "tab", 0x0F },
            { "space", 0x39 },    { "enter", 0x1C },    { "return", 0x1C },
            { "backspace", 0x0E }, { "capslock", 0x3A }, { "grave", 0x29 },
            { "tilde", 0x29 },    { "backslash", 0x2B },
            { "lshift", 0x2A },   { "rshift", 0x36 },   { "lctrl", 0x1D },
            { "rctrl", 0x9D },    { "lalt", 0x38 },     { "ralt", 0xB8 },
            { "f1", 0x3B },  { "f2", 0x3C },  { "f3", 0x3D },  { "f4", 0x3E },
            { "f5", 0x3F },  { "f6", 0x40 },  { "f7", 0x41 },  { "f8", 0x42 },
            { "f9", 0x43 },  { "f10", 0x44 }, { "f11", 0x57 }, { "f12", 0x58 },
            { "a", 0x1E }, { "b", 0x30 }, { "c", 0x2E }, { "d", 0x20 }, { "e", 0x12 },
            { "f", 0x21 }, { "g", 0x22 }, { "h", 0x23 }, { "i", 0x17 }, { "j", 0x24 },
            { "k", 0x25 }, { "l", 0x26 }, { "m", 0x32 }, { "n", 0x31 }, { "o", 0x18 },
            { "p", 0x19 }, { "q", 0x10 }, { "r", 0x13 }, { "s", 0x1F }, { "t", 0x14 },
            { "u", 0x16 }, { "v", 0x2F }, { "w", 0x11 }, { "x", 0x2D }, { "y", 0x15 },
            { "z", 0x2C },
        };

        // A DirectInput scan code: a name ("F11"), hex ("0x1F") or decimal ("31"). Base 0
        // lets the same parse handle the last two. On garbage, keep the default passed in.
        [[nodiscard]] std::uint32_t AsScanCode(const std::string& a_val, std::uint32_t a_def) {
            const std::string v = Lower(Trim(a_val));
            for (const auto& e : kKeyNames) {
                if (v == e.name) {
                    return e.code;
                }
            }
            try {
                return static_cast<std::uint32_t>(std::stoul(a_val, nullptr, 0));
            } catch (...) {
                return a_def;
            }
        }

        // A wait in game hours, 0..720. 0 is legal and means "no wait" — useful for
        // testing objectives without sleeping through a night for each one. The upper
        // clamp is a month, past which the feature would look broken rather than slow.
        [[nodiscard]] std::uint32_t AsHours(const std::string& a_val, std::uint32_t a_def) {
            try {
                const auto n = std::stoul(a_val, nullptr, 10);
                return n <= 720 ? static_cast<std::uint32_t>(n) : a_def;
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
        g_storageCodex = false;
        g_skyrimNetIntegration = true;
        g_threatLabels = true;
        g_threatTargets = ThreatTargets::kAggro;
        g_threatRange = 4000;
        g_threatKey = 0x44;
        g_questFirstTaskHours = 12;
        g_questIntervalHours = 24;
        g_questRerollButton = false;
        g_selfTestKey = 0;
        g_logInputDiag = false;

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
            } else if (key == "storagecodex") {
                g_storageCodex = AsBool(val);
            } else if (key == "skyrimnetintegration") {
                g_skyrimNetIntegration = AsBool(val);
            } else if (key == "threatlabels") {
                g_threatLabels = AsBool(val);
            } else if (key == "threatlabeltargets") {
                const std::string v = Lower(val);
                g_threatTargets = v == "all"         ? ThreatTargets::kAll
                                  : v == "crosshair" ? ThreatTargets::kCrosshair
                                  : v == "hostile"   ? ThreatTargets::kHostile
                                                     : ThreatTargets::kAggro;
            } else if (key == "threatlabelkey") {
                g_threatKey = AsScanCode(val, g_threatKey);
            } else if (key == "threatlabelrange") {
                // Clamped rather than trusted: 0 would switch the feature off through the
                // back door, and a huge value would label things across a whole hold.
                try {
                    g_threatRange = std::clamp(
                        static_cast<std::uint32_t>(std::stoul(val, nullptr, 0)), 500u, 20000u);
                } catch (...) {
                }
            } else if (key == "firsttaskhours") {
                g_questFirstTaskHours = AsHours(val, g_questFirstTaskHours);
            } else if (key == "taskintervalhours") {
                g_questIntervalHours = AsHours(val, g_questIntervalHours);
            } else if (key == "questrerollbutton") {
                g_questRerollButton = AsBool(val);
            } else if (key == "selftestkey") {
                g_selfTestKey = AsScanCode(val, g_selfTestKey);
            } else if (key == "loginputdiagnostics") {
                g_logInputDiag = AsBool(val);
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

        logger::info("Config: HideSealedNodes={}, SystemMenuKey={}, SystemMenuModifier={}, "
                     "DormantHeroLevel={}, DormantAscendedLevel={}, StorageCodex={}, "
                     "SkyrimNetIntegration={}, ThreatLabels={} (targets={}, range={}, key={}), "
                     "FirstTaskHours={}, TaskIntervalHours={}, QuestRerollButton={}, "
                     "SelfTestKey={}, LogInputDiagnostics={}",
                     g_hideSealedNodes, KeyName(g_systemMenuKey), KeyName(g_systemMenuModifier),
                     g_dormantHeroLevel, g_dormantAscendedLevel, g_storageCodex,
                     g_skyrimNetIntegration, g_threatLabels,
                     g_threatTargets == ThreatTargets::kAll         ? "all"
                     : g_threatTargets == ThreatTargets::kCrosshair ? "crosshair"
                     : g_threatTargets == ThreatTargets::kHostile   ? "hostile"
                                                                    : "aggro",
                     g_threatRange, KeyName(g_threatKey), g_questFirstTaskHours,
                     g_questIntervalHours, g_questRerollButton, KeyName(g_selfTestKey),
                     g_logInputDiag);
    }

    std::string KeyName(std::uint32_t a_scanCode) {
        const auto hex = std::format("{:#04x}", a_scanCode);
        if (a_scanCode == 0) {
            return hex + " (none)";
        }
        for (const auto& e : kKeyNames) {
            if (e.code == a_scanCode) {
                std::string name = e.name;
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                return hex + " (" + name + ")";
            }
        }
        return hex;
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

    bool StorageCodex() {
        return g_storageCodex;
    }

    bool SkyrimNetIntegration() {
        return g_skyrimNetIntegration;
    }

    bool ThreatLabels() {
        return g_threatLabels;
    }

    ThreatTargets ThreatLabelTargets() {
        return g_threatTargets;
    }

    std::uint32_t ThreatLabelKey() {
        return g_threatKey;
    }

    std::uint32_t ThreatLabelRange() {
        return g_threatRange;
    }

    std::uint32_t QuestFirstTaskHours() {
        return g_questFirstTaskHours;
    }

    std::uint32_t QuestIntervalHours() {
        return g_questIntervalHours;
    }

    bool QuestRerollButton() {
        return g_questRerollButton;
    }

    std::uint32_t SelfTestKey() {
        return g_selfTestKey;
    }

    bool LogInputDiagnostics() {
        return g_logInputDiag;
    }
}
