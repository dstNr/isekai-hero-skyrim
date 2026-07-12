#pragma once

struct ImGuiIO;

namespace Isekai::UI {

    // Subscribe to Skyrim's own input event stream.
    //
    // We cannot read the mouse through the window message queue: Skyrim grabs it
    // via DirectInput, so WM_LBUTTONDOWN & friends never reach the window proc.
    // The game's InputEvent bus is the only place the clicks actually show up.
    void InstallInput();

    // Push the accumulated input into ImGui. Must run on the render thread,
    // *before* ImGui::NewFrame().
    void FeedImGui(ImGuiIO& a_io);
}
