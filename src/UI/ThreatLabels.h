#pragma once

// The System's read on what is in front of you, floating above the actor itself.
//
// This replaces the old Analyze hotkey. That gated a threat verdict behind a skill-tree
// node and a key press, which is backwards for something you want to know at the moment
// you decide whether to fight: by the time you have pressed a key and read a panel, the
// decision has been made for you. The verdict is now simply visible, always, and the
// panel it used to live in is gone.
//
// This file only SELECTS: which actors get a label and what each one says. The hud page
// draws them (magelight/IsekaiHero/hud.html) and UI/Hud.cpp pushes them there from the
// frame tick. On flat screens a label floats over the actor's head at a projected point;
// in VR, where Magelight cannot pin an element over a moving actor, the page shows the
// same labels as a head-locked scan list (spec 8).

#include "UI/UiLogic.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Isekai::UI {

    // Arm the on/off key (Config::ThreatLabelKey) from the current Config. Call after
    // UI::ClearHotkeys(), at load and again whenever the settings are re-read. a_log is
    // true at load only; a rebind stays quiet so a journal close does not write to the log.
    void BindThreatLabelHotkeys(bool a_log);

    // Off and on again for this session. The ini decides where it starts; this is
    // deliberately NOT saved - a display toggle is not part of a character.
    void ToggleThreatLabels();
    [[nodiscard]] bool ThreatLabelsVisible();

    // A point in the hud page's space: 0..1 of the view, y running DOWN.
    struct ScreenPt {
        float x = 0.0f;
        float y = 0.0f;
    };

    // One resource pool's worth of numbers.
    struct LabelPool {
        std::int32_t now = 0;
        std::int32_t max = 0;  // 0 = this actor has none
    };

    // One label, already judged.
    struct LabelData {
        std::uint32_t  id = 0;    // the actor's FormID, stable while it stays labelled
        ScreenPt       pos;       // the head point; {0, 0} when not projected (VR)
        float          distance = 0.0f;
        Logic::Verdict verdict = Logic::Verdict::Manageable;
        const char*    tag = "";  // the verdict word, already translated
        std::int32_t   level = 0;
        LabelPool      hp;
        LabelPool      mp;
        LabelPool      sp;
        std::string    name;      // empty = unnamed; the page says "Unknown"
    };

    // Main thread. The labels for this frame, nearest first, at most 12. Empty when there
    // is nothing to label: no player in the world, the game paused, no camera.
    //
    // a_project: project each head point to the screen (flat screens). The projection
    // also filters (off screen = no label) and drives the crosshair cone of the
    // Crosshair and Aggro modes. Without it (VR) range and line of sight are the whole
    // filter and pos stays {0, 0}.
    [[nodiscard]] std::vector<LabelData> SelectLabels(bool a_project);

    // Main thread, on load and new game. Forget which actors the lethal warning already
    // spoke about, so the new session warns afresh.
    void ResetLethalWarnings();

    // Main thread, flat screens. The game's render size in pixels (1920 x 1080 if it
    // cannot be read): what the projection and LabelPushGate measure pixels in.
    void LabelScreenSize(float& a_width, float& a_height);
}
