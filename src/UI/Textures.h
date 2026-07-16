#pragma once

#include <imgui.h>

#include <string>

namespace Isekai::UI {

    // PNG -> D3D11 texture, cached by path. Returns 0 if the file is missing or
    // unreadable (logged once per path, then cached as a miss so a bad path cannot
    // spam the log at frame rate).
    //
    // Render thread only: creation needs the game's D3D device mid-frame, and the
    // returned id is only meaningful inside an ImGui frame anyway.
    [[nodiscard]] ImTextureID GetTexture(const std::string& a_path);
}
