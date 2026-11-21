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
    // outrank it. A line that outranks the one still playing cuts it off.
    void Say(Line a_line, std::uint32_t a_delayMs = Sounds::kResumeGraceMs);

    // Load or new game: forget the last line, so nothing from the old session gates or
    // plays into the new one.
    void ResetSession();
}
