#pragma once

// The System's read on what is in front of you, floating above the actor itself.
//
// This replaces the old Analyze hotkey. That gated a threat verdict behind a skill-tree
// node and a key press, which is backwards for something you want to know at the moment
// you decide whether to fight: by the time you have pressed a key and read a panel, the
// decision has been made for you. The verdict is now simply visible, always, and the
// panel it used to live in is gone.
//
// Two renderers, one set of labels. On SE/AE the frames are drawn straight onto the
// screen at a projected point. In VR they are drawn into a helper-owned panel texture and
// handed to ImGuiVRHelper as world-anchored billboards, which is the only way they stay
// on the actor instead of swimming with the head — see DrawThreatLabelsVR.

#include "ImGuiVRHelperTypes.h"  // WorldQuad; header-only, no ImGui/OpenVR dependency

#include <imgui.h>

#include <vector>

namespace Isekai::UI {

    // Arm the on/off key (Config::ThreatLabelKey). Call at kDataLoaded, after the ini
    // has been read. No key is armed in VR: the labels are drawn by the ImGui overlay,
    // which VR does not get, so a key that toggled an invisible feature would be worse
    // than no key at all.
    void InstallThreatLabels();

    // Draw the labels. Called by the overlay once per frame, inside an ImGui frame.
    // Cheap no-op when the feature is switched off in the ini or by the key.
    void DrawThreatLabels();

    // The VR path. Draws each frame into its own horizontal band of a a_panel-sized
    // texture and appends one billboard per label to a_out, positioned at the actor's
    // head in Skyrim world units — the helper does the projection, so nothing here
    // depends on a camera.
    //
    // One band per label rather than a grid: a band is the full panel width, so a long
    // name can never bleed into the neighbouring label's sub-rect and end up sampled by
    // the wrong billboard.
    //
    // Call inside the VR client's own ImGui frame. a_out is cleared first; an empty
    // result must still be submitted, or the helper keeps showing the previous frame.
    void DrawThreatLabelsVR(const ImVec2&                                     a_panel,
                            std::vector<ImGuiVRHelperPluginAPI::WorldQuad>&   a_out);

    // Off and on again for this session. The ini decides where it starts; this is
    // deliberately NOT saved — a display toggle is not part of a character.
    void ToggleThreatLabels();
    [[nodiscard]] bool ThreatLabelsVisible();
}
