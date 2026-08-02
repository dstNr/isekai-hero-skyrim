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

    // True while ANY of our own built-in screens is up (panel, skill tree, shop). The
    // "don't act over a live screen" guards all want this rather than the individual
    // predicates: written out by hand, each new screen has to be remembered at every
    // call site, and the shop was in fact missed at all four until this existed.
    // Note this covers the ImGui screens only — the PrismaUI equivalent is
    // Prisma::IsBusy(), and callers that care about both check each.
    [[nodiscard]] bool IsSystemScreenOpen();

    // ESC behaviour: dismiss the panel as if its single text button (CLOSE/CONTINUE)
    // had been clicked. Panels with an actual decision to make (several text buttons,
    // like the blessing) ignore this — ESC must not choose for the player.
    void DismissSystemWindow();

    // Draw the panel. Called by the overlay once per frame, inside an ImGui frame.
    void DrawSystemWindow();
}
