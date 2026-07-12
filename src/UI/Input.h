#pragma once

#include <cstdint>
#include <functional>

struct ImGuiIO;

namespace Isekai::UI {

    // Run fn when a key is pressed, whether or not a panel is open. The callback is
    // dispatched to the main thread, so it is safe to touch game state from it.
    // a_scanCode is a DirectInput scan code (F11 = 0x57).
    void RegisterHotkey(std::uint32_t a_scanCode, std::function<void()> a_fn);

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
