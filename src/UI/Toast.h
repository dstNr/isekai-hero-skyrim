#pragma once

#include <string>

// Transient status lines, top-centre, the way an MMO reports progress: they appear over
// normal gameplay, never take focus or pause anything, and fade out on their own.
//
// This exists because RE::SendHUDMessage::ShowHUDMessage was the wrong instrument for quest feedback.
// It writes to Skyrim's own corner queue, which is shared with every other mod, holds
// several lines at once, and is easy to miss entirely — the same property that made the
// self-test look broken. Kill progress needs to be seen the moment it happens.
//
// Drawn by the ImGui overlay on SE/AE, and on ImGuiVRHelper's head-locked HUD plane in
// VR. With neither surface available — VR without the helper installed — the calls fall
// back to the HUD message rather than going silent.

namespace Isekai::UI {

    // Post a line. Safe from any thread — the queue is locked and the drawing happens on
    // the render thread. An empty string is ignored.
    //
    // a_key groups related messages: posting with a key already on screen REPLACES that
    // line and restarts its timer, instead of stacking a second one. Quest progress uses
    // it, so twenty kills leave one counter counting up rather than twenty toasts.
    // Pass an empty key for a standalone message.
    void ShowToast(std::string a_text, std::string a_key = {});

    // A toast that stays longer and reads as an announcement rather than a tally — the
    // System handing out a new objective, or paying one out.
    void ShowToastBanner(std::string a_text, std::string a_key = {});

    // Draw and expire. Called by the overlay once per frame, inside an ImGui frame.
    //
    // a_dt is how much time to age the queue by, in seconds; negative means "ask ImGui".
    // The VR path has to pass a real one: the helper SDK's RenderHud pins its private
    // context's DeltaTime to a flat 1/60, so on a 90 Hz headset every toast would sit
    // there half again as long as it was asked to.
    void DrawToasts(float a_dt = -1.0f);
}
