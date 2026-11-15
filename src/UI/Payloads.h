#pragma once

// The JSON payloads the web renderers show: the skill tree, the shop catalogue and a
// System dialog panel. Pure builders: they read game state where the screen needs it,
// and touch no view. Every function here is MAIN THREAD ONLY.

#include <string>
#include <vector>

#include "UI/Screens.h"  // Choice
#include "UI/UiLogic.h"

namespace Isekai::UI::Payloads {

    struct PanelSpec {
        std::string         title;
        std::string         body;
        std::vector<Choice> choices;
        float               revealCharsPerSec = 40.0f;
        float               widthHint = 460.0f;
        Logic::Tone         tone = Logic::Tone::Cyan;
        std::string         emblemIcon;     // game-relative path; emitted as "icons/<file>"
        std::string         emblemCaption;  // empty = none
        Logic::Tone         emblemTone = Logic::Tone::Cyan;
    };

    // Whole tree + header. Reads game state (perk pool).
    [[nodiscard]] std::string Tree();

    // The shop catalog + the current balance. Reads game state.
    [[nodiscard]] std::string Shop();

    // A dialog panel: title, body, buttons, plus tone and optional emblem.
    [[nodiscard]] std::string Panel(const PanelSpec& a_spec);

}
