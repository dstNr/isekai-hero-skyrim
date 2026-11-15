#pragma once

#include "UI/Magelight.h"

#include <string>
#include <string_view>

// Screens facade over the Magelight system view (spec 6.3). For now only the callbacks the
// host wrapper (Magelight.cpp) makes; the call-site API follows with the screens.
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

}  // namespace Isekai::UI::Screens::detail
