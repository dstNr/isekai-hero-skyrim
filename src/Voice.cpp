#include "Voice.h"

#include "Config.h"
#include "Plugin.h"
#include "System.h"  // DelayedMainThread
#include "VoiceLogic.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>

namespace Isekai::Voice {

    namespace {
        // Local FormIDs of the voice SNDR records in IsekaiHero.esp, in Line order.
        // Each lists its variants as fx\isekai\voice\<name>_<nn>.wav (docs/CREATION_KIT_ESP.md
        // Part N); tools/check.mjs reads this table and checks it against the plugin.
        constexpr std::array<RE::FormID, 20> kFormIDs = {
            0x000DB7,  // IsekaiVOX_reincarnation
            0x000DB8,  // IsekaiVOX_blessing_normal
            0x000DB9,  // IsekaiVOX_blessing_hero
            0x000DBA,  // IsekaiVOX_blessing_ascended
            0x000DBB,  // IsekaiVOX_blessing_dormant
            0x000DBC,  // IsekaiVOX_blessing_custom
            0x000DBD,  // IsekaiVOX_levelup
            0x000DBE,  // IsekaiVOX_milestone
            0x000DBF,  // IsekaiVOX_rank_d
            0x000DC0,  // IsekaiVOX_rank_c
            0x000DC1,  // IsekaiVOX_rank_b
            0x000DC2,  // IsekaiVOX_rank_a
            0x000DC3,  // IsekaiVOX_rank_s
            0x000DC4,  // IsekaiVOX_quest_new
            0x000DC5,  // IsekaiVOX_quest_done
            0x000DC6,  // IsekaiVOX_dormant
            0x000DC7,  // IsekaiVOX_reboot
            0x000DC8,  // IsekaiVOX_lethal
            0x000DC9,  // IsekaiVOX_guild
            0x000DCA,  // IsekaiVOX_cosave_lost
        };
        static_assert(kFormIDs.size() == static_cast<std::size_t>(Line::CoSaveLost) + 1);
        static_assert(static_cast<int>(Line::RankS) - static_cast<int>(Line::RankD) ==
                      Logic::kRankS - Logic::kRankD);

        std::array<RE::BGSSoundDescriptorForm*, kFormIDs.size()> g_descriptors{};

        // Gate state, touched by Say (any thread), OnRank, SayLethal and ResetSession.
        std::mutex g_lock;
        Logic::Gate g_gate;
        std::uint64_t g_generation = 0;  // bumped per accepted line
        std::uint64_t g_session = 0;     // bumped only by ResetSession (load, new game)

        // The line on the speakers. Main thread only (the DelayedMainThread callback).
        RE::BSSoundHandle g_handle;
        int g_handlePriority = 0;

        int PriorityOf(Line a_line) {
            switch (a_line) {
            case Line::CoSaveLost:
            case Line::Reincarnation:
            case Line::BlessingNormal:
            case Line::BlessingHero:
            case Line::BlessingAscended:
            case Line::BlessingDormant:
            case Line::BlessingCustom:
            case Line::Reboot:
                return Logic::kTopPriority;
            case Line::Dormant:
            case Line::RankD:
            case Line::RankC:
            case Line::RankB:
            case Line::RankA:
            case Line::RankS:
                return 2;
            default:
                return 1;
            }
        }

        double NowSeconds() {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        }

        // A line of the same weight waits for the running one: every 250 ms, for ~10 s.
        constexpr std::uint32_t kWaitStepMs = 250;
        constexpr int kWaitTries = 40;

