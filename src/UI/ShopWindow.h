#pragma once

// The built-in (ImGui) System Shop, used whenever the PrismaUI patch is absent — the
// same split every other screen uses. Renders Shop::Catalog() as cards; the catalog and
// the prices live in Shop.cpp, so this file only draws.

namespace Isekai::UI {

    // Open the shop. Holds the game like every System panel. Main thread only.
    void ShowShopWindow();

    // True while the shop is up — i.e. while it should own mouse and keyboard.
    [[nodiscard]] bool IsShopWindowOpen();

    // ESC behaviour: close the shop (same as the CLOSE button).
    void DismissShopWindow();

    // Draw it. Called by the overlay once per frame, inside an ImGui frame.
    void DrawShopWindow();
}
