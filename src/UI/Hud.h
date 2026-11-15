#pragma once

#include <string>

// The hud producer: toasts, the level-up flourish and the threat labels, sent to the hud
// view (magelight/IsekaiHero/hud.html) through UI/Magelight.h. The page owns every
// timing (toast life, key replacement, the flourish); this side only says what to show.
//
// Toasts and the flourish go out at once, from any thread: the hud page shows them while
// the game is paused (a purchase toast over the shop) and in the free camera, where the
// frame tick does not run. Threat labels come from the frame tick (FrameTick.h) and stop
// with it; the page hides them 300 ms after the last labels.* call.
//
// Without Magelight UI, toasts fall back to the game's own corner notification.

namespace Isekai::UI {

    // Post a line. Any thread. An empty string is ignored.
    //
    // a_key groups related messages: posting with a key already on screen REPLACES that
    // line and restarts its timer, instead of stacking a second one. Quest progress uses
    // it, so twenty kills leave one counter counting up rather than twenty toasts.
    // Pass an empty key for a standalone message.
    void ShowToast(std::string a_text, std::string a_key = {});

    // A toast that stays longer and reads as an announcement rather than a tally — the
    // System handing out a new objective, or paying one out.
    void ShowToastBanner(std::string a_text, std::string a_key = {});

    // Fire the milestone flourish: rings, a title punch, an edge pulse, gold. Purely
    // visual — it does not pause the game or take input. Main thread (it also starts the
    // level-up sting, delayed past a menu-pause resume).
    void PlayLevelUpEffect(std::string a_title, std::string a_subtitle);

    namespace Hud {

        // kDataLoaded, main thread, after Magelight::Install and Screens::Install. Sets the
        // hud prelude (Screens::SettingsCalls, plus isekai.vr(true) in Skyrim VR), and with
        // Magelight UI present adds the threat-label pass to the frame tick and a menu
        // watcher that clears the labels as a pausing menu or the console opens.
        void Install();

    }  // namespace Hud

}  // namespace Isekai::UI
