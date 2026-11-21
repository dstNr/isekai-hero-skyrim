#pragma once

#include "Sounds.h"  // kResumeGraceMs

#include <cstddef>
#include <cstdint>
#include <utility>

namespace Isekai::Voice {

    // The System's spoken lines. Each one is a Sound Descriptor in IsekaiHero.esp that
    // lists every recorded variant; the engine picks one at random. Same order as the
    // FormID table in Voice.cpp.
    enum class Line {
        Reincarnation,
        BlessingNormal,
        BlessingHero,
        BlessingAscended,
        BlessingDormant,
        BlessingCustom,
        LevelUp,
        Milestone,
        RankD,
        RankC,
        RankB,
        RankA,
        RankS,
        QuestNew,
        QuestDone,
        Dormant,
        Reboot,
        Lethal,
        Guild,
        CoSaveLost,
    };

    // Resolve the descriptors out of the ESP. Call at kDataLoaded.
    void Install();

    // How many of the voice descriptors resolved, and how many were expected.
    [[nodiscard]] std::pair<std::size_t, std::size_t> Resolved();

    // Speak a line a_delayMs from now (never sooner than Sounds::kResumeGraceMs, for the
    // reason given there). Quietly does nothing when the switch is off, the descriptor is
    // missing, or a line spoke less than Logic::kMinGapSeconds ago and this one does not
    // outrank it (top-priority lines - reincarnation, blessings, reboot, lost cosave - pass
    // each other). At the speakers a strictly higher line cuts the running one off, an equal
    // one waits for it to finish (up to ~10 s), a lower one is dropped.
    void Say(Line a_line, std::uint32_t a_delayMs = Sounds::kResumeGraceMs);

    // The System rank now (IsekaiHeroAPI::Rank: E = 0, D = 1 .. S = 5), from wherever it is
    // published. Says RankD..RankS only when the rank rose from one already seen this
    // session, so the first rank after a load or new game is silent. Any value is recorded;
    // one without a line (E, out of range) is never spoken.
    void OnRank(int a_rank);

    // The lethal-threat warning, at most once per Logic::kLethalCooldownSeconds. The
    // cooldown starts only when the line passes the gap gate, so a swallowed warning does
    // not silence the next one.
    void SayLethal();

    // Load or new game: forget the last line, the rank and the lethal cooldown, drop every
    // line still pending or queued and stop the one playing, so nothing from the old
    // session gates or plays into the new one. Main thread only.
    void ResetSession();
}
