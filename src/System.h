#pragma once

// Core of the Isekai "System". Ported from the Papyrus version's data model
// (see papyrus/Scripts/Source). Native C++ / CommonLibSSE-NG implementation.

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Isekai {

    // The engine's true cap on unspent perk points. CommonLibSSE declares the field
    // as int8, but the game treats it as UNSIGNED: a tester holding 202 points in the
    // vanilla menu read back as -54 through the signed lens. The cap is 255, and every
    // read must go through an unsigned cast.
    inline constexpr std::int32_t kMaxPerkPoints = 255;

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

        // "Shattered" awakening: keep the blessing's TIER (reward scale + the deeper
        // skill tree it unlocks) but skip the flat starting grant — skills, level,
        // gear, gold, the seeded points. A HERO/ASCENDED who wants the higher ceiling
        // without the handed-out floor earns every step at the blessing's pace.
        bool shattered = false;

        // "Dormant" awakening: the tier itself is not chosen up front but grows with
        // the character. `power` above stays the tier held RIGHT NOW — everything that
        // reads it (RewardScale, MilestonePerkPoints, how deep the skill tree opens)
        // therefore needs no special case — and CheckDormantAwakening() raises it as
        // the character levels: NORMAL, then HERO, then ASCENDED at the thresholds in
        // the ini. Orthogonal to `shattered`, which still decides whether each
        // awakening also hands over its flat grant.
        bool dormant = false;

        // The three blessing axes, normally locked together by the tier but separable on
        // the CUSTOM path. `power` above stays the REWARD tier (reward scale + milestone
        // perk points); these two split off the other two things a tier used to bundle:
        //   grantTier — which flat starting grant is handed over (NORMAL = none, i.e. the
        //               same "no floor" a SHATTERED start gives).
        //   treeTier  — how deep the skill tree opens (which node tiers are unlockable).
        // For every preset (and each dormant awakening) these are set to match power (or
        // NORMAL for a shattered start), so nothing about the presets changes. Only CUSTOM
        // sets them apart — e.g. the whole tree open (treeTier ASCENDED) on a NORMAL reward
        // pace, which self-balances because System Points are still earned at ×1. `custom`
        // just tags such a build so the panels can label it.
        PowerLevel grantTier = PowerLevel::Normal;
        PowerLevel treeTier = PowerLevel::Normal;
        bool       custom = false;

        // Milestones already paid out, by their stable key. Guards against paying
        // twice — quest stage events fire more than once, and the retroactive
        // catch-up runs on every load.
        std::vector<std::uint32_t> grantedMilestones;

        // The dimensional storage chest reference, created at runtime on first use
        // (see Storage.cpp). 0 = not created yet.
        std::uint32_t storageChest = 0;

        // Skill tree nodes bought with System Points, by their stable key. One-shot
        // nodes only — each appears at most once.
        std::vector<std::uint32_t> unlockedNodes;

        // Purchase counts for REPEATABLE nodes (incremental stat boosts), by node key.
        // These never enter unlockedNodes (so they stay buyable), so their rank — how
        // many times bought — is tracked here and drives the accumulated bonus.
        std::vector<std::pair<std::uint32_t, std::int32_t>> nodeRanks;

        // The System's own currency (docs/IDEAS.md): uncapped, unlike perk points,
        // and deliberately separate from dragon souls. Paid by milestones; souls can
        // be converted into it as their post-main-quest sink.
        std::int32_t systemPoints = 0;
    };

    [[nodiscard]] State& GetState();

    // "NORMAL" / "HERO" / "ASCENDED" — for panels and logs.
    [[nodiscard]] std::string PowerName(PowerLevel a_power);

    // Re-open the blessing choice on an existing, already-reincarnated character —
    // the "reboot the System" the panel offers. Only the blessing is re-chosen; every
    // earned thing (milestones, skill-tree nodes/ranks, System Points, the storage
    // chest) is kept. A previous FULL blessing's flat stats are NOT clawed back — we
    // track no deltas, so a true clean slate remains a new game. Main thread only.
    void RebootSystem();

    // Raise a DORMANT blessing to whatever tier the character's level has earned.
    // Cheap and idempotent: a no-op unless the player is dormant AND has crossed a
    // threshold they have not been paid for. Called wherever the level may have
    // changed — every menu close, and on load.
    void CheckDormantAwakening();

    // The character level at which a DORMANT blessing next rises, or 0 when the
    // player is not dormant / already ASCENDED. For the status panel.
    [[nodiscard]] std::uint16_t NextAwakeningLevel();

    // The level at which a DORMANT blessing reaches a_tier, or 0 for a player who is
    // not dormant. Lets the skill tree say "awakens at level 25" instead of "requires
    // a HERO rebirth" — for these players the seal is a timer, not a closed door.
    [[nodiscard]] std::uint16_t AwakeningLevelFor(PowerLevel a_tier);

    // Multiplier on every milestone reward — passives, dragon souls, the lot.
    // The blessing taken at the start is not a one-off head start: it decides how
    // fast the System keeps feeding you for the rest of the run.
    [[nodiscard]] float RewardScale();

    // Perk points a milestone endpoint pays out. Scaled to the blessing taken at the
    // start, so that choice keeps mattering for the whole playthrough.
    [[nodiscard]] std::int32_t MilestonePerkPoints();

    // Add perk points, clamped to what the engine can actually hold (kMaxPerkPoints).
    void GrantPerkPoints(std::int32_t a_points);

    // Add unspent dragon souls. Unlike perk points these have no engine cap.
    void GrantDragonSouls(std::int32_t a_souls);

    // Add System Points (never negative-clamps; spending happens in SkillTree).
    void GrantSystemPoints(std::int32_t a_points);

    // System Points a milestone pays: 1 base, 5 at endpoints, times RewardScale.
    [[nodiscard]] std::int32_t MilestoneSystemPoints(bool a_endpoint);

    // Install everything: co-save serialization (persist choices + the
    // "already reincarnated" flag) and a start-method-independent trigger that
    // fires the first time the player is actually in control in the world
    // (works with vanilla, coc, Alternate Start, Skyrim Unbound, ...).
    void Install();
}
