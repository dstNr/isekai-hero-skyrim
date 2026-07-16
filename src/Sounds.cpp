#include "Sounds.h"

#include "Plugin.h"

#include <array>

namespace Isekai::Sounds {

    namespace {
        // Local FormIDs of the SNDR records in IsekaiHero.esp, in Sfx order.
        //
        // Read straight out of the plugin file (the SNDR records carry their editor
        // ID and WAV path right there), so each ID below is provably the sound it
        // claims to be — no reliance on creation order:
        //
        //   0x001DBF  IsekaiSND_LevelUp      fx\isekai\Cinematic_6_1.wav
        //   0x001DC0  IsekaiSND_WindowOpen   fx\isekai\Cinematic_7_2.wav
        //   0x001DC1  IsekaiSND_ButtonClick  fx\isekai\Modern_2_2.wav
        //   0x001DC2  IsekaiSND_WindowClose  fx\isekai\Modern_5_2.wav
        constexpr std::array<RE::FormID, 4> kFormIDs = {
            0x001DBF,  // LevelUp
            0x001DC0,  // WindowOpen
            0x001DC1,  // ButtonClick
            0x001DC2,  // WindowClose
        };

        std::array<RE::BGSSoundDescriptorForm*, 4> g_descriptors{};
    }

    void Install() {
        g_descriptors.fill(nullptr);

        if (!Plugin::IsLoaded()) {
            return;
        }
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            return;
        }

        std::size_t resolved = 0;
        for (std::size_t i = 0; i < kFormIDs.size(); ++i) {
            if (kFormIDs[i] == 0) {
                continue;  // not wired up yet
            }
            g_descriptors[i] =
                data->LookupForm<RE::BGSSoundDescriptorForm>(kFormIDs[i], Plugin::kFileName);
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

    void Play(Sfx a_sfx) {
        auto* descriptor = g_descriptors[static_cast<std::size_t>(a_sfx)];
        if (!descriptor) {
            return;
        }
        if (auto* audio = RE::BSAudioManager::GetSingleton()) {
            audio->Play(descriptor);
        }
    }
}
