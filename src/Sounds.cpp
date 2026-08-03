#include "Sounds.h"

#include "Plugin.h"
#include "System.h"  // DelayedMainThread

#include <array>

namespace Isekai::Sounds {

    namespace {
        // Local FormIDs of the SNDR records in IsekaiHero.esp, in Sfx order.
        //
        // Read straight out of the plugin file (the SNDR records carry their editor
        // ID and WAV path right there), so each ID below is provably the sound it
        // claims to be. Renumbered when the plugin was compacted into the ESL range:
        //
        //   0x000D7B  IsekaiSND_LevelUp      fx\isekai\Cinematic_6_1.wav
        //   0x000D7C  IsekaiSND_WindowOpen   fx\isekai\Cinematic_7_2.wav
        //   0x000D7D  IsekaiSND_ButtonClick  fx\isekai\Modern_2_2.wav
        //   0x000D7E  IsekaiSND_WindowClose  fx\isekai\Modern_5_2.wav
        constexpr std::array<RE::FormID, 4> kFormIDs = {
            0x000D7B,  // LevelUp
            0x000D7C,  // WindowOpen
            0x000D7D,  // ButtonClick
            0x000D7E,  // WindowClose
        };

        std::array<RE::BGSSoundDescriptorForm*, 4> g_descriptors{};
    }

    void Install() {
        g_descriptors.fill(nullptr);

        if (!Plugin::IsLoaded()) {
            return;
        }
        std::size_t resolved = 0;
        for (std::size_t i = 0; i < kFormIDs.size(); ++i) {
            if (kFormIDs[i] == 0) {
                continue;  // not wired up yet
            }
            g_descriptors[i] = Plugin::LookupOurForm<RE::BGSSoundDescriptorForm>(kFormIDs[i]);
            if (g_descriptors[i]) {
                ++resolved;
            } else {
                logger::error("Sounds: no descriptor {:#08x} in {}", kFormIDs[i],
                              Plugin::kFileName);
            }
        }

        if (resolved > 0) {
            logger::info("Sounds: {} of {} descriptors resolved", resolved, kFormIDs.size());
        } else {
            logger::info("Sounds: no descriptors wired yet — System stays silent");
        }
    }

    std::pair<std::size_t, std::size_t> Resolved() {
        std::size_t n = 0;
        for (auto* d : g_descriptors) {
            if (d) {
                ++n;
            }
        }
        return { n, g_descriptors.size() };
    }

    void Play(Sfx a_sfx) {
        auto* descriptor = g_descriptors[static_cast<std::size_t>(a_sfx)];
        if (!descriptor) {
            return;
        }
        auto* audio = RE::BSAudioManager::GetSingleton();
        if (!audio) {
            return;
        }
        // The return value matters for once: a refused sound and a sound that
        // started and was then cut short are indistinguishable by ear, and we had
        // to tell them apart to track down a missing level-up sting.
        if (!audio->Play(descriptor)) {
            logger::warn("Sounds: engine refused sfx {}", static_cast<int>(a_sfx));
        }
    }

    void PlayDelayed(Sfx a_sfx, std::uint32_t a_ms) {
        DelayedMainThread(a_ms, [a_sfx]() { Play(a_sfx); });
    }
}
