#pragma once

#include "UI/Magelight.h"
#include "UI/UiLogic.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Screens facade over the Magelight system view (spec 6.3, 6.4, 6.5): the call-site API the
// game logic has always used, now backed by one page (system.html) with four screens —
// panel, status, tree and shop. Opening a screen pushes its JSON with
// isekai.show(<screen>, <payload>) and asks for UI mode with pause; switching between
// screens happens inside the open view. Page actions come back through
// Screens::detail::OnAction on the game thread.
//
// Unless a comment says otherwise, every function here is MAIN THREAD ONLY.
namespace Isekai::UI {

    // A panel button: a label, and optionally an icon beside it. The icon is a path whose
    // file name is looked up in the pages' icons\ folder (the folder part is dropped).
    // iconOnly buttons leave the bottom row and sit as bare icons at the panel's top-right.
    struct Choice {
        std::string label;
        std::string icon;  // empty = text only
        bool        iconOnly = false;
        std::optional<Logic::Tone> tone;  // this button's own tone; empty = the panel's
        // Escape, pad B and a lost view answer with this button, so the panel is no
        // binding decision (the reboot confirm's CANCEL).
        bool        cancel = false;
    };

    // The insignia beside the panel title: a crest and an optional caption, in a tone.
    struct Emblem {
        std::string icon;     // empty = none
        std::string caption;  // empty = crest alone
        Logic::Tone tone = Logic::Tone::Cyan;
    };

    // Open the System panel. a_onSelect receives the 0-based index of the chosen button,
    // on the main thread. a_revealCharsPerSec drives the typewriter (scaled by the
    // TextSpeed setting; a huge value is instant); a_widthHint is the panel width in
    // 1080p pixels. a_tone colours the whole window (spec 7.2).
    //
    // A panel with more than one text button is a decision: if UI mode ends without an
    // answer (refused, lost), it stays pending, is offered again once by itself, and after
    // that the system-menu hotkey brings it back (ReofferPendingDecision). A panel with one
    // text button that loses UI mode is answered with that button, so a chain of panels
    // never stops halfway.
    void ShowSystemWindow(std::string a_title, std::string a_body, std::vector<Choice> a_choices,
                          std::function<void(int)> a_onSelect, float a_revealCharsPerSec = 45.0f,
                          float a_widthHint = 720.0f, Emblem a_emblem = {},
                          Logic::Tone a_tone = Logic::Tone::Cyan);

    // Convenience for the common text-only case.
    void ShowSystemWindow(std::string a_title, std::string a_body, std::vector<std::string> a_choices,
                          std::function<void(int)> a_onSelect, float a_revealCharsPerSec = 45.0f,
                          float a_widthHint = 720.0f, Logic::Tone a_tone = Logic::Tone::Cyan);

    // The data screens. Each opens the view, or switches to its screen inside an open view.
    void ShowSkillTree();
    void ShowShop();
    void ShowStatus(std::string a_json);  // a_json: Progression::StatusJson()

    // A panel is up, or a decision panel is waiting for its answer. Any thread.
    [[nodiscard]] bool IsSystemWindowOpen();

    // One of our screens is up or asked for, or a decision panel is waiting for its
    // answer: the "don't open anything over it" guard. Any thread.
    [[nodiscard]] bool IsSystemScreenOpen();

    // The system view has ENTERED UI mode and holds it: the screen is on display and the
    // player is looking at it. A screen still queued behind another mod's UI mode does not
    // count (its keys belong to whatever is on screen), and neither does a decision waiting
    // off screen. For the input guards in Input.cpp. Any thread.
    [[nodiscard]] bool HoldsInput();

    // Escape / Tab / pad B from the game's menu chain: closes a data screen, answers a
    // panel with its cancel button or its one text button, and leaves a decision alone. Acts only
    // on the screen that is up NOW and entered (HoldsInput), on the next game-thread task,
    // so it never answers a panel the player has not seen. Any thread.
    void DismissSystemWindow();

    // Magelight UI is there and both views exist (spec 6.5). Any thread.
    [[nodiscard]] bool Available();

    // The native notification that Isekai Hero needs Magelight UI.
    void ShowNeedsMagelight();

    // A decision panel that lost UI mode is waiting: show it again and return true. The
    // system-menu hotkey calls this before it opens the status screen (and only with the
    // game running, which also lets it recover a screen whose UI mode ended unreported).
    bool ReofferPendingDecision();

    namespace Screens {

        // kDataLoaded, after Magelight::Install: sets the system view's prelude (the
        // translation table and UiScale, sent first on every page ready). The hud's is
        // set by Hud::Install, built on SettingsCalls.
        void Install();

        // The prelude calls: isekai.loc(<table>) and isekai.scale(<UiScale>). A view whose
        // prelude needs more (the hud's VR flag) builds on this.
        [[nodiscard]] std::vector<std::string> SettingsCalls();

        // After a settings reload: sends UiScale to both pages again if it changed.
        void OnSettingsReloaded();

        // kPostLoadGame / kNewGame: the loaded save has its own state, so a pending
        // decision and any screen open from before the load are dropped.
        void OnGameLoaded();

    }  // namespace Screens

}  // namespace Isekai::UI

namespace Isekai::UI::Screens::detail {

    // All of these run on the game thread (an SKSE task): RE:: state is safe here.

    // An "isekai.action" message from the system page, unparsed ("choose:2", "nav:tree").
    void OnAction(std::string_view a_raw);

    // The system view entered UI mode.
    void OnEntered();

    // UI mode ended. a_reopened: this mod already holds UI mode again (a quick reopen), so
    // the system view was left on screen.
    void OnExited(bool a_reopened);

    // A RequestOpen was not entered (a lost race, low memory); a_detail is the host's reason.
    // Delivered directly from the event, not via PostGameTask, so it can arrive before an
    // earlier OnExited.
    void OnRefused(std::string a_detail);

    // A page sent "isekai.ready"; every call queued for it has been sent.
    void OnPageReady(Magelight::View a_view, std::string a_version);

    // The system page was reloaded (ViewReloaded): its state is gone, so the next ready
    // shows the current screen again, once.
    void OnPageReloaded();

}  // namespace Isekai::UI::Screens::detail
