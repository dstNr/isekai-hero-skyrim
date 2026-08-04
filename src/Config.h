#pragma once

#include <cstdint>

// Lightweight INI settings, read once at kDataLoaded from
// Data\SKSE\Plugins\IsekaiHero.ini. Absent file / keys fall back to defaults, so
// the mod runs fine with no ini at all.

namespace Isekai::Config {

    // Parse the ini (idempotent). Call at kDataLoaded.
    void Load();

    // Skill tree: hide nodes gated above the player's rebirth tier instead of showing
    // them greyed with a "requires HERO/ASCENDED" hint. Default false (show greyed).
    [[nodiscard]] bool HideSealedNodes();

    // The key that opens the [ SYSTEM ] menu, as a DirectInput scan code. Default 0x1F
    // (S). Read once at load; the hotkey is registered from it.
    [[nodiscard]] std::uint32_t SystemMenuKey();

    // Scan code that must be held with the key above. Default 0x36 (Right Shift);
    // 0 = no modifier (the key alone opens the menu).
    [[nodiscard]] std::uint32_t SystemMenuModifier();

    // Character levels at which a DORMANT blessing rises to HERO / ASCENDED.
    // Defaults 25 and 80 — 80 is where the Ebony Warrior comes knocking, i.e. the
    // point vanilla itself treats as "you are done being mortal".
    [[nodiscard]] std::uint16_t DormantHeroLevel();
    [[nodiscard]] std::uint16_t DormantAscendedLevel();

    // Feed the player's System status (blessing, milestones) to SkyrimNet, if it is
    // installed, so AI-driven NPCs can react to the reincarnated hero. Default true, but
    // it only ever does anything when SkyrimNet is actually present — off is for players
    // who run SkyrimNet yet want no System context in it. See src/SkyrimNet.cpp.
    [[nodiscard]] bool SkyrimNetIntegration();

    // The key that runs a System Analysis scan (once the matching skill-tree node is
    // bought), as a DirectInput scan code. Default 0x2F (V) — no modifier; distinct from
    // the System-menu key so it can fire without opening a menu. See src/Analyze.cpp.
    [[nodiscard]] std::uint32_t AnalyzeKey();

    // Scan code that runs the in-game self-test (src/SelfTest.cpp), or 0 for off, which
    // is the default — it is a diagnostic to be switched on when reporting a problem,
    // not something a normal playthrough should be able to trigger by accident.
    [[nodiscard]] std::uint32_t SelfTestKey();

    // Log every button press with its input device and scan code. Off by default; it is
    // noisy, and only useful when a hotkey does not fire.
    //
    // The point is that it needs no special build: when a tester reports a dead hotkey,
    // the log otherwise cannot distinguish "no input event arrives at all" from "one
    // arrives under a device or scan code we do not accept". That second case is real —
    // Skyrim VR delivers controller buttons as kVRRight/kVRLeft, which the hotkey path
    // ignores.
    [[nodiscard]] bool LogKeyPresses();
}
