#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Isekai::UI {

    // A panel button: a label, and optionally a PNG icon drawn beside it.
    // iconOnly buttons leave the bottom row entirely: they render as large, bare
    // icons anchored to the panel's top-right (the label survives as a tooltip).
    struct Choice {
        std::string label;
        std::string icon;  // path relative to the game folder; empty = text only
        bool        iconOnly = false;
    };

    // Open the System panel. Call from the main thread.
    // a_onSelect receives the 0-based index of the chosen button, and is invoked
    // back on the main thread (the panel itself is drawn on the render thread).
    // a_revealCharsPerSec drives the typewriter: the default reads nicely for a few
    // lines of story text, but a long status listing would take half a minute to type
    // itself out — pass something large there.
    // a_width is in 1080p pixels (scaled with the display): the default suits story
    // panels; tabular ones like the status ledger need more room, or their columns
    // wrap and the table falls apart.
    void ShowSystemWindow(std::string a_title, std::string a_body,
                          std::vector<Choice> a_choices,
                          std::function<void(int)> a_onSelect,
                          float a_revealCharsPerSec = 45.0f, float a_width = 720.0f);

    // Convenience for the common text-only case.
    void ShowSystemWindow(std::string a_title, std::string a_body,
                          std::vector<std::string> a_choices,
                          std::function<void(int)> a_onSelect,
                          float a_revealCharsPerSec = 45.0f, float a_width = 720.0f);

    // True while the panel is up — i.e. while it should own mouse and keyboard.
    [[nodiscard]] bool IsSystemWindowOpen();

    // Draw the panel. Called by the overlay once per frame, inside an ImGui frame.
    void DrawSystemWindow();
}
