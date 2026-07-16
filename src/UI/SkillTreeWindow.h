#pragma once

namespace Isekai::UI {

    // Open the skill tree. Holds the game like every System panel. Main thread only.
    void ShowSkillTree();

    // True while the tree is up — i.e. while it should own mouse and keyboard.
    [[nodiscard]] bool IsSkillTreeOpen();

    // ESC behaviour: close the tree (same as the CLOSE button).
    void DismissSkillTree();

    // Draw it. Called by the overlay once per frame, inside an ImGui frame.
    void DrawSkillTree();
}
