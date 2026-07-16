#include "Sounds.h"

#include "Plugin.h"

#include <array>

namespace Isekai::Sounds {

    namespace {
        // Local FormIDs of the SNDR records in IsekaiHero.esp, in Sfx order.
        //
        // Zero = not created yet; Play() then just stays silent. Sound descriptors
        // carry neither an editor ID nor a name at runtime, and their file paths are
        // stored as CRC hashes — so the records are mapped by creation order and the
        // IDs filled in from the form dump, like the ability spells before them.
        //
        // CK creation order (see docs/CREATION_KIT_ESP.md, Teil E):
        //   1. LevelUp      Cinematic_6_1.wav
        //   2. WindowOpen   Cinematic_7_2.wav
        //   3. ButtonClick  Modern_2_2.wav
        //   4. WindowClose  Modern_5_2.wav
        constexpr std::array<RE::FormID, 4> kFormIDs = {
            0x000000,  // LevelUp
            0x000000,  // WindowOpen
            0x000000,  // ButtonClick
            0x000000,  // WindowClose
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
