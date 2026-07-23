#pragma once

// Lightweight INI settings, read once at kDataLoaded from
// Data\SKSE\Plugins\IsekaiHero.ini. Absent file / keys fall back to defaults, so
// the mod runs fine with no ini at all.

namespace Isekai::Config {

    // Parse the ini (idempotent). Call at kDataLoaded.
    void Load();

    // Skill tree: hide nodes gated above the player's rebirth tier instead of showing
    // them greyed with a "requires HERO/ASCENDED" hint. Default false (show greyed).
    [[nodiscard]] bool HideSealedNodes();
}
