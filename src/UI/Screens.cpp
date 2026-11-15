#include "UI/Screens.h"

#include <cstddef>

// Stubs until the screens move onto the system view: each callback logs one line.
namespace Isekai::UI::Screens::detail {

    namespace {
        constexpr std::size_t kMaxLoggedAction = 200;

        std::string_view ViewName(Magelight::View a_view) {
            return a_view == Magelight::View::Hud ? "hud"sv : "system"sv;
        }
    }

    void OnAction(std::string_view a_raw) {
        logger::info("Screens: action '{}'{}", a_raw.substr(0, kMaxLoggedAction),
                     a_raw.size() > kMaxLoggedAction ? "..." : "");
    }

    void OnEntered() { logger::info("Screens: system view entered UI mode"); }

    void OnExited(bool a_reopened) {
        logger::info("Screens: UI mode ended{}", a_reopened ? " (reopened)" : "");
    }

    void OnRefused(std::string a_detail) {
        logger::info("Screens: opening the system view was refused: {}", a_detail);
    }

    void OnPageReady(Magelight::View a_view, std::string a_version) {
        logger::info("Screens: {} page ready, version '{}'", ViewName(a_view), a_version.substr(0, 64));
    }

}  // namespace Isekai::UI::Screens::detail
