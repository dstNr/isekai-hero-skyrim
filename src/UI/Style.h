#pragma once

#include <imgui.h>

// Visual language of the System: cold cyan light on a deep translucent navy
// panel, hard angular edges, no rounded "app" look.
namespace Isekai::UI::Style {

    inline const ImVec4 kPanelBg{ 0.02f, 0.05f, 0.10f, 0.92f };
    inline const ImVec4 kAccent{ 0.35f, 0.80f, 1.00f, 1.00f };
    inline const ImVec4 kText{ 0.85f, 0.94f, 1.00f, 1.00f };
    inline const ImVec4 kTextDim{ 0.50f, 0.65f, 0.78f, 1.00f };

    [[nodiscard]] inline ImU32 Col(const ImVec4& a_color, float a_alphaScale = 1.0f) {
        return ImGui::GetColorU32(
            ImVec4{ a_color.x, a_color.y, a_color.z, a_color.w * a_alphaScale });
    }

    // Bloom, faked: stack progressively larger rects at falling alpha. Cheap and
    // it reads exactly like the light-bleed around a hologram panel.
    inline void DrawGlowBorder(ImDrawList* a_dl, ImVec2 a_min, ImVec2 a_max,
                               const ImVec4& a_color, float a_alphaScale = 1.0f,
                               int a_layers = 7, float a_spread = 2.0f) {
        for (int i = a_layers; i >= 1; --i) {
            const float offset = static_cast<float>(i) * a_spread;
            // Quadratic falloff — a linear ramp looks like a flat outline, not a glow.
            const float t = 1.0f - (static_cast<float>(i) / static_cast<float>(a_layers + 1));
            const float alpha = t * t * 0.30f * a_alphaScale;
            a_dl->AddRect(ImVec2{ a_min.x - offset, a_min.y - offset },
                          ImVec2{ a_max.x + offset, a_max.y + offset },
                          Col(a_color, alpha), 0.0f, 0, 1.5f);
        }
        a_dl->AddRect(a_min, a_max, Col(a_color, a_alphaScale), 0.0f, 0, 1.5f);
    }

    // Targeting-reticle brackets at the four corners.
    inline void DrawCornerBrackets(ImDrawList* a_dl, ImVec2 a_min, ImVec2 a_max,
                                   const ImVec4& a_color, float a_alphaScale = 1.0f,
                                   float a_len = 20.0f, float a_thickness = 2.5f) {
        const ImU32 col = Col(a_color, a_alphaScale);

        a_dl->AddLine(a_min, ImVec2{ a_min.x + a_len, a_min.y }, col, a_thickness);
        a_dl->AddLine(a_min, ImVec2{ a_min.x, a_min.y + a_len }, col, a_thickness);

        a_dl->AddLine(ImVec2{ a_max.x, a_min.y }, ImVec2{ a_max.x - a_len, a_min.y }, col, a_thickness);
        a_dl->AddLine(ImVec2{ a_max.x, a_min.y }, ImVec2{ a_max.x, a_min.y + a_len }, col, a_thickness);

        a_dl->AddLine(ImVec2{ a_min.x, a_max.y }, ImVec2{ a_min.x + a_len, a_max.y }, col, a_thickness);
        a_dl->AddLine(ImVec2{ a_min.x, a_max.y }, ImVec2{ a_min.x, a_max.y - a_len }, col, a_thickness);

        a_dl->AddLine(a_max, ImVec2{ a_max.x - a_len, a_max.y }, col, a_thickness);
        a_dl->AddLine(a_max, ImVec2{ a_max.x, a_max.y - a_len }, col, a_thickness);
    }
}
