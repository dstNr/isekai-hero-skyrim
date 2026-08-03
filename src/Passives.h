#pragma once

#include <vector>

namespace Isekai::Passives {

    // Resolve the ability spells out of IsekaiHero.esp. Call at kDataLoaded.
    void Install();

    // The actor values that actually have a backing ability spell, discovered at load
    // from each ESP magic effect's primaryAV — so this is knowable only in a running
    // game, never from the source.
    //
    // It matters because it is a silent contract: a milestone passive or an
    // Effect::kAttributes skill-tree node naming an actor value that is NOT in here
    // grants nothing at all, with no error anywhere. The self-test cross-checks the node
    // table against this list for exactly that reason.
    [[nodiscard]] std::vector<RE::ActorValue> CoveredActorValues();

    // Recompute every passive from the milestones earned so far and push the totals
    // into the abilities on the player.
    //
    // Safe to call as often as you like: the bonuses are *derived*, never accumulated.
    // That is the whole point of doing it this way rather than nudging base actor
    // values — a double-apply is impossible by construction, because we always write
    // the total rather than adding a delta.
    void Refresh();
}
