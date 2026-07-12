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

    // Pay out anything the player already earned before the mod was installed (or
    // while it was disabled). Call on kPostLoadGame / kNewGame, after the co-save
    // state has been read back.
    void CatchUpOnLoad();

    // Passives the player currently holds — for the status panel.
    [[nodiscard]] std::vector<const Passive*> EarnedPassives();
}
