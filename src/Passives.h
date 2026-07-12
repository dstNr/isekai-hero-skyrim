#pragma once

namespace Isekai::Passives {

    // Resolve the ability spells out of IsekaiHero.esp. Call at kDataLoaded.
    void Install();

    // Recompute every passive from the milestones earned so far and push the totals
    // into the abilities on the player.
    //
    // Safe to call as often as you like: the bonuses are *derived*, never accumulated.
    // That is the whole point of doing it this way rather than nudging base actor
    // values — a double-apply is impossible by construction, because we always write
    // the total rather than adding a delta.
    void Refresh();
}
