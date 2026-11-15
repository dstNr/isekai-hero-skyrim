#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// Magelight UI host wrapper (spec 6.2, 6.4, 6.5, 8, 9): the mod "isekai-hero" with two
// fullscreen views, and the only file that talks to the Magelight C++ API.
//
//   system  Data\Magelight\IsekaiHero\system.html — every interactive screen. Panel layer,
//           hidden until RequestOpen, takes input while it holds UI mode.
//   hud     Data\Magelight\IsekaiHero\hud.html — toasts, the level-up flourish and the
//           threat labels. Click-through, always shown, Popup layer so it stays above our
//           own open screens (Layer::Hud as a fallback, see HudBelowPanels).
//
// Page messages and host events are forwarded to Screens::detail (Screens.h) on the game
// thread. Unless a comment says otherwise, every function here may be called from any
// thread; none touches RE:: state.
namespace Isekai::UI::Magelight {

    enum class View : std::uint8_t { System, Hud };

    // kDataLoaded, after Loc::Load. Acquires Magelight (host >= 1.7.0), registers the mod,
    // creates both views and their listeners. False = no Isekai Hero UI this session; one
    // log line says why (host missing or too old, a page file missing, a refused call).
    bool Install();

    // True after a successful Install and until the overlay reports RenderDead.
    bool Available();

    // The running host's version string, "" before a successful Install.
    std::string_view HostVersion();

    bool ViewCreated(View a_view);

    // The page has sent "isekai.ready" since it last (re)loaded.
    bool PageReady(View a_view);

    // The version string the page sent with its last "isekai.ready", "" before that.
    std::string PageVersion(View a_view);

    // The host refused a click-through Popup view and the hud sits on Layer::Hud, below
    // our own panels: the system page then mirrors toasts while it is open (spec 6.2).
    bool HudBelowPanels();

    // InvokeJS on the view, from ANY thread (paused game and free camera included). Before
    // the page reports ready the call is queued; once it does, the queue drains in order.
    // Callers build js only from nlohmann::json::dump() output.
    void Run(View a_view, std::string a_js);

    // RequestUIMode(system, pause | queue). True = the entry is queued: UIModeEntered or
    // UIModeRefused follows (Screens::detail::OnEntered / OnRefused).
    bool RequestOpen();

    // Withdraws an entry that has not run yet, else ReleaseUIMode. The system view is
    // hidden on UIModeExited (or at once when the entry was withdrawn).
    void Close();

    // This mod owns UI mode right now (the system view is open and focused).
    bool HoldsUiMode();

}  // namespace Isekai::UI::Magelight
