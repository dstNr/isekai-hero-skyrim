#pragma once

// The non-combat half of the game, feeding the System.
//
// Harvesting, smelting, tanning, smithing, alchemy and enchanting all count as one
// "profession" track: every ProfessionActionsPerPoint of them pays a System Point.
// Deliberately one track rather than a skill per station — the System rewards that the
// character is *working*, and splitting it would need a UI to show six numbers nobody
// asked for.

namespace Isekai::Professions {

    // Register the harvest and craft sinks. Call once, after kDataLoaded. A no-op when
    // ProfessionActionsPerPoint is 0, which is how the feature is switched off.
    void Install();
}
