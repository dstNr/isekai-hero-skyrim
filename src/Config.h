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

    // Language code for the translation file at
    // Data\SKSE\Plugins\IsekaiHero\lang\<code>.txt. Default "en", which loads nothing
    // and leaves every string as it ships. A missing file falls back to English rather
    // than failing, and so does every key a translation does not cover.
    [[nodiscard]] const std::string& Language();

    // Skill tree: hide nodes gated above the player's rebirth tier instead of showing
    // them greyed with a "requires HERO/ASCENDED" hint. Default false (show greyed).
    [[nodiscard]] bool HideSealedNodes();

    // Let the System bind itself the first time the player is in control of the world.
    // Default true. Off means it waits for the System hotkey instead — the answer to
    // alternate starts that open with a prologue somewhere the boot sequence does not
    // belong (a modern-world intro, a dream, a prison cell), which we cannot detect and
    // the player can.
    [[nodiscard]] bool AutoStart();

    // Multiplier on the typewriter that reveals panel text. 1.0 = as written, 2.0 =
    // twice as fast, 0 = no typing at all. Clamped to 0..20.
    //
    // This exists for the second playthrough and for anyone testing a modlist: the
    // reveal is a story beat exactly once, and a tester who restarts twenty times reads
    // the same boot sequence twenty times. A panel can also be clicked to skip its
    // reveal, which no setting can replace — but a setting is what stops the mod from
    // being disabled between tests.
    [[nodiscard]] float TextSpeed();

    // The key that opens the [ SYSTEM ] menu, as a DirectInput scan code. Default 0x1F
    // (S). Read once at load; the hotkey is registered from it.
    [[nodiscard]] std::uint32_t SystemMenuKey();

    // Scan code that must be held with the key above. Default 0x36 (Right Shift);
    // 0 = no modifier (the key alone opens the menu).
    [[nodiscard]] std::uint32_t SystemMenuModifier();

    // The same on a controller, as XInput button masks (A = 0x1000, Back = 0x0020).
    // Default Back with Left Shoulder held, and 0 for the button switches the controller
    // off entirely.
    //
    // A combination rather than one button, because a controller has no spare buttons:
    // every face button, both shoulders and both sticks are bound in vanilla, and a mod
    // that claims one of them outright breaks whatever it took. Holding LB and tapping
    // Back does fire LB's own action (a shout) on the way in — harmless, and the price of
    // not stealing a binding.
    [[nodiscard]] std::uint32_t GamepadMenuButton();
    [[nodiscard]] std::uint32_t GamepadMenuModifier();

    // Multiplier on the size of everything the mod draws: panels, text, the skill tree,
    // the threat labels. 1.0 = as authored, 1.5 = half again as large. Clamped to 0.5..3.
    //
    // The interface already scales with resolution, which keeps it the same *apparent*
    // size on a 4K screen — that is not the same question as "can you read it", and a
    // player who asked for this told me plainly which question they had.
    [[nodiscard]] float UiScale();

    // Button on a Skyrim VR motion controller that opens the System menu, as the code the
    // game reports for it. Default 0 = none.
    //
    // No default binding on purpose: VR controller codes are not standardised the way
    // scan codes are, they differ between headsets, and guessing one would steal a grip
    // or a trigger from whoever is holding it. Set LogInputDiagnostics = 1, press the
    // button you want, and read its code out of the log.
    [[nodiscard]] std::uint32_t VRMenuButton();

    // A readable name for an XInput button mask ("0x0020 (BACK)"), for log lines.
    [[nodiscard]] std::string GamepadButtonName(std::uint32_t a_button);

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
        kAggro,      // what you are looking at, plus whatever is actually fighting you
                     // (default)
        kHostile,    // every enemy in range, whether or not it has noticed you
        kAll,        // every actor that is not you or a follower — includes townspeople
        kCrosshair,  // only what you are looking at
    };
    [[nodiscard]] ThreatTargets ThreatLabelTargets();

    // Scan code that switches the threat labels off and on again mid-session, or 0 for
    // no key at all. Default 0x44 (F10) — a function key rather than a letter, since
    // every letter is a movement or an action in a game that has no key to spare. The ini
    // decides whether they START on; this decides nothing permanent, and a toggle is not
    // saved.
    [[nodiscard]] std::uint32_t ThreatLabelKey();

    // Draw the actual health figure on the threat label's bar ("312 / 480"). Default on.
    // A bar answers "roughly how much is left"; a fight where it matters whether that is
    // 40 or 400 wants the number.
    [[nodiscard]] bool ThreatLabelNumbers();

    // Show magicka and stamina under the health bar — side by side in one thin strip,
    // blue and green, no figures. Default ON: sharing a row costs the frame five pixels,
    // which is what a row each did not (that layout is why this used to default off).
    // A pool is skipped for an actor that does not have one, so most animals still show
    // a bare health bar and a wolf's frame is exactly as tall as before.
    [[nodiscard]] bool ThreatLabelResources();

    // How far a labelled actor may be, in game units (~70 per metre). Beyond this the
    // label is dropped entirely rather than shrunk to an unreadable smudge.
    [[nodiscard]] std::uint32_t ThreatLabelRange();

    // How far off the crosshair an actor may be and still count as "aimed at", as a
    // PERCENTAGE of screen height. Sideways tolerance only — the check measures against
    // the actor's whole body, so height is already covered. Default 15, clamped 2..50.
    [[nodiscard]] std::uint32_t ThreatLabelAimRadius();

    // VR only: how tall a threat label stands in the world, in METRES.
    //
    // On a flat screen the frame is measured in pixels and shrunk with distance by hand.
    // In VR it is a billboard of a fixed physical size, so distance shrinks it for free —
    // this is the one number that decides whether it reads as a nameplate over the actor
    // or as a signboard hanging in front of it.
    [[nodiscard]] float VRThreatLabelHeight();

    // System objectives (src/Quests.cpp), in GAME hours.
    //
    // FirstTask is the wait before the very first objective of a character's life;
    // Interval is the wait after each completed one. Both exist because "the System hands
    // out work on its own schedule" is the whole idea — an objective that is simply there
    // the moment you are reincarnated, and again the instant you finish one, is a chore
    // list rather than something that happens to you.
    //
    // 0 means "at once", which is what you want while testing. Clamped to 0..720 (30 days).
    [[nodiscard]] std::uint32_t QuestFirstTaskHours();
    [[nodiscard]] std::uint32_t QuestIntervalHours();
    [[nodiscard]] float         QuestRewardScale();

    // 0 = no bounties. How many kills of one quarry type pay KillBountyPoints.
    [[nodiscard]] std::uint32_t KillsPerBounty();
    [[nodiscard]] std::int32_t  KillBountyPoints();
    // 0 = professions off. Harvest/smelt/tan/craft actions per System Point.
    [[nodiscard]] std::uint32_t ProfessionActionsPerPoint();

    // Show the NEW TASK button on the status panel, which throws the standing objective
    // away and rolls another one — free, instantly, as often as you like.
    //
    // A TESTING TOOL, and off by default because of what it does to the feature it
    // tests: objectives are meant to arrive on the System's schedule, and a button that
    // re-rolls until an easy quarry comes up is the shortest possible way around that.
    // docs/MANUAL_TESTS.md needs it — walking the objective table otherwise means
    // hunting whatever the dice picked, one quarry at a time.
    [[nodiscard]] bool QuestRerollButton();

    // Scan code that runs the in-game self-test (src/SelfTest.cpp), or 0 for off, which
    // is the default — it is a diagnostic to be switched on when reporting a problem,
    // not something a normal playthrough should be able to trigger by accident.
    [[nodiscard]] std::uint32_t SelfTestKey();

    // Scan code that grants 100 System Points, or 0 for off (the default). A debugging
    // tool: milestones are the only source of points, so testing the skill tree or the
    // shop — or reproducing a report about either — otherwise means playing far enough
    // to afford the thing under test.
    [[nodiscard]] std::uint32_t DebugPointsKey();

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
