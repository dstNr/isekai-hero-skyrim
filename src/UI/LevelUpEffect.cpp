#include "UI/LevelUpEffect.h"

#include "UI/Style.h"

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

namespace Isekai::UI {

    namespace {
        // Fired on the main thread, drawn on the render thread.
        std::mutex        g_mutex;
        std::string       g_title;
        std::string       g_subtitle;
        float             g_elapsed = 0.0f;
        std::atomic<bool> g_active{ false };

        constexpr float kDuration = 3.2f;
        constexpr float kRingCount = 3.0f;
        constexpr float kRingLife = 1.4f;     // how long one ring takes to reach full size
        constexpr float kRingStagger = 0.18f; // delay between rings
        constexpr float kTextIn = 0.30f;      // punch-in time
        constexpr float kFadeOut = 0.7f;      // trailing fade

        // Decelerating: fast out of the gate, easing into stillness. A linear ring
        // reads as a moving line; this reads as a shockwave.
        [[nodiscard]] float EaseOutCubic(float a_t) {
            const float inv = 1.0f - a_t;
            return 1.0f - inv * inv * inv;
        }

        [[nodiscard]] float EaseOutBack(float a_t) {
            constexpr float c1 = 1.70158f;
            constexpr float c3 = c1 + 1.0f;
            const float     inv = a_t - 1.0f;
            return 1.0f + c3 * inv * inv * inv + c1 * inv * inv;
        }

        void DrawCentredText(ImDrawList* a_dl, ImFont* a_font, float a_size, ImVec2 a_centre,
                             const std::string& a_text, ImU32 a_col) {
            if (a_text.empty()) {
                return;
            }
            ImFont* font = a_font ? a_font : ImGui::GetFont();
            const ImVec2 size = font->CalcTextSizeA(a_size, FLT_MAX, 0.0f, a_text.c_str());
            const ImVec2 pos{ a_centre.x - size.x * 0.5f, a_centre.y - size.y * 0.5f };
            a_dl->AddText(font, a_size, pos, a_col, a_text.c_str());
        }
    }

    void PlayLevelUpEffect(std::string a_title, std::string a_subtitle) {
        {
            std::scoped_lock lock(g_mutex);
            g_title = std::move(a_title);
            g_subtitle = std::move(a_subtitle);
            g_elapsed = 0.0f;  // retrigger restarts it rather than stacking
        }
        g_active.store(true, std::memory_order_release);
    }

    void DrawLevelUpEffect() {
        if (!g_active.load(std::memory_order_acquire)) {
            return;
        }

        std::scoped_lock lock(g_mutex);

        ImGuiIO& io = ImGui::GetIO();
        g_elapsed += io.DeltaTime;
        if (g_elapsed >= kDuration) {
            g_active.store(false, std::memory_order_release);
            return;
        }

        const float t = g_elapsed;
        const float s = Style::g_scale;

        // Everything fades together at the end, so the flourish leaves cleanly.
        const float tail = std::clamp((kDuration - t) / kFadeOut, 0.0f, 1.0f);

        ImDrawList*  dl = ImGui::GetForegroundDrawList();
        const ImVec2 screen = io.DisplaySize;
        const ImVec2 centre{ screen.x * 0.5f, screen.y * 0.42f };
        const float  maxRadius = screen.y * 0.45f;

        // --- Shockwave rings, staggered outward ---
        for (int i = 0; i < static_cast<int>(kRingCount); ++i) {
            const float ringT = (t - static_cast<float>(i) * kRingStagger) / kRingLife;
            if (ringT <= 0.0f || ringT >= 1.0f) {
                continue;
            }
            const float radius = EaseOutCubic(ringT) * maxRadius;
            const float alpha = (1.0f - ringT) * (1.0f - ringT) * tail;

            dl->AddCircle(centre, radius, Style::Col(Style::kAccent, alpha * 0.8f), 96, 2.5f * s);
            // A second, softer ring just inside sells it as light rather than a stroke.
            dl->AddCircle(centre, radius * 0.97f, Style::Col(Style::kAccent, alpha * 0.25f), 96,
                          6.0f * s);
        }

        // --- Cyan vignette pulse: brightest as the first ring lands ---
        const float pulse = std::clamp(1.0f - t / (kRingLife * 0.9f), 0.0f, 1.0f);
        if (pulse > 0.0f) {
            const float  strength = pulse * pulse * 0.35f * tail;
            const float  band = screen.y * 0.28f;
            const ImU32  edge = Style::Col(Style::kAccent, strength);
            const ImU32  clear = Style::Col(Style::kAccent, 0.0f);
            dl->AddRectFilledMultiColor({ 0.0f, 0.0f }, { screen.x, band }, edge, edge, clear, clear);
            dl->AddRectFilledMultiColor({ 0.0f, screen.y - band }, { screen.x, screen.y }, clear,
                                        clear, edge, edge);
            dl->AddRectFilledMultiColor({ 0.0f, 0.0f }, { band, screen.y }, edge, clear, clear, edge);
            dl->AddRectFilledMultiColor({ screen.x - band, 0.0f }, { screen.x, screen.y }, clear,
                                        edge, edge, clear);
        }

        // --- Title: overshoots, then settles ---
        const float textT = std::clamp(t / kTextIn, 0.0f, 1.0f);
        const float punch = 1.35f - 0.35f * EaseOutBack(textT);
        const float titleSize = Style::TitleSize() * 1.6f * punch;

        // Glow behind the glyphs: the same text, larger and faint, a few times over.
        for (int i = 3; i >= 1; --i) {
            DrawCentredText(dl, Style::g_title, titleSize + static_cast<float>(i) * 2.0f * s, centre,
                            g_title, Style::Col(Style::kAccent, 0.10f * textT * tail));
        }
        DrawCentredText(dl, Style::g_title, titleSize, centre, g_title,
                        Style::Col(Style::kAccent, textT * tail));

        // --- Subtitle: slides up under the title once the punch has settled ---
        const float subT = std::clamp((t - kTextIn) / 0.35f, 0.0f, 1.0f);
        if (subT > 0.0f) {
            const float rise = (1.0f - EaseOutCubic(subT)) * 18.0f * s;
            const ImVec2 subCentre{ centre.x, centre.y + titleSize * 0.9f + rise };
            DrawCentredText(dl, Style::g_body, Style::BodySize() * 1.15f, subCentre, g_subtitle,
                            Style::Col(Style::kText, subT * tail));
        }
    }
}
