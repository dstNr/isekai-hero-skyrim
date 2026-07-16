#pragma once

// Core of the Isekai "System". Ported from the Papyrus version's data model
// (see papyrus/Scripts/Source). Native C++ / CommonLibSSE-NG implementation.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Isekai {

    // Run fn on the main thread after a_ms. Game and UI calls must happen on the main
    // thread, so a detached timer marshals back through SKSE's task interface.
    void DelayedMainThread(std::uint32_t a_ms, std::function<void()> a_fn);

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
        bool       reincarnated = false;
        PowerLevel power = PowerLevel::Normal;
        SkillFocus skills = SkillFocus::Balanced;

        // Milestones already paid out, by their stable key. Guards against paying
        // twice — quest stage events fire more than once, and the retroactive
        // catch-up runs on every load.
        std::vector<std::uint32_t> grantedMilestones;

        // The dimensional storage chest reference, created at runtime on first use
        // (see Storage.cpp). 0 = not created yet.
        std::uint32_t storageChest = 0;

        // Skill tree nodes bought with dragon souls, by their stable key.
        std::vector<std::uint32_t> unlockedNodes;
    };

    [[nodiscard]] State& GetState();

    // "NORMAL" / "HERO" / "ASCENDED" — for panels and logs.
    [[nodiscard]] std::string PowerName(PowerLevel a_power);

    // Multiplier on every milestone reward — passives, dragon souls, the lot.
    // The blessing taken at the start is not a one-off head start: it decides how
    // fast the System keeps feeding you for the rest of the run.
    [[nodiscard]] float RewardScale();

    // Perk points a milestone endpoint pays out. Scaled to the blessing taken at the
    // start, so that choice keeps mattering for the whole playthrough.
    [[nodiscard]] std::int32_t MilestonePerkPoints();

    // Add perk points, clamped to what the engine can actually hold (127).
    void GrantPerkPoints(std::int32_t a_points);

    // Add unspent dragon souls. Unlike perk points these have no engine cap.
    void GrantDragonSouls(std::int32_t a_souls);

    // Install everything: co-save serialization (persist choices + the
    // "already reincarnated" flag) and a start-method-independent trigger that
    // fires the first time the player is actually in control in the world
    // (works with vanilla, coc, Alternate Start, Skyrim Unbound, ...).
    void Install();
}
