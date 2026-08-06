#pragma once

// System Quests — the System hands out a standing objective ("Slay 12 Draugr") and pays
// System Points for it.
//
// Why this exists: milestones were the only SOURCE of System Points, and there are a
// finite 79 of them. Meanwhile the sinks kept growing — the skill tree, then the shop's
// material packs, gold and potions, and the Dimensional Storage no longer arrives
// pre-stocked. Quests give points an income that comes from playing rather than from
// finishing named quests, which is what keeps the shop meaningful for a whole run.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Isekai::Quests {

    // Every quarry as (display name, ActorType keyword editor ID), for the self-test to
    // confirm against the running load order. These IDs are typed by hand, and a wrong
    // one fails silently — the objective is simply never satisfiable, with no error
    // anywhere. Checking them is the single most valuable thing the self-test does.
    [[nodiscard]] std::vector<std::pair<const char*, const char*>> QuarryKeywords();

    // Register the death-event sink and make sure a character that should have an
    // objective has one. Call at kDataLoaded.
    void Install();

    // Hand out the next objective if one is due. Cheap and idempotent — safe to call as
    // often as convenient, which is how it gets called: the overlay polls it, and so does
    // every load. There is no per-frame main-thread hook in an SKSE plugin, so "the
    // System offers work on its own" has to be assembled out of the events we do get.
    void Tick();

    // Game days until the next objective, or 0 when one is already running or due now.
    // The status panel shows it, so an idle System reads as "waiting" rather than broken.
    [[nodiscard]] float DaysUntilNext();

    // Roll a fresh objective if the player is reincarnated and has none. Called after
    // the reincarnation and on every load, so a save from before this feature (or one
    // whose objective was somehow lost) picks one up without ceremony.
    void EnsureObjective();

    // True when there is an objective to show. False for a character that never
    // reincarnated — the System hands out no work before it has bound itself.
    [[nodiscard]] bool Active();

    // "Slay 12 Draugr" — the objective as a sentence, or "" when inactive.
    [[nodiscard]] std::string Text();

    // Progress toward the current objective, for a counter or a bar. Both 0 when
    // inactive.
    [[nodiscard]] std::int32_t Progress();
    [[nodiscard]] std::int32_t Target();

    // What completing the current objective pays, already scaled by the blessing.
    [[nodiscard]] std::int32_t Reward();

    // Drop the current objective and roll a different one. Costs nothing — the point of
    // a standing objective is that it fits how you are already playing, so being stuck
    // with a bad draw would just make the feature annoying.
    void Reroll();
}
