#pragma once

// Core of the Isekai "System". Ported from the Papyrus version's data model
// (see papyrus/Scripts/Source). Native C++ / CommonLibSSE-NG implementation.

namespace Isekai {

    // The reincarnated soul's world of origin.
    enum class OriginWorld {
        Earth,
        Japan,
        Korea,
        Fantasy,
        SciFi,
        Apocalyptic,
    };

    // How much power the System grants on reincarnation.
    enum class PowerLevel {
        Normal,    // no boost, pure challenge
        Hero,      // maxed skills, some perks
        Ascended,  // maxed everything, level 255, many perks
    };

    // Which skills the past life mastered.
    enum class SkillFocus {
        Balanced,
        Warrior,
        Mage,
        Thief,
        Custom,
    };

    // Player choices + runtime flags. One instance per game.
    struct State {
        bool        reincarnated = false;
        OriginWorld origin = OriginWorld::Earth;
        PowerLevel  power = PowerLevel::Normal;
        SkillFocus  skills = SkillFocus::Balanced;
    };

    [[nodiscard]] State& GetState();

    // Hook into SKSE's lifecycle (new game / load / data loaded).
    void RegisterMessageListener();
}
