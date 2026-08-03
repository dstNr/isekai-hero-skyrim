#pragma once

#include <cstddef>
#include <utility>

namespace Isekai::Sounds {

    // The mod's own sound effects, backed by Sound Descriptor records in
    // IsekaiHero.esp (which route them through the game's audio system, so the
    // in-game volume sliders apply — unlike playing the WAVs raw).
    enum class Sfx {
        LevelUp,      // milestone flourish / reincarnation (replaces UILevelUp)
        WindowOpen,   // a System panel opens
        ButtonClick,  // a choice is taken in a System panel
        WindowClose,  // a System panel is dismissed
    };

    // Resolve the descriptors out of the ESP. Call at kDataLoaded.
    void Install();

    // How many of the ESP's sound descriptors resolved, and how many were expected.
    // A missing one is silent by design (the System just plays nothing), so the
    // self-test reports the ratio rather than leaving it to the load log.
    [[nodiscard]] std::pair<std::size_t, std::size_t> Resolved();

    // Fire and forget. Quietly does nothing while the descriptors are not wired up
    // yet, so call sites never need to care.
    void Play(Sfx a_sfx);

    // Play a_ms from now, back on the main thread.
    //
    // Needed whenever a sound follows a PrismaUI view losing focus: that unfocus
    // takes the game out of menu-pause, and the engine's resume pass discards
    // sounds started in the same frame — the level-up sting simply never reached
    // the speakers. A grace period of kResumeGraceMs sidesteps it. The ImGui UI
    // never pauses the game, so it has no such problem and calls Play directly.
    void PlayDelayed(Sfx a_sfx, std::uint32_t a_ms);

    // Long enough to land in a later frame than the unpause, short enough that
    // nobody hears the sound as late.
    inline constexpr std::uint32_t kResumeGraceMs = 150;
}
