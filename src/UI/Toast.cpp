#include "UI/Toast.h"

#include "UI/Overlay.h"
#include "UI/Style.h"

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

            // No overlay to draw on (Skyrim VR): fall back to the engine's own corner
            // queue rather than silently dropping the message. Worse, but not nothing.
            if (!OverlayReady()) {
                RE::DebugNotification(a_text.c_str());
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

    void DrawToasts() {
        std::vector<Toast> shown;
        {
            std::scoped_lock lock(g_mutex);
            if (g_toasts.empty()) {
                return;
            }
            const float dt = ImGui::GetIO().DeltaTime;
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

            // A backing plate, or the text is unreadable against a bright sky. Kept
            // narrow and low-contrast so it reads as a HUD element, not a dialog.
            const ImVec2 padXY{ 16.0f * s, 7.0f * s };
            const ImVec2 bMin{ pos.x - padXY.x, pos.y - padXY.y };
            const ImVec2 bMax{ pos.x + dim.x + padXY.x, pos.y + dim.y + padXY.y };
            dl->AddRectFilled(bMin, bMax,
                              ImGui::GetColorU32(ImVec4{ Style::kPanelBg.x, Style::kPanelBg.y,
                                                         Style::kPanelBg.z, 0.72f * alpha }));
            dl->AddLine(ImVec2{ bMin.x, bMax.y }, ImVec2{ bMax.x, bMax.y },
                        Style::Col(Style::kAccent, 0.55f * alpha), 1.0f * s);

            const ImVec4& col = t.banner ? Style::kAccent : Style::kText;
            if (font) {
                dl->AddText(font, size, pos, Style::Col(col, alpha), t.text.c_str());
            } else {
                dl->AddText(pos, Style::Col(col, alpha), t.text.c_str());
            }

            y += dim.y + padXY.y * 2.0f + 8.0f * s;
        }
    }
}
