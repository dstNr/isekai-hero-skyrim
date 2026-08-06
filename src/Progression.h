#pragma once

#include <string>
#include <vector>

namespace Isekai::Progression {

    // A permanent bonus the System grants. Presented as a passive skill.
    //
    // These are NOT Skyrim ability spells: an ability would need a MagicEffect and a
    // SpellItem, i.e. forms, i.e. an ESP — or forms created at runtime, which the game
    // does not reliably persist across a save/load. So they are permanent actor value
    // changes instead, tracked here and listed in the System's own status panel.
    // Mechanically identical; they just live in our panel rather than Skyrim's
    // "Active Effects" list.
    struct Passive {
        const char*    name;
        const char*    stat;        // "Health", "Magic Resist", ...
        RE::ActorValue actorValue;
        float          baseAmount;  // before the blessing's reward scale is applied
        bool           percent;     // render as "+10% Magic Resist" rather than "+10 Health"
    };

    // What the passive is actually worth to this character, i.e. after scaling.
    [[nodiscard]] float PassiveAmount(const Passive& a_passive);

    // "+50 Health" — built from the scaled amount, never hard-coded, so the panel
    // cannot promise one number and hand out another.
    [[nodiscard]] std::string PassiveEffectText(const Passive& a_passive);

    // Register the quest watcher. Call once, at kDataLoaded.
    void Install();

    // Open the System status screen — exactly what the hotkey does, including the choice
    // between the web view and the built-in panel. Exported so another entry point (the
    // status screen's own "new task" button, which has to come back to a refreshed panel)
    // does not have to rebuild the status JSON or re-decide which renderer to use.
    // Main thread only.
    void OpenStatusPanel();

    // Pay out anything the player already earned before the mod was installed (or
    // while it was disabled). Call on kPostLoadGame / kNewGame, after the co-save
    // state has been read back.
    void CatchUpOnLoad();

    // Passives the player currently holds — for the status panel.
    [[nodiscard]] std::vector<const Passive*> EarnedPassives();

    // A derived "System Rank" (E through S) — no state of its own, just a label read off
    // milestones earned, character level and System Points ever invested in the tree.
    // Every isekai/tower-climbing story ranks its protagonist; we already track
    // everything needed to compute one, so this surfaces it as a single number instead
    // of three separate stats the player has to eyeball themselves. Feeds the status
    // panel now, and is meant to be the hook a future SkyrimNet "legend scales" decorator
    // reads instead of the coarser blessing tier (see docs/IDEAS.md).
    [[nodiscard]] std::string SystemRank();

    // Milestones whose quest editor ID resolves to nothing in the current load order,
    // by display name, plus the total milestone count.
    //
    // These are the mod's largest silent-failure surface: 79 hand-typed editor IDs, and
    // one that does not match simply never pays out — no error, no missing feature the
    // player could name, just a reward that never arrives. Install() warns about them in
    // the log, but a warning in a long log is not something a tester will spot; the
    // self-test turns it into a PASS/FAIL line.
    [[nodiscard]] std::pair<std::vector<std::string>, std::size_t> UnresolvedMilestones();

    // Every (passive title, actor value) the milestone table hands out, so the self-test
    // can check them against Passives::CoveredActorValues().
    //
    // Same silent contract as the skill tree's kAttributes nodes, and the reason this
    // exists: a passive naming an actor value with no ability spell behind it grants
    // nothing, reports nothing, and looks exactly like a milestone that simply has a
    // small effect.
    [[nodiscard]] std::vector<std::pair<const char*, RE::ActorValue>> PassiveActorValues();
}
