#pragma once

// Optional PrismaUI (web/HTML) renderer for the skill tree. Entirely runtime-gated:
// if the PrismaUI framework is not loaded, or the view files (the optional patch) are
// not installed, this stays dormant and the ImGui skill tree is used instead. The mod
// has NO build- or load-time dependency on PrismaUI.
//
// Data stays single-source in SkillTree.cpp: the web view is handed the tree as JSON
// and calls back into SkillTree::TryUnlock. Only the renderer is duplicated.

namespace Isekai::UI::PrismaTree {

    // At kDataLoaded: request the PrismaUI API and, if present AND the view files exist,
    // create the view and wire the JS listeners. Safe to call when neither is there.
    void Install();

    // True when the web tree is usable — PrismaUI loaded, patch installed, view valid.
    // The skill-tree open path uses this to choose web vs. ImGui.
    [[nodiscard]] bool Active();

    // True while the web tree is on screen (focused/paused).
    [[nodiscard]] bool IsOpen();

    // Show + focus the web tree (pauses the game). Main thread only. No-op if !Active().
    void Open();

    // Unfocus + hide the web tree (unpauses). Main thread only.
    void Close();
}
