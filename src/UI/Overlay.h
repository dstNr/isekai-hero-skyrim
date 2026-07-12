#pragma once

namespace Isekai::UI {

    // Hook the game's D3D11 swap chain so we can draw on top of the rendered frame.
    // Call once, after the renderer exists (kDataLoaded is early enough).
    void Install();

    // True while one of our windows is up and should own mouse and keyboard.
    [[nodiscard]] bool IsCapturingInput();
}
