#pragma once

#include <imgui.h>

#include <algorithm>

// Visual language of the System: cold cyan light on a deep translucent navy
// panel, hard angular edges, no rounded "app" look.
namespace Isekai::UI::Style {

    inline const ImVec4 kPanelBg{ 0.02f, 0.05f, 0.10f, 0.92f };
    inline const ImVec4 kAccent{ 0.35f, 0.80f, 1.00f, 1.00f };
    inline const ImVec4 kText{ 0.85f, 0.94f, 1.00f, 1.00f };
    inline const ImVec4 kTextDim{ 0.50f, 0.65f, 0.78f, 1.00f };

    // Monospace on purpose: it sells the "terminal" read, and it makes the option
    // list line up in columns, which a proportional font cannot do.
    // Set by Overlay::InitImGui. Null is fine — ImGui then keeps its built-in font.
    inline ImFont* g_body = nullptr;
    inline ImFont* g_title = nullptr;

    // Everything is authored against 1080p and scaled from there, so the panel does
    // not shrink into a postage stamp on a 4K screen.
    inline float g_scale = 1.0f;

    inline void UpdateScale(float a_displayHeight) {
        g_scale = std::max(a_displayHeight / 1080.0f, 0.5f);
    }

    [[nodiscard]] inline float BodySize() { return 20.0f * g_scale; }
    [[nodiscard]] inline float TitleSize() { return 30.0f * g_scale; }

    [[nodiscard]] inline ImU32 Col(const ImVec4& a_color, float a_alphaScale = 1.0f) {
        return ImGui::GetColorU32(
            ImVec4{ a_color.x, a_color.y, a_color.z, a_color.w * a_alphaScale });
    }

    // Text with a drop shadow, for anything drawn straight onto the game world.
    //
    // The HUD layers (toasts, threat labels) cannot rely on a backing plate for
    // legibility: a plate solid enough to guarantee contrast against snow at noon is
    // far too heavy to sit over the world all the time. A shadow costs one extra draw,
    // works against any background, and is what every game HUD does for the same reason.
    inline void DrawTextShadowed(ImDrawList* a_dl, ImFont* a_font, float a_size, ImVec2 a_pos,
                                 const ImVec4& a_color, const char* a_text,
                                 float a_alphaScale = 1.0f) {
        const float off = std::max(1.0f, a_size * 0.06f);
        const ImU32 shadow = ImGui::GetColorU32(ImVec4{ 0.0f, 0.0f, 0.0f, 0.72f * a_alphaScale });
        const ImVec2 shadowPos{ a_pos.x + off, a_pos.y + off };
        if (a_font) {
            a_dl->AddText(a_font, a_size, shadowPos, shadow, a_text);
            a_dl->AddText(a_font, a_size, a_pos, Col(a_color, a_alphaScale), a_text);
        } else {
            a_dl->AddText(shadowPos, shadow, a_text);
            a_dl->AddText(a_pos, Col(a_color, a_alphaScale), a_text);
        }
    }

    // A horizontal band that fades out towards both ends instead of stopping at a hard
    // edge. Two rects, because ImGui's gradient fill has only two stops. Used for the
    // HUD's backing plates and rules: an edge is what makes a translucent panel read as
    // a box sitting on the screen, so the way to make one recede is to remove its edges,
    // not only to lower its alpha.
    inline void DrawFadingBand(ImDrawList* a_dl, ImVec2 a_min, ImVec2 a_max,
                               const ImVec4& a_color, float a_alphaScale = 1.0f) {
        const ImU32 solid = Col(a_color, a_alphaScale);
        const ImU32 clear = Col(a_color, 0.0f);
        const float mid = (a_min.x + a_max.x) * 0.5f;
        a_dl->AddRectFilledMultiColor(a_min, ImVec2{ mid, a_max.y }, clear, solid, solid, clear);
        a_dl->AddRectFilledMultiColor(ImVec2{ mid, a_min.y }, a_max, solid, clear, clear, solid);
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

    // Our own pointer, drawn into the foreground. ImGui's stock arrow is a plain
    // grey OS cursor and reads as a foreign object on top of the panel; this one is
    // cut from the same cyan as the frame. Skyrim has no cursor of its own to borrow
    // outside its Scaleform menus, so drawing one is the only option.
    inline void DrawSystemCursor(ImDrawList* a_dl, ImVec2 a_pos, float a_scale,
                                 const ImVec4& a_color) {
        const auto P = [&](float x, float y) {
            return ImVec2{ a_pos.x + x * a_scale, a_pos.y + y * a_scale };
        };

        // A hard, angular arrowhead — no rounded OS-cursor curves.
        const ImVec2 tip = P(0.0f, 0.0f);
        const ImVec2 tail = P(0.0f, 18.0f);
        const ImVec2 barb = P(13.0f, 13.0f);

        // Drop shadow first, so the cursor stays readable over a bright sky.
        const ImVec2 off{ 2.0f * a_scale, 2.0f * a_scale };
        a_dl->AddTriangleFilled(ImVec2{ tip.x + off.x, tip.y + off.y },
                                ImVec2{ tail.x + off.x, tail.y + off.y },
                                ImVec2{ barb.x + off.x, barb.y + off.y },
                                IM_COL32(0, 0, 0, 120));

        for (int i = 3; i >= 1; --i) {
            const float spread = static_cast<float>(i) * 1.5f * a_scale;
            a_dl->AddTriangle(ImVec2{ tip.x - spread, tip.y - spread },
                              ImVec2{ tail.x - spread, tail.y + spread },
                              ImVec2{ barb.x + spread, barb.y + spread },
                              Col(a_color, 0.10f), 1.0f);
        }

        a_dl->AddTriangleFilled(tip, tail, barb, Col(a_color, 0.30f));
        a_dl->AddTriangle(tip, tail, barb, Col(a_color), 1.5f * a_scale);
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
