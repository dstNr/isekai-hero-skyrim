#include "UI/Toast.h"

#include "UI/Overlay.h"
#include "UI/Style.h"
#include "UI/VROverlay.h"

#include <imgui.h>

#include <algorithm>
#include <deque>
#include <mutex>
#include <utility>

namespace Isekai::UI {

    namespace {
        struct Toast {
            std::string text;
            std::string key;       // groups replacements; empty = standalone
            float       life = 0;  // seconds remaining
            float       total = 0;
            bool        banner = false;
        };

        // Posted from the main thread, drawn on the render thread.
        std::mutex        g_mutex;
        std::deque<Toast> g_toasts;

        constexpr float kLineSeconds = 3.2f;
        constexpr float kBannerSeconds = 5.5f;
        constexpr float kFadeSeconds = 0.8f;   // tail of the life, spent fading out
        constexpr float kRiseSeconds = 0.22f;  // entrance: fade in while sliding up
        // More than this on screen at once stops being a status line and becomes a wall.
        constexpr std::size_t kMaxVisible = 4;

        void Post(std::string a_text, std::string a_key, bool a_banner) {
            if (a_text.empty()) {
                return;
            }

            // Nowhere to draw: fall back to the engine's own corner queue rather than
            // silently dropping the message. Worse, but not nothing.
            //
            // Two surfaces now, and either one is enough — the flat overlay on SE/AE, or
            // the helper's HUD plane in VR. VR reaches this fallback only when
            // ImGuiVRHelper is absent, which is exactly where it was before.
            if (!OverlayReady() && !VROverlayReady()) {
                RE::SendHUDMessage::ShowHUDMessage(a_text.c_str());
                return;
            }

            const float seconds = a_banner ? kBannerSeconds : kLineSeconds;
            std::scoped_lock lock(g_mutex);

            // A keyed toast replaces the one already showing under that key instead of
            // stacking. Twenty kills should leave one counter counting up.
            if (!a_key.empty()) {
                for (auto& t : g_toasts) {
                    if (t.key == a_key) {
                        t.text = std::move(a_text);
                        t.life = seconds;
                        t.total = seconds;
                        t.banner = a_banner;
                        return;
                    }
                }
            }

            g_toasts.push_back({ std::move(a_text), std::move(a_key), seconds, seconds, a_banner });
            while (g_toasts.size() > kMaxVisible) {
                g_toasts.pop_front();  // oldest goes, newest is what you care about
            }
        }
    }

    void ShowToast(std::string a_text, std::string a_key) {
        Post(std::move(a_text), std::move(a_key), /*a_banner=*/false);
    }

    void ShowToastBanner(std::string a_text, std::string a_key) {
        Post(std::move(a_text), std::move(a_key), /*a_banner=*/true);
    }

    void DrawToasts(float a_dt) {
        std::vector<Toast> shown;
        {
            std::scoped_lock lock(g_mutex);
            if (g_toasts.empty()) {
                return;
            }
            const float dt = a_dt >= 0.0f ? a_dt : ImGui::GetIO().DeltaTime;
            for (auto& t : g_toasts) {
                t.life -= dt;
            }
            std::erase_if(g_toasts, [](const Toast& t) { return t.life <= 0.0f; });
            shown.assign(g_toasts.begin(), g_toasts.end());
        }
        if (shown.empty()) {
            return;
        }

        const float s = Style::g_scale;
        ImGuiIO&    io = ImGui::GetIO();

        // The foreground list, so a toast is never hidden behind one of our own panels —
        // and drawn without a window, so it can never take focus or eat a click.
        ImDrawList* dl = ImGui::GetForegroundDrawList();

        float y = io.DisplaySize.y * 0.12f;  // just under the compass, clear of the crosshair
        for (const auto& t : shown) {
            const float age = t.total - t.life;
            const float in = std::min(age / kRiseSeconds, 1.0f);
            const float out = std::min(t.life / kFadeSeconds, 1.0f);
            const float alpha = std::min(in, out);

            ImFont*     font = t.banner ? Style::g_title : Style::g_body;
            const float size = (t.banner ? 26.0f : 21.0f) * s;
            const ImVec2 dim = font ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, t.text.c_str())
                                    : ImGui::CalcTextSize(t.text.c_str());

            // Slides the last few pixels into place as it appears.
            const float slide = (1.0f - in) * 14.0f * s;
            const ImVec2 pos{ io.DisplaySize.x * 0.5f - dim.x * 0.5f, y + slide };

            // The plate. It used to be a flat 72%-opaque rectangle with hard edges,
            // which is a dialog box parked over the middle of the screen — the one thing
            // a status line must not look like, since it appears while you are fighting.
            //
            // Now it fades out to nothing at both ends and peaks at a third of that
            // opacity. What actually keeps the text readable is the shadow on the text
            // itself, so the plate no longer has to carry the contrast alone and can be
            // as faint as it likes. A plate wide enough to have visible edges is a box;
            // one that dissolves into the scene is a HUD.
            const ImVec2 padXY{ 28.0f * s, 7.0f * s };
            const ImVec2 bMin{ pos.x - padXY.x, pos.y - padXY.y };
            const ImVec2 bMax{ pos.x + dim.x + padXY.x, pos.y + dim.y + padXY.y };
            Style::DrawFadingBand(dl, bMin, bMax, Style::kPanelBg, 0.34f * alpha);

            // The rule under a banner fades out the same way, so nothing on the layer
            // draws a straight edge across the view.
            if (t.banner) {
                Style::DrawFadingBand(dl, ImVec2{ bMin.x, bMax.y - 1.0f * s },
                                      ImVec2{ bMax.x, bMax.y + 0.5f * s }, Style::kAccent,
                                      0.5f * alpha);
            }

            // Both lines cyan: the toast layer is the System talking, and it should read
            // as the panels do. Banner and line still differ by size and font.
            const ImVec4& col = Style::kAccent;
            Style::DrawTextShadowed(dl, font, size, pos, col, t.text.c_str(), alpha);

            y += dim.y + padXY.y * 2.0f + 8.0f * s;
        }
    }
}