        // Main thread (DelayedMainThread callback). Never throws.
        void Attempt(RE::BGSSoundDescriptorForm* a_descriptor, int a_priority, Line a_line,
                     std::uint64_t a_session, std::uint64_t a_generation, int a_triesLeft) {
            {
                std::scoped_lock lock(g_lock);
                // Checked on every retry too: a load while this line queues drops it.
                if (!Logic::PendingStillDue(a_session, g_session, a_generation, g_generation,
                                            g_gate.lastPriority, a_priority)) {
                    return;
                }
            }
            auto* audio = RE::BSAudioManager::GetSingleton();
            if (!audio) {
                return;
            }
            const bool playing = g_handle.IsValid() && g_handle.IsPlaying();
            switch (Logic::HandoffFor(playing, g_handlePriority, a_priority)) {
            case Logic::Handoff::kDrop:
                return;
            case Logic::Handoff::kWait:
                if (a_triesLeft > 0) {
                    DelayedMainThread(kWaitStepMs, [=]() {
                        Attempt(a_descriptor, a_priority, a_line, a_session, a_generation,
                                a_triesLeft - 1);
                    });
                } else {
                    logger::warn("Voice: line {} gave up waiting for the one before it",
                                 static_cast<int>(a_line));
                }
                return;
            case Logic::Handoff::kInterrupt:
                g_handle.Stop();
                break;
            case Logic::Handoff::kStart:
                break;
            }
            RE::BSSoundHandle handle;
            if (!audio->GetSoundHandle(handle, a_descriptor) || !handle.Play()) {
                logger::warn("Voice: engine refused line {}", static_cast<int>(a_line));
                return;
            }
            g_handle = handle;
            g_handlePriority = a_priority;
        }

        // Gate a line (a_lethal: through the lethal cooldown too) and schedule it.
        void Submit(Line a_line, std::uint32_t a_delayMs, bool a_lethal) {
            if (!Config::SystemVoice()) {
                return;
            }
            const auto index = static_cast<std::size_t>(a_line);
            auto* descriptor = index < g_descriptors.size() ? g_descriptors[index] : nullptr;
            if (!descriptor) {
                return;
            }
            const int priority = PriorityOf(a_line);
            std::uint64_t session = 0;
            std::uint64_t generation = 0;
            {
                std::scoped_lock lock(g_lock);
                const double now = NowSeconds();
                if (!(a_lethal ? g_gate.AcceptLethal(now, priority) : g_gate.Accept(now, priority))) {
                    return;
                }
                generation = ++g_generation;
                session = g_session;
            }
            DelayedMainThread(std::max(a_delayMs, Sounds::kResumeGraceMs),
                              [descriptor, priority, a_line, session, generation]() {
                                  Attempt(descriptor, priority, a_line, session, generation,
                                          kWaitTries);
                              });
        }
    }

    void Install() {
        g_descriptors.fill(nullptr);
        if (!Plugin::IsLoaded()) {
            return;
        }
        std::size_t resolved = 0;
        for (std::size_t i = 0; i < kFormIDs.size(); ++i) {
            g_descriptors[i] = Plugin::LookupOurForm<RE::BGSSoundDescriptorForm>(kFormIDs[i]);
            if (g_descriptors[i]) {
                ++resolved;
            } else {
                logger::error("Voice: no descriptor {:#08x} in {}", kFormIDs[i], Plugin::kFileName);
            }
        }
        logger::info("Voice: {} of {} descriptors resolved", resolved, kFormIDs.size());
    }

    std::pair<std::size_t, std::size_t> Resolved() {
        const auto n = static_cast<std::size_t>(
            std::count_if(g_descriptors.begin(), g_descriptors.end(), [](auto* d) { return d; }));
        return { n, g_descriptors.size() };
    }

    void Say(Line a_line, std::uint32_t a_delayMs) {
        Submit(a_line, a_delayMs, false);
    }

    void SayLethal() {
        Submit(Line::Lethal, Sounds::kResumeGraceMs, true);
    }

    void OnRank(int a_rank) {
        bool rose = false;
        {
            std::scoped_lock lock(g_lock);
            rose = g_gate.RankAnnounce(a_rank);  // recorded even with the voice off
        }
        if (rose) {
            Say(static_cast<Line>(static_cast<int>(Line::RankD) + a_rank - Logic::kRankD));
        }
    }

    void ResetSession() {
        {
            std::scoped_lock lock(g_lock);
            g_gate.Reset();
            ++g_session;  // every line still pending or queued from the old session is dropped
        }
        // Main thread (kPostLoadGame / kNewGame), like every other g_handle access.
        if (g_handle.IsValid() && g_handle.IsPlaying()) {
            g_handle.Stop();
        }
        g_handlePriority = 0;
    }
}
