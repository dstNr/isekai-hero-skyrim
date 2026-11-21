#include "Voice.h"

#include "Config.h"
#include "Plugin.h"
#include "System.h"  // DelayedMainThread
#include "VoiceLogic.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
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

        std::array<RE::BGSSoundDescriptorForm*, kFormIDs.size()> g_descriptors{};

        // Gate state, touched by Say (any thread) and ResetSession.
        std::mutex g_lock;
        double g_lastAt = -std::numeric_limits<double>::infinity();
        int g_lastPriority = 0;
        std::uint64_t g_generation = 0;  // bumped per accepted line

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
                return 3;
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

        void Speak(RE::BGSSoundDescriptorForm* a_descriptor, int a_priority, Line a_line) {
            auto* audio = RE::BSAudioManager::GetSingleton();
            if (!audio) {
                return;
            }
            if (g_handle.IsValid() && g_handle.IsPlaying()) {
                // Only an outranked line is still running here: the gate let this one
                // through, so either it matters more or the gap has passed. A lower line
                // never cuts a higher one off mid-sentence.
                if (a_priority < g_handlePriority) {
                    return;
                }
                g_handle.Stop();
            }
            RE::BSSoundHandle handle;
            if (!audio->GetSoundHandle(handle, a_descriptor) || !handle.Play()) {
                logger::warn("Voice: engine refused line {}", static_cast<int>(a_line));
                return;
            }
            g_handle = handle;
            g_handlePriority = a_priority;
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
        if (!Config::SystemVoice()) {
            return;
        }
        const auto index = static_cast<std::size_t>(a_line);
        auto* descriptor = index < g_descriptors.size() ? g_descriptors[index] : nullptr;
        if (!descriptor) {
            return;
        }
        const int priority = PriorityOf(a_line);
        std::uint64_t generation = 0;
        {
            std::scoped_lock lock(g_lock);
            const double now = NowSeconds();
            if (!Logic::ShouldSpeak(now, g_lastAt, g_lastPriority, priority, Logic::kMinGapSeconds)) {
                return;
            }
            g_lastAt = now;
            g_lastPriority = priority;
            generation = ++g_generation;
        }
        DelayedMainThread(std::max(a_delayMs, Sounds::kResumeGraceMs),
                          [descriptor, priority, a_line, generation]() {
                              {
                                  std::scoped_lock lock(g_lock);
                                  // Superseded by a later line that matters as much, or by a load.
                                  if (generation != g_generation && g_lastPriority >= priority) {
                                      return;
                                  }
                              }
                              Speak(descriptor, priority, a_line);
                          });
    }

    void ResetSession() {
        std::scoped_lock lock(g_lock);
        // The gap is over at once; the highest priority there is drops every line still
        // waiting from the old session (the next Say passes on the gap alone).
        g_lastAt = -std::numeric_limits<double>::infinity();
        g_lastPriority = std::numeric_limits<int>::max();
        ++g_generation;
    }
}
