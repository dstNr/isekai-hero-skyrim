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

        // Where MCM Helper puts what the player chose, if the optional MCM component is
        // installed at all. Absent is the normal case, not an error.
        constexpr const char* kMcmIniPath = "Data\\MCM\\Settings\\IsekaiHero.ini";

        std::string   g_language = "en";
        bool          g_hideSealedNodes = false;
        bool          g_autoStart = true;
        float         g_textSpeed = 1.0f;
        float         g_uiScale = 1.0f;
        std::uint32_t g_systemMenuKey = 0x1F;       // DIK_S
        std::uint32_t g_systemMenuModifier = 0x36;  // DIK_RSHIFT
        std::uint32_t g_vrMenuButton = 0;           // none; VR codes differ per headset
        std::uint32_t g_padMenuButton = 0x0020;     // XINPUT_GAMEPAD_BACK
        std::uint32_t g_padMenuModifier = 0x0100;   // XINPUT_GAMEPAD_LEFT_SHOULDER
        std::uint16_t g_dormantHeroLevel = 25;
        std::uint16_t g_dormantAscendedLevel = 80;
        // Off until the codex actually works: it is handed out, it carries the
        // container's name, and drinking it does not open anything. An unfinished
        // feature should not be leaving items in people's inventories.
        bool          g_storageCodex = false;
        bool          g_skyrimNetIntegration = true;
        bool          g_threatLabels = true;
        ThreatTargets g_threatTargets = ThreatTargets::kAggro;
        bool          g_threatNumbers = true;
        // On: the two pools share one thin strip under the health bar, so they cost the
        // frame five pixels rather than the two extra rows that kept this off before.
        bool          g_threatResources = true;
        std::uint32_t g_threatRange = 4000;
        // Percent of screen height. Was a hardcoded 8 and measured against the head alone,
        // which is why only aiming at an enemy's face produced a reading.
        std::uint32_t g_threatAimRadius = 15;
        float         g_vrThreatHeight = 0.15f;
        std::uint32_t g_threatKey = 0x44;  // DIK_F10
        std::uint32_t g_questFirstTaskHours = 12;
        std::uint32_t g_questIntervalHours = 24;
        float         g_questRewardScale = 1.0f;
        std::uint32_t g_killsPerBounty = 50;
        std::int32_t  g_killBountyPoints = 1;
        std::uint32_t g_professionActionsPerPoint = 25;
        // Read once at reincarnation and then fixed for that character — see
        // State::nodeCostScale. Live here only so a new character picks up the ini.
        float         g_nodeCostScale = 1.0f;
        // Live: node effects are re-derived on every load, so changing this mid-save
        // recomputes cleanly instead of stacking.
        float         g_nodeEffectScale = 1.0f;
        std::uint32_t g_heroSkillLevel = 50;
        std::uint32_t g_heroAttributeTarget = 180;
        std::int32_t  g_heroSystemPoints = 5;
        std::uint32_t g_ascendedSkillLevel = 100;
        std::uint32_t g_ascendedAttributeTarget = 600;
        std::int32_t  g_ascendedSystemPoints = 10;
        bool          g_questRerollButton = false;
        // 0 = off. Off by default: the self-test is a diagnostic for bug reports, not a
        // feature, and a stray key that pops a notification would just be noise.
        std::uint32_t g_selfTestKey = 0;
        // 0 = off, for the same reason: handing out points is a testing aid, and a stray
        // key that quietly makes the game easier is worse than one that does nothing.
        std::uint32_t g_debugPointsKey = 0;
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

        // XInput button masks, by the names printed on a controller. "LB"/"RB" and
        // "L3"/"R3" are in there next to the long forms because that is what the buttons
        // are actually called out loud.
        constexpr KeyEntry kPadNames[] = {
            { "a", 0x1000 },      { "b", 0x2000 },      { "x", 0x4000 },
            { "y", 0x8000 },      { "lb", 0x0100 },     { "rb", 0x0200 },
            { "leftshoulder", 0x0100 },                 { "rightshoulder", 0x0200 },
            { "back", 0x0020 },   { "select", 0x0020 }, { "start", 0x0010 },
            { "l3", 0x0040 },     { "r3", 0x0080 },     { "leftthumb", 0x0040 },
            { "rightthumb", 0x0080 },
            { "dpadup", 0x0001 }, { "dpaddown", 0x0002 },
            { "dpadleft", 0x0004 }, { "dpadright", 0x0008 },
            { "none", 0 },
        };

        // An XInput button: a name ("Back", "LB") or a raw mask ("0x0020"). Unlike a scan
        // code, 0 is a legal answer here — it is how the ini spells "no controller
        // binding" — so garbage keeps the default but an explicit 0 does not.
        [[nodiscard]] std::uint32_t AsPadButton(const std::string& a_val, std::uint32_t a_def) {
            const std::string v = Lower(Trim(a_val));
            for (const auto& e : kPadNames) {
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

        // A whole number inside a range. Out of range is refused rather than clamped:
        // a value someone typed by hand and got wrong should keep the documented default,
        // not silently become the nearest legal thing.
        [[nodiscard]] std::uint32_t AsCount(const std::string& a_val, std::uint32_t a_def,
                                            std::uint32_t a_min, std::uint32_t a_max) {
            try {
                const auto n = std::stoul(a_val, nullptr, 10);
                return (n >= a_min && n <= a_max) ? static_cast<std::uint32_t>(n) : a_def;
            } catch (...) {
                return a_def;
            }
        }

        // A speed multiplier, 0..20. 0 is legal and means "no typewriter at all".
        [[nodiscard]] float AsSpeed(const std::string& a_val, float a_def) {
            try {
                const float f = std::stof(a_val);
                return (f >= 0.0f && f <= 20.0f) ? f : a_def;
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

        // Applying a key is split out of the reading loop because two files feed it: the
        // shipped ini, then MCM Helper's, whose values are the ones the player just
        // clicked and therefore win. Both go through the same clamps.
        void Apply(const std::string& key, const std::string& val) {
            if (key == "language") {
                g_language = Lower(val);
            } else if (key == "hidesealednodes") {
                g_hideSealedNodes = AsBool(val);
            } else if (key == "autostart") {
                g_autoStart = AsBool(val);
            } else if (key == "textspeed") {
                g_textSpeed = AsSpeed(val, g_textSpeed);
            } else if (key == "uiscale") {
                // Clamped, not trusted: 0 would make the interface invisible and there
                // would be no way left to reach the setting that did it.
                try {
                    g_uiScale = std::clamp(std::stof(val), 0.5f, 3.0f);
                } catch (...) {
                }
            } else if (key == "systemmenukey") {
                g_systemMenuKey = AsScanCode(val, g_systemMenuKey);
            } else if (key == "systemmenumodifier") {
                g_systemMenuModifier = AsScanCode(val, g_systemMenuModifier);
            } else if (key == "vrmenubutton") {
                g_vrMenuButton = AsScanCode(val, g_vrMenuButton);
            } else if (key == "gamepadmenubutton") {
                g_padMenuButton = AsPadButton(val, g_padMenuButton);
            } else if (key == "gamepadmenumodifier") {
                g_padMenuModifier = AsPadButton(val, g_padMenuModifier);
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
            } else if (key == "threatlabelnumbers") {
                g_threatNumbers = AsBool(val);
            } else if (key == "threatlabelresources") {
                g_threatResources = AsBool(val);
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
            } else if (key == "threatlabelaimradius") {
                // Clamped: 0 would mean "only a pixel-perfect hit counts" (the feature off
                // through the back door), and past half the screen height everything in
                // front of you is "aimed at".
                try {
                    g_threatAimRadius = std::clamp(
                        static_cast<std::uint32_t>(std::stoul(val, nullptr, 0)), 2u, 50u);
                } catch (...) {
                }
            } else if (key == "vrthreatlabelheight") {
                // Metres, clamped: this is a physical size in the world, and a 0 would
                // make every billboard degenerate while a large value would put a
                // house-sized banner over a mudcrab.
                try {
                    g_vrThreatHeight = std::clamp(std::stof(val), 0.03f, 1.0f);
                } catch (...) {
                }
            } else if (key == "firsttaskhours") {
                g_questFirstTaskHours = AsHours(val, g_questFirstTaskHours);
            } else if (key == "taskintervalhours") {
                g_questIntervalHours = AsHours(val, g_questIntervalHours);
            } else if (key == "killsperbounty") {
                g_killsPerBounty = AsCount(val, g_killsPerBounty, 0u, 10000u);
            } else if (key == "killbountypoints") {
                g_killBountyPoints =
                    static_cast<std::int32_t>(AsCount(val, static_cast<std::uint32_t>(
                                                               g_killBountyPoints), 1u, 1000u));
            } else if (key == "professionactionsperpoint") {
                g_professionActionsPerPoint = AsCount(val, g_professionActionsPerPoint, 0u, 10000u);
            } else if (key == "questrewardscale") {
                // Clamped rather than trusted: 0 would hand out objectives that pay
                // nothing, which reads as the quest system being broken rather than as a
                // setting. The floor still allows "barely worth it" at a tenth.
                try {
                    g_questRewardScale = std::clamp(std::stof(val), 0.1f, 10.0f);
                } catch (...) {
                }
            } else if (key == "nodecostscale") {
                // Clamped, not trusted: 0 would make the whole tree free, which reads as
                // the mod being broken rather than as a setting.
                try {
                    g_nodeCostScale = std::clamp(std::stof(val), 0.1f, 10.0f);
                } catch (...) {
                }
            } else if (key == "nodeeffectscale") {
                try {
                    g_nodeEffectScale = std::clamp(std::stof(val), 0.1f, 10.0f);
                } catch (...) {
                }
            } else if (key == "heroskilllevel") {
                g_heroSkillLevel = AsCount(val, g_heroSkillLevel, 0u, 100u);
            } else if (key == "heroattributetarget") {
                g_heroAttributeTarget = AsCount(val, g_heroAttributeTarget, 0u, 10000u);
            } else if (key == "herosystempoints") {
                g_heroSystemPoints =
                    static_cast<std::int32_t>(AsCount(val, static_cast<std::uint32_t>(g_heroSystemPoints), 0u, 100000u));
            } else if (key == "ascendedskilllevel") {
                g_ascendedSkillLevel = AsCount(val, g_ascendedSkillLevel, 0u, 100u);
            } else if (key == "ascendedattributetarget") {
                g_ascendedAttributeTarget = AsCount(val, g_ascendedAttributeTarget, 0u, 10000u);
            } else if (key == "ascendedsystempoints") {
                g_ascendedSystemPoints =
                    static_cast<std::int32_t>(AsCount(val, static_cast<std::uint32_t>(g_ascendedSystemPoints), 0u, 100000u));
            } else if (key == "questrerollbutton") {
                g_questRerollButton = AsBool(val);
            } else if (key == "selftestkey") {
                g_selfTestKey = AsScanCode(val, g_selfTestKey);
            } else if (key == "debugpointskey") {
                g_debugPointsKey = AsScanCode(val, g_debugPointsKey);
            } else if (key == "loginputdiagnostics") {
                g_logInputDiag = AsBool(val);
            }
        }

        // Read one ini and hand every key to Apply. a_mcm rewrites MCM Helper's spelling
        // into ours: it stores engine settings, whose names carry Bethesda's type prefix
        // (bThreatLabels, iFirstTaskHours, fUiScale) because the engine reads a setting's
        // type from its first letter. Ours do not, so the prefix comes off.
        bool ReadIni(const char* a_path, bool a_mcm) {
            std::ifstream in(a_path);
            if (!in) {
                return false;
            }

            // Deliberately section-agnostic: a flat key=value scan is enough for our
            // handful of settings and keeps the parser trivial. Lines starting ';' or '#'
            // (or an inline trailer of one) are comments.
            std::string line;
            while (std::getline(in, line)) {
                if (const auto cut = line.find_first_of(";#"); cut != std::string::npos) {
                    line.erase(cut);
                }
                const auto eq = line.find('=');
                if (eq == std::string::npos) {
                    continue;
                }
                std::string key = Lower(Trim(line.substr(0, eq)));
                std::string val = Trim(line.substr(eq + 1));

                if (a_mcm) {
                    if (key.size() > 1 && (key[0] == 'b' || key[0] == 'i' || key[0] == 'f')) {
                        key.erase(0, 1);
                    }
                    // The one setting whose shapes differ. Ours is a word; the MCM stores
                    // the index of the chosen option, because MCM Helper has no string
                    // setting to store a word in.
                    if (key == "threatlabeltargets") {
                        static constexpr const char* kNames[] = { "aggro", "hostile",
                                                                  "crosshair", "all" };
                        try {
                            const auto i = std::stoul(val, nullptr, 10);
                            if (i < std::size(kNames)) {
                                val = kNames[i];
                            }
                        } catch (...) {
                        }
                    }
                }
                Apply(key, val);
            }
            return true;
        }

    }

    void Load() {
        // Defaults, overwritten only by an explicit key below.
        g_hideSealedNodes = false;
        g_autoStart = true;
        g_textSpeed = 1.0f;
        g_uiScale = 1.0f;
        g_systemMenuKey = 0x1F;       // DIK_S
        g_systemMenuModifier = 0x36;  // DIK_RSHIFT
        g_vrMenuButton = 0;
        g_padMenuButton = 0x0020;
        g_padMenuModifier = 0x0100;
        g_dormantHeroLevel = 25;
        g_dormantAscendedLevel = 80;
        g_storageCodex = false;
        g_skyrimNetIntegration = true;
        g_threatLabels = true;
        g_threatTargets = ThreatTargets::kAggro;
        g_threatNumbers = true;
        g_threatResources = true;
        g_threatRange = 4000;
        g_threatAimRadius = 15;
        g_vrThreatHeight = 0.15f;
        g_threatKey = 0x44;
        g_questFirstTaskHours = 12;
        g_questIntervalHours = 24;
        g_questRewardScale = 1.0f;
        g_killsPerBounty = 50;
        g_killBountyPoints = 1;
        g_professionActionsPerPoint = 25;
        g_nodeCostScale = 1.0f;
        g_nodeEffectScale = 1.0f;
        g_heroSkillLevel = 50;
        g_heroAttributeTarget = 180;
        g_heroSystemPoints = 5;
        g_ascendedSkillLevel = 100;
        g_ascendedAttributeTarget = 600;
        g_ascendedSystemPoints = 10;
        g_questRerollButton = false;
        g_selfTestKey = 0;
        g_logInputDiag = false;

        if (!ReadIni(kIniPath, false)) {
            logger::info("Config: no ini at {} — using defaults", kIniPath);
        }

        // MCM Helper writes what the player chose here, and it is layered ON TOP: a key
        // it does not mention falls through to the shipped ini rather than resetting.
        // Nothing is ever written back - two writers on one file is how settings quietly
        // revert, and the menu is the only thing that needs to write.
        if (ReadIni(kMcmIniPath, true)) {
            logger::info("Config: MCM settings applied over the ini ({})", kMcmIniPath);
        }

        // The ladder only makes sense upwards. Swapped thresholds would otherwise
        // hand out ASCENDED first and then never fire HERO at all.
        if (g_dormantAscendedLevel <= g_dormantHeroLevel) {
            logger::warn("Config: DormantAscendedLevel ({}) must be above DormantHeroLevel ({}) — "
                         "raising it to {}",
                         g_dormantAscendedLevel, g_dormantHeroLevel, g_dormantHeroLevel + 1);
            g_dormantAscendedLevel = static_cast<std::uint16_t>(g_dormantHeroLevel + 1);
        }

        logger::info("Config: Language={}, AutoStart={}, TextSpeed={}, "
                     "HideSealedNodes={}, SystemMenuKey={}, SystemMenuModifier={}, "
                     "VRMenuButton={}, GamepadMenuButton={}, GamepadMenuModifier={}, "
                     "DormantHeroLevel={}, DormantAscendedLevel={}, StorageCodex={}, "
                     "SkyrimNetIntegration={}, ThreatLabels={} (targets={}, range={}, aim={}%, "
                     "key={}), "
                     "FirstTaskHours={}, TaskIntervalHours={}, QuestRewardScale={}, QuestRerollButton={}, "
                     "SelfTestKey={}, DebugPointsKey={}, LogInputDiagnostics={}"
                     ", NodeCostScale={}, NodeEffectScale={}, Hero={}/{}/{}, Ascended={}/{}/{}",
                     g_language, g_autoStart, g_textSpeed,
                     g_hideSealedNodes, KeyName(g_systemMenuKey), KeyName(g_systemMenuModifier),
                     KeyName(g_vrMenuButton), GamepadButtonName(g_padMenuButton),
                     GamepadButtonName(g_padMenuModifier),
                     g_dormantHeroLevel, g_dormantAscendedLevel, g_storageCodex,
                     g_skyrimNetIntegration, g_threatLabels,
                     g_threatTargets == ThreatTargets::kAll         ? "all"
                     : g_threatTargets == ThreatTargets::kCrosshair ? "crosshair"
                     : g_threatTargets == ThreatTargets::kHostile   ? "hostile"
                                                                    : "aggro",
                     g_threatRange, g_threatAimRadius, KeyName(g_threatKey), g_questFirstTaskHours,
                     g_questIntervalHours, g_questRewardScale, g_questRerollButton, KeyName(g_selfTestKey),
                     KeyName(g_debugPointsKey), g_logInputDiag,
                     g_nodeCostScale, g_nodeEffectScale, g_heroSkillLevel,
                     g_heroAttributeTarget, g_heroSystemPoints, g_ascendedSkillLevel,
                     g_ascendedAttributeTarget, g_ascendedSystemPoints);
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

    const std::string& Language() {
        return g_language;
    }

    bool HideSealedNodes() {
        return g_hideSealedNodes;
    }

    bool AutoStart() {
        return g_autoStart;
    }

    float TextSpeed() {
        return g_textSpeed;
    }

    float UiScale() {
        return g_uiScale;
    }

    std::uint32_t SystemMenuKey() {
        return g_systemMenuKey;
    }

    std::uint32_t SystemMenuModifier() {
        return g_systemMenuModifier;
    }

    std::uint32_t VRMenuButton() {
        return g_vrMenuButton;
    }

    std::uint32_t GamepadMenuButton() {
        return g_padMenuButton;
    }

    std::uint32_t GamepadMenuModifier() {
        return g_padMenuModifier;
    }

    std::string GamepadButtonName(std::uint32_t a_button) {
        const auto hex = std::format("{:#06x}", a_button);
        if (a_button == 0) {
            return hex + " (none)";
        }
        for (const auto& e : kPadNames) {
            if (e.code == a_button) {
                std::string name = e.name;
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                return hex + " (" + name + ")";
            }
        }
        return hex;
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

    bool ThreatLabelNumbers() {
        return g_threatNumbers;
    }

    bool ThreatLabelResources() {
        return g_threatResources;
    }

    ThreatTargets ThreatLabelTargets() {
        return g_threatTargets;
    }

    std::uint32_t ThreatLabelKey() {
        return g_threatKey;
    }

    float VRThreatLabelHeight() {
        return g_vrThreatHeight;
    }

    std::uint32_t ThreatLabelAimRadius() {
        return g_threatAimRadius;
    }

    std::uint32_t ThreatLabelRange() {
        return g_threatRange;
    }

    std::uint32_t QuestFirstTaskHours() {
        return g_questFirstTaskHours;
    }

    std::uint32_t KillsPerBounty() {
        return g_killsPerBounty;
    }

    std::int32_t KillBountyPoints() {
        return g_killBountyPoints;
    }

    std::uint32_t ProfessionActionsPerPoint() {
        return g_professionActionsPerPoint;
    }

    float QuestRewardScale() {
        return g_questRewardScale;
    }

    float NodeCostScale() {
        return g_nodeCostScale;
    }

    float NodeEffectScale() {
        return g_nodeEffectScale;
    }

    std::uint32_t HeroSkillLevel() {
        return g_heroSkillLevel;
    }

    std::uint32_t HeroAttributeTarget() {
        return g_heroAttributeTarget;
    }

    std::int32_t HeroSystemPoints() {
        return g_heroSystemPoints;
    }

    std::uint32_t AscendedSkillLevel() {
        return g_ascendedSkillLevel;
    }

    std::uint32_t AscendedAttributeTarget() {
        return g_ascendedAttributeTarget;
    }

    std::int32_t AscendedSystemPoints() {
        return g_ascendedSystemPoints;
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

    std::uint32_t DebugPointsKey() {
        return g_debugPointsKey;
    }

    bool LogInputDiagnostics() {
        return g_logInputDiag;
    }
}
