#pragma once

// The System's read on what is in front of you, floating above the actor itself.
//
// This replaces the old Analyze hotkey. That gated a threat verdict behind a skill-tree
// node and a key press, which is backwards for something you want to know at the moment
// you decide whether to fight: by the time you have pressed a key and read a panel, the
// decision has been made for you. The verdict is now simply visible, always, and the
// panel it used to live in is gone.
//
// Drawn by the ImGui overlay, so like everything else on that layer this is SE/AE only —
// the overlay hook is deliberately skipped in Skyrim VR.

namespace Isekai::UI {

    // Arm the on/off key (Config::ThreatLabelKey). Call at kDataLoaded, after the ini
    // has been read. No key is armed in VR: the labels are drawn by the ImGui overlay,
    // which VR does not get, so a key that toggled an invisible feature would be worse
    // than no key at all.
    void InstallThreatLabels();

    // Draw the labels. Called by the overlay once per frame, inside an ImGui frame.
    // Cheap no-op when the feature is switched off in the ini or by the key.
    void DrawThreatLabels();

    // Off and on again for this session. The ini decides where it starts; this is
    // deliberately NOT saved — a display toggle is not part of a character.
    void ToggleThreatLabels();
    [[nodiscard]] bool ThreatLabelsVisible();
}
