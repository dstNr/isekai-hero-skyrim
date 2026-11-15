#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

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
//
// Page contract (system.html and hud.html):
//   - Once its scripts can take calls, the page sends magelight.send("isekai.ready", <page
//     version>). Until then every Run for that view is queued.
//   - It defines isekai.hello(), which sends "isekai.ready" again with the same version.
//     After a reload (ViewReloaded) the wrapper resets the view to not-ready and calls
//     `window.isekai && isekai.hello && isekai.hello()` once, bypassing the queue: the
//     page's first ready may have been handled before the reload event, and would then be
//     lost. Sending ready twice is harmless.
//   - The system page sends magelight.send("isekai.action", "<type>[:<arg>]") for every
//     user action (parsed by Screens, see Logic::ParseAction).
//
// On every ready (the first one and each after a reload) the wrapper sends, in order:
// the view's prelude calls (SetPrelude), then the queued calls, then it calls
// Screens::detail::OnPageReady.
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

    // Main thread, before or after Install. a_build is called on the game thread each time
    // the view's page reports ready (initial and after every reload), outside any wrapper
    // lock; the calls it returns are sent before the queued ones. Used to push what every
    // fresh page needs first (loc table, UI scale). Each call is built only from
    // nlohmann::json::dump() output. An empty function clears it.
    void SetPrelude(View a_view, std::function<std::vector<std::string>()> a_build);

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
