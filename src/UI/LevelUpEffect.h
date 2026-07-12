#pragma once

#include <string>

namespace Isekai::UI {

    // Fire the milestone flourish: rings, a title punch, a cyan vignette pulse.
    // Purely visual — it does not pause the game or take input, so it can play over
    // whatever the player is doing. Call from the main thread.
    void PlayLevelUpEffect(std::string a_title, std::string a_subtitle);

    // Draw it. Called by the overlay each frame, inside an ImGui frame.
    void DrawLevelUpEffect();
}
