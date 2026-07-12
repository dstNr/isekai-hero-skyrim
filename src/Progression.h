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
        const char*    effect;  // human-readable, e.g. "+20 Health"
        RE::ActorValue actorValue;
        float          amount;
    };

    // Register the quest watcher. Call once, at kDataLoaded.
    void Install();

    // Pay out anything the player already earned before the mod was installed (or
    // while it was disabled). Call on kPostLoadGame / kNewGame, after the co-save
    // state has been read back.
    void CatchUpOnLoad();

    // Passives the player currently holds — for the status panel.
    [[nodiscard]] std::vector<const Passive*> EarnedPassives();
}
