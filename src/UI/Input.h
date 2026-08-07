#pragma once

#include <cstdint>
#include <functional>

struct ImGuiIO;

namespace Isekai::UI {

    // Run fn when a key is pressed, whether or not a panel is open. The callback is
    // dispatched to the main thread, so it is safe to touch game state from it.
    // Scan codes are DirectInput (F10 = 0x44, right shift = 0x36). A non-zero
    // a_modifier must be held down at the moment the key is pressed.
    void RegisterHotkey(std::uint32_t a_scanCode, std::function<void()> a_fn,
                        std::uint32_t a_modifier = 0);

    // The same, for a controller. a_button is an XInput mask (A = 0x1000, Back = 0x0020),
    // which is what Skyrim puts in a gamepad ButtonEvent's id code.
    //
    // Separate from the call above rather than an extra parameter, because the two
    // number spaces overlap: gamepad D-pad up is 0x0001 and so is keyboard ESCAPE. The
    // hotkey table keys on the device as well as the code for that reason.
    void RegisterGamepadHotkey(std::uint32_t a_button, std::function<void()> a_fn,
                               std::uint32_t a_modifier = 0);

    // Log every hotkey that ended up armed. Call once, after everything has registered.
    // Each module logs its own registration already, but those lines are scattered
    // through the load log and say what a module INTENDED — this is the table the input
    // handler actually consults, which is the only thing that answers "why does my key
    // do nothing".
    void LogHotkeys();

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
