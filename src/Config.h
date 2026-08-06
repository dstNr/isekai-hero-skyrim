#pragma once

#include <cstdint>
#include <string>

// Lightweight INI settings, read once at kDataLoaded from
// Data\SKSE\Plugins\IsekaiHero.ini. Absent file / keys fall back to defaults, so
// the mod runs fine with no ini at all.

namespace Isekai::Config {

    // Parse the ini (idempotent). Call at kDataLoaded.
    void Load();

    // A readable name for a DirectInput scan code ("0x57 (F11)"), for log lines. A hotkey
    // that misbehaves is nearly always bound to something other than what its owner
    // thinks, and a bare number in the log does not make that obvious.
    [[nodiscard]] std::string KeyName(std::uint32_t a_scanCode);

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

    // Hand the player the storage codex — an inventory item meant to open the Dimensional
    // Storage when "drunk". **Default false: the feature is not finished.** The item is
    // handed out and it carries the container's own name, but drinking it does not open
    // anything, so all it does today is sit in the inventory looking like a stray copy of
    // the chest. Turn it back on once that works. See src/Storage.cpp.
    [[nodiscard]] bool StorageCodex();

    // Feed the player's System status (blessing, milestones) to SkyrimNet, if it is
    // installed, so AI-driven NPCs can react to the reincarnated hero. Default true, but
    // it only ever does anything when SkyrimNet is actually present — off is for players
    // who run SkyrimNet yet want no System context in it. See src/SkyrimNet.cpp.
    [[nodiscard]] bool SkyrimNetIntegration();

    // Floating threat verdicts over actors (src/UI/ThreatLabels.cpp). Default on.
    // SE/AE only — they are drawn by the ImGui overlay, which Skyrim VR does not get.
    [[nodiscard]] bool ThreatLabels();

    // Which actors carry one.
    enum class ThreatTargets {
        kHostile,    // enemies only: hostile to you, or already fighting you (default)
        kAll,        // every actor that is not you or a follower — includes townspeople
        kCrosshair,  // only what you are looking at
    };
    [[nodiscard]] ThreatTargets ThreatLabelTargets();

    // How far a labelled actor may be, in game units (~70 per metre). Beyond this the
    // label is dropped entirely rather than shrunk to an unreadable smudge.
    [[nodiscard]] std::uint32_t ThreatLabelRange();

    // Scan code that runs the in-game self-test (src/SelfTest.cpp), or 0 for off, which
    // is the default — it is a diagnostic to be switched on when reporting a problem,
    // not something a normal playthrough should be able to trigger by accident.
    [[nodiscard]] std::uint32_t SelfTestKey();

    // Log button presses with their input device and scan code, to diagnose a hotkey that
    // does nothing. Off by default, and it stops itself after a short burst (see
    // kInputDiagnosticLimit in Input.cpp).
    //
    // It needs no special build, which is the point: when a tester reports a dead hotkey,
    // the log otherwise cannot distinguish "no input event arrives at all" from "one
    // arrives under a device we do not accept". That second case is real — Skyrim VR
    // delivers controller buttons as kVRRight/kVRLeft, which the hotkey path ignores.
    //
    // On what this records, since the name invites the question: we subscribe to Skyrim's
    // own in-process event stream (BSInputDeviceManager), not a Windows keyboard hook —
    // no SetWindowsHookEx, no GetAsyncKeyState, no raw input. Only presses the running
    // game routes to us, and only as device + scan code; nothing anywhere translates a
    // scan code into a character. Text typed into a game field would still appear as
    // scan codes though, which is why this is off by default and self-limiting.
    [[nodiscard]] bool LogInputDiagnostics();
}
