#pragma once

namespace Isekai::UI {

    // Hook the game's D3D11 swap chain so we can draw on top of the rendered frame.
    // Call once, after the renderer exists (kDataLoaded is early enough).
    void Install();

    // True while one of our windows is up and should own mouse and keyboard.
    [[nodiscard]] bool IsCapturingInput();

    // True once the overlay is hooked and drawing. False in Skyrim VR, where the hook is
    // deliberately skipped, and during the first frames of a session. Anything that draws
    // through the overlay rather than through a game menu has to ask first and fall back —
    // otherwise the feature is simply invisible in VR with nothing to say why.
    [[nodiscard]] bool OverlayReady();

    // Hold the world still while a panel is up: pause the game (the same counter a
    // vanilla menu bumps) and block the player-control handlers. Balanced by an
    // internal flag, so redundant calls cannot skew the pause count. Main thread only.
    void SetGameHold(bool a_hold);
}
