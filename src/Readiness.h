#pragma once

#include "ReadinessLogic.h"

namespace Isekai::Readiness {

    // Blocker, Name() and the decision logic live in ReadinessLogic.h, where
    // tests/readiness_tests.cpp reaches them without the engine. This is the live half.

    // True once the player is really playing: 3D loaded, not paused, past character
    // creation, not on a loading screen, and in control (so nothing fires during the
    // Helgen cart ride or any forced-walk cutscene). A pure query: it logs nothing.
    [[nodiscard]] Blocker Check();

    // Check() == kNone. Logs the blocker once each time it changes, so a player stuck
    // on one condition leaves exactly one line instead of one per menu close.
    [[nodiscard]] bool IsPlayerReady();

    // "Ready" alone is not "in the game world": character-creation start rooms hand the
    // player full control while they are still picking a face. True once the player has
    // left such a room (or never was in one).
    [[nodiscard]] bool InGameWorld();

    // Forget the first ready cell and the last logged blocker; a new game starts fresh.
    void Reset();
}
