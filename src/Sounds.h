#pragma once

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

    // Fire and forget. Quietly does nothing while the descriptors are not wired up
    // yet, so call sites never need to care.
    void Play(Sfx a_sfx);
}
