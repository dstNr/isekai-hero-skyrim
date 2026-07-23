#pragma once

// Optional PrismaUI (web/HTML) renderer for the WHOLE Isekai UI — the skill tree, the
// System dialog panels (blessing choice, status ledger, milestone payouts) and the
// level-up flourish. Entirely runtime-gated: if the PrismaUI framework is not loaded,
// or the view files (the optional patch) are not installed, this stays dormant and the
// built-in ImGui UI is used instead. The mod has NO build- or load-time dependency on
// PrismaUI.
//
// One PrismaUI view, several "screens" toggled by the JS side. Data stays single-source
// in the plugin (SkillTree / the panel's own callback); only the renderer is duplicated.

#include <functional>
#include <string>
#include <vector>

#include "UI/SystemWindow.h"  // Choice

namespace Isekai::UI::Prisma {

    // At kDataLoaded: request the PrismaUI API and, if present AND the view files exist,
    // create the view and wire the JS listeners. Safe when neither is there.
    void Install();

    // True when the web UI is usable — PrismaUI loaded, patch installed, view valid.
    // Every ImGui entry point checks this to choose web vs. ImGui.
    [[nodiscard]] bool Active();

    // True while a focused/paused screen (tree or dialog) is up.
    [[nodiscard]] bool IsBusy();

    // --- Skill tree ---
    void OpenTree();  // main thread; no-op if !Active()

    // --- Dialog panel (mirrors ShowSystemWindow) ---
    void ShowPanel(std::string a_title, std::string a_body, std::vector<Choice> a_choices,
                   std::function<void(int)> a_onSelect, float a_revealCharsPerSec, float a_width);

    // --- Level-up flourish (non-interactive overlay, no pause) ---
    void Flourish(std::string a_title, std::string a_subtitle);
}
