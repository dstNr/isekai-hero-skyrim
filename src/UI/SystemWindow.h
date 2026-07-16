#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Isekai::UI {

    // Open the System panel. Call from the main thread.
    // a_onSelect receives the 0-based index of the chosen button, and is invoked
    // back on the main thread (the panel itself is drawn on the render thread).
    // a_revealCharsPerSec drives the typewriter: the default reads nicely for a few
    // lines of story text, but a long status listing would take half a minute to type
    // itself out — pass something large there.
    void ShowSystemWindow(std::string a_title, std::string a_body,
                          std::vector<std::string> a_choices,
                          std::function<void(int)> a_onSelect,
                          float a_revealCharsPerSec = 45.0f);

    // True while the panel is up — i.e. while it should own mouse and keyboard.
    [[nodiscard]] bool IsSystemWindowOpen();

    // Draw the panel. Called by the overlay once per frame, inside an ImGui frame.
    void DrawSystemWindow();
}
