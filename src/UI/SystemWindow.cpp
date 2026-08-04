#include "UI/SystemWindow.h"

#include "Sounds.h"
#include "UI/Overlay.h"
#include "UI/Prisma.h"
#include "UI/ShopWindow.h"
#include "UI/SkillTreeWindow.h"
#include "UI/Style.h"
#include "UI/Textures.h"

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <utility>

namespace Isekai::UI {

    namespace {
        // The panel is opened/answered on the main thread but drawn on the render
        // thread, so every field it touches lives behind g_mutex. g_open is atomic
        // so the render thread can bail out without taking the lock at all.
        struct WindowState {
            std::string              title;
            std::string              body;
            std::vector<Choice>      choices;
            std::string              emblem;  // optional header insignia; empty = none
            std::function<void(int)> onSelect;
            float                    revealCharsPerSec = 45.0f;
            float                    width = 720.0f;  // at 1080p; scaled with display
            float                    elapsed = 0.0f;  // seconds open; drives the reveal
        };

        std::mutex        g_mutex;
        WindowState       g_win;
        std::atomic<bool> g_open{ false };

        constexpr float kFadeInSeconds = 0.30f;

        // Called from the render thread once a button was clicked. Hands the answer
        // back to the main thread, where touching game state is legal.
        void Answer(int a_index) {
            std::function<void(int)> fn;
            bool                     multiChoice = false;
            {
                std::scoped_lock lock(g_mutex);
                fn = std::move(g_win.onSelect);
                multiChoice = g_win.choices.size() > 1;
                g_win = WindowState{};
            }
            g_open.store(false, std::memory_order_release);

            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([fn = std::move(fn), a_index, multiChoice]() {
                    // Picking an option clicks; dismissing the panel whooshes. Not both:
                    // every panel closes through a button, so playing click AND close on
                    // the same press would double up — and a selection usually opens a
                    // follow-up panel anyway, which brings its own open sound.
                    Sounds::Play(multiChoice ? Sounds::Sfx::ButtonClick
                                             : Sounds::Sfx::WindowClose);

                    SetGameHold(false);
                    if (fn) {
                        fn(a_index);
                    }
                });
            }
        }

        // Draws the panel and returns the clicked button index, or -1.
        // Caller holds g_mutex.
        int DrawPanel() {
            ImGuiIO& io = ImGui::GetIO();
            g_win.elapsed += io.DeltaTime;

            const float fade = std::min(g_win.elapsed / kFadeInSeconds, 1.0f);

            // Reveal the body one character at a time, buttons only once it's done.
            // The reveal starts WITH the fade, not after it: gating it on the fade
            // made even instant-reveal panels (the status ledger) sit visibly empty
            // for a third of a second before any content appeared.
            const auto revealed =
                static_cast<std::size_t>(g_win.elapsed * g_win.revealCharsPerSec);
            const bool  bodyComplete = revealed >= g_win.body.size();
            std::string shownBody = bodyComplete ? g_win.body : g_win.body.substr(0, revealed);
            if (!bodyComplete) {
                shownBody.push_back('_');  // cursor, while it types itself out
            }

            const float  s = Style::g_scale;
            const float  width = g_win.width * s;
            const ImVec2 screen = io.DisplaySize;

            // The height is computed here, deterministically, instead of letting
            // AlwaysAutoResize feel its way: auto-resize sizes from the PREVIOUS
            // frame's items, and our absolutely-positioned icon actions fed their
            // positions back into it — an unstable loop that kept leaving icons
            // dangling over the frame. Measured against the FULL body text, so the
            // panel also does not breathe while the typewriter runs.
            std::size_t iconActions = 0;
            for (const auto& c : g_win.choices) {
                iconActions += c.iconOnly ? 1 : 0;
            }

            ImGui::PushFont(Style::g_body, Style::BodySize());
            const float  wrapW = width - 64.0f * s;
            const ImVec2 bodySize =
                ImGui::CalcTextSize(g_win.body.c_str(), nullptr, false, wrapW);
            ImGui::PopFont();

            const float headH = 28.0f * s + Style::TitleSize() + 32.0f * s;  // pad+title+sep zone
            const float footH = 46.0f * s + 48.0f * s;                       // buttons + padding
            const float iconsH = iconActions > 0
                ? 16.0f * s + static_cast<float>(iconActions) * 86.0f * s
                : 0.0f;

            float height = headH + std::max(bodySize.y + 24.0f * s, iconsH) + footH;
            height = std::min(height, screen.y * 0.85f);

            ImGui::SetNextWindowPos(ImVec2{ screen.x * 0.5f, screen.y * 0.45f }, ImGuiCond_Always,
                                    ImVec2{ 0.5f, 0.5f });
            ImGui::SetNextWindowSize(ImVec2{ width, height }, ImGuiCond_Always);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 32.0f * s, 28.0f * s });
            ImGui::PushStyleColor(ImGuiCol_WindowBg,
                                  ImVec4{ Style::kPanelBg.x, Style::kPanelBg.y, Style::kPanelBg.z,
                                          Style::kPanelBg.w * fade });

            constexpr auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoSavedSettings;

            int chosen = -1;

            if (ImGui::Begin("##IsekaiSystem", nullptr, flags)) {
                ImDrawList*  dl = ImGui::GetWindowDrawList();
                const ImVec2 wMin = ImGui::GetWindowPos();
                const ImVec2 size = ImGui::GetWindowSize();
                const ImVec2 wMax{ wMin.x + size.x, wMin.y + size.y };

                Style::DrawGlowBorder(dl, wMin, wMax, Style::kAccent, fade, 7, 2.0f * s);
                Style::DrawCornerBrackets(dl, wMin, wMax, Style::kAccent, fade, 24.0f * s,
                                          2.5f * s);

                const float inner = ImGui::GetContentRegionAvail().x;

                // --- Title, centred ---
                ImGui::PushFont(Style::g_title, Style::TitleSize());
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ImVec4{ Style::kAccent.x, Style::kAccent.y, Style::kAccent.z, fade });
                const float titleW = ImGui::CalcTextSize(g_win.title.c_str()).x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (inner - titleW) * 0.5f);
                ImGui::TextUnformatted(g_win.title.c_str());
                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::Spacing();
                const float sepY = ImGui::GetCursorScreenPos().y;
                dl->AddLine(ImVec2{ wMin.x + 24.0f * s, sepY }, ImVec2{ wMax.x - 24.0f * s, sepY },
                            Style::Col(Style::kAccent, 0.45f * fade), 1.0f);
                ImGui::Spacing();
                ImGui::Spacing();

                // --- Header emblem (the System Rank insignia), in the left margin.
                // Drawn straight onto the draw list rather than through the layout, so
                // it cannot push the centred title off-centre or feed the panel's
                // height calculation. Every title we use is short enough that the
                // margin is free; a missing PNG simply draws nothing.
                if (!g_win.emblem.empty()) {
                    if (const ImTextureID emblem = GetTexture(g_win.emblem)) {
                        const float band = sepY - wMin.y;
                        const float side = std::min(56.0f * s, band - 8.0f * s);
                        if (side > 0.0f) {
                            const float x = wMin.x + 20.0f * s;
                            const float cy = (wMin.y + sepY) * 0.5f;
                            dl->AddImage(emblem, ImVec2{ x, cy - side * 0.5f },
                                         ImVec2{ x + side, cy + side * 0.5f }, ImVec2{ 0, 0 },
                                         ImVec2{ 1, 1 },
                                         IM_COL32(255, 255, 255, static_cast<int>(fade * 255.0f)));
                        }
                    }
                }

                // --- Body, typewriter reveal ---
                ImGui::PushFont(Style::g_body, Style::BodySize());
                ImGui::PushStyleColor(
                    ImGuiCol_Text, ImVec4{ Style::kText.x, Style::kText.y, Style::kText.z, fade });
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner);
                ImGui::TextUnformatted(shownBody.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::Spacing();
                ImGui::Spacing();

                // --- Choices, only once the text has finished typing ---
                if (bodyComplete && !g_win.choices.empty()) {
                    // Pin the row to the bottom of the (fixed-height) panel. When the
                    // content is taller than the panel and scrolls, the natural cursor
                    // is already past this point and the row simply follows the flow.
                    const float footY = height - (46.0f + 44.0f) * s;
                    ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), footY));

                    ImGui::PushFont(Style::g_title, Style::BodySize());
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
                    ImGui::PushStyleColor(
                        ImGuiCol_ButtonHovered,
                        ImVec4{ Style::kAccent.x, Style::kAccent.y, Style::kAccent.z, 0.18f });
                    ImGui::PushStyleColor(
                        ImGuiCol_ButtonActive,
                        ImVec4{ Style::kAccent.x, Style::kAccent.y, Style::kAccent.z, 0.35f });
                    ImGui::PushStyleColor(ImGuiCol_Border, Style::kAccent);
                    ImGui::PushStyleColor(ImGuiCol_Text, Style::kText);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

                    // Icon-only choices live at the top-right, not in this row.
                    std::size_t textCount = 0;
                    for (const auto& c : g_win.choices) {
                        textCount += c.iconOnly ? 0 : 1;
                    }

                    const float spacing = ImGui::GetStyle().ItemSpacing.x;
                    const float btnW = textCount > 0
                        ? (inner - spacing * (static_cast<float>(textCount) - 1.0f)) /
                              static_cast<float>(textCount)
                        : inner;

                    bool first = true;
                    for (std::size_t i = 0; i < g_win.choices.size(); ++i) {
                        const auto& choice = g_win.choices[i];
                        if (choice.iconOnly) {
                            continue;
                        }
                        if (!first) {
                            ImGui::SameLine();
                        }
                        first = false;

                        ImGui::PushID(static_cast<int>(i));

                        const float       btnH = 46.0f * s;
                        const ImTextureID icon =
                            choice.icon.empty() ? ImTextureID{} : GetTexture(choice.icon);

                        if (icon) {
                            // ImGui has no icon+text button, so: an empty button does the
                            // hit-testing and hover colors, and icon plus label are drawn
                            // over it by hand, centred as one unit.
                            if (ImGui::Button("##choice", ImVec2{ btnW, btnH })) {
                                chosen = static_cast<int>(i);
                            }
                            const ImVec2 bMin = ImGui::GetItemRectMin();
                            const ImVec2 bMax = ImGui::GetItemRectMax();
                            const float  iconSize = btnH - 12.0f * s;
                            const float  gap = 8.0f * s;
                            const ImVec2 textSize = ImGui::CalcTextSize(choice.label.c_str());
                            const float  totalW = iconSize + gap + textSize.x;
                            const float  x = bMin.x + ((bMax.x - bMin.x) - totalW) * 0.5f;
                            const float  cy = (bMin.y + bMax.y) * 0.5f;

                            dl->AddImage(icon, ImVec2{ x, cy - iconSize * 0.5f },
                                         ImVec2{ x + iconSize, cy + iconSize * 0.5f });
                            dl->AddText(ImVec2{ x + iconSize + gap, cy - textSize.y * 0.5f },
                                        Style::Col(Style::kText), choice.label.c_str());
                        } else if (ImGui::Button(choice.label.c_str(), ImVec2{ btnW, btnH })) {
                            chosen = static_cast<int>(i);
                        }

                        ImGui::PopID();
                    }

                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(5);
                    ImGui::PopFont();
                }

                // --- Icon-only actions: large bare icons, anchored to the top-right,
                // in the panel's upper half. Drawn last so they sit above any body
                // text that happens to run underneath them. On hover: accent frame
                // plus the label fading in to their left, so they stay self-explaining
                // without carrying permanent text.
                if (bodyComplete) {
                    const float iconSize = 76.0f * s;
                    const float inset = 22.0f * s;
                    float       y = sepY + 16.0f * s;

                    for (std::size_t i = 0; i < g_win.choices.size(); ++i) {
                        const auto& choice = g_win.choices[i];
                        if (!choice.iconOnly) {
                            continue;
                        }
                        const ImTextureID icon =
                            choice.icon.empty() ? ImTextureID{} : GetTexture(choice.icon);
                        if (!icon) {
                            continue;
                        }

                        ImGui::SetCursorScreenPos(ImVec2{ wMax.x - inset - iconSize, y });
                        ImGui::PushID(static_cast<int>(i) + 1000);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                              ImVec4{ Style::kAccent.x, Style::kAccent.y,
                                                      Style::kAccent.z, 0.12f });
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                              ImVec4{ Style::kAccent.x, Style::kAccent.y,
                                                      Style::kAccent.z, 0.28f });
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 2.0f * s, 2.0f * s });

                        if (ImGui::ImageButton("##iconAction", icon,
                                               ImVec2{ iconSize, iconSize })) {
                            chosen = static_cast<int>(i);
                        }
                        if (ImGui::IsItemHovered()) {
                            const ImVec2 iMin = ImGui::GetItemRectMin();
                            const ImVec2 iMax = ImGui::GetItemRectMax();
                            dl->AddRect(iMin, iMax, Style::Col(Style::kAccent, 0.9f), 0.0f, 0,
                                        1.5f * s);
                            ImGui::PushFont(Style::g_body, Style::BodySize());
                            const ImVec2 ts = ImGui::CalcTextSize(choice.label.c_str());
                            dl->AddText(ImVec2{ iMin.x - ts.x - 10.0f * s,
                                                (iMin.y + iMax.y) * 0.5f - ts.y * 0.5f },
                                        Style::Col(Style::kAccent), choice.label.c_str());
                            ImGui::PopFont();
                        }

                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor(3);
                        ImGui::PopID();

                        y += iconSize + 10.0f * s;
                    }
                }
            }
            ImGui::End();

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(3);

            return chosen;
        }
    }

    bool IsSystemWindowOpen() {
        return g_open.load(std::memory_order_acquire);
    }

    bool IsSystemScreenOpen() {
        return IsSystemWindowOpen() || IsSkillTreeOpen() || IsShopWindowOpen();
    }

    bool BuiltInUiCanDisplay() {
        if (!REL::Module::IsVR()) {
            return true;
        }
        // In VR the only renderer we have is the PrismaUI patch. Say so — once per
        // session, and as a native notification, because those DO render in the headset
        // while nothing of ours does.
        static bool warned = false;
        if (!warned) {
            warned = true;
            logger::warn("UI: asked to show a built-in screen in VR without an active "
                         "PrismaUI view. The ImGui overlay is disabled in VR, so nothing "
                         "would have appeared. Needs the Isekai PrismaUI patch AND "
                         "PrismaUI's 1.5.0 VR build.");
        }
        RE::DebugNotification("[ SYSTEM ] Needs the PrismaUI patch (1.5.0 VR build) to show "
                              "its menu in VR.");
        return false;
    }

    void DismissSystemWindow() {
        if (!IsSystemWindowOpen()) {
            return;
        }

        int         lastText = -1;
        std::size_t textCount = 0;
        {
            std::scoped_lock lock(g_mutex);
            for (std::size_t i = 0; i < g_win.choices.size(); ++i) {
                if (!g_win.choices[i].iconOnly) {
                    ++textCount;
                    lastText = static_cast<int>(i);
                }
            }
        }
        if (textCount == 1 && lastText >= 0) {
            Answer(lastText);
        }
    }

    void ShowSystemWindow(std::string a_title, std::string a_body,
                          std::vector<Choice> a_choices,
                          std::function<void(int)> a_onSelect, float a_revealCharsPerSec,
                          float a_width, std::string a_emblem) {
        // Optional web renderer: hand the whole panel to PrismaUI when the patch is live.
        // It owns focus/pause and calls the selection back on the main thread — the ImGui
        // path below (state, SetGameHold, DrawPanel) is skipped entirely.
        if (Prisma::Active()) {
            Prisma::ShowPanel(std::move(a_title), std::move(a_body), std::move(a_choices),
                              std::move(a_onSelect), a_revealCharsPerSec, a_width);
            return;
        }
        if (!BuiltInUiCanDisplay()) {
            // The callback is dropped on purpose: a panel nobody can see must not leave a
            // pending decision behind. Callers that own a one-shot flag (the reincarnation)
            // guard on Prisma::Active() themselves before getting here.
            return;
        }
        {
            std::scoped_lock lock(g_mutex);
            g_win.title = std::move(a_title);
            g_win.body = std::move(a_body);
            g_win.choices = std::move(a_choices);
            g_win.emblem = std::move(a_emblem);
            g_win.onSelect = std::move(a_onSelect);
            g_win.revealCharsPerSec = std::max(a_revealCharsPerSec, 1.0f);
            g_win.width = std::max(a_width, 300.0f);
            g_win.elapsed = 0.0f;
        }
        Sounds::Play(Sounds::Sfx::WindowOpen);
        SetGameHold(true);
        g_open.store(true, std::memory_order_release);
    }

    void ShowSystemWindow(std::string a_title, std::string a_body,
                          std::vector<std::string> a_choices,
                          std::function<void(int)> a_onSelect, float a_revealCharsPerSec,
                          float a_width) {
        std::vector<Choice> choices;
        choices.reserve(a_choices.size());
        for (auto& label : a_choices) {
            choices.push_back({ std::move(label), {} });
        }
        ShowSystemWindow(std::move(a_title), std::move(a_body), std::move(choices),
                         std::move(a_onSelect), a_revealCharsPerSec, a_width);
    }

    void DrawSystemWindow() {
        if (!IsSystemWindowOpen()) {
            return;
        }

        int chosen = -1;
        {
            std::scoped_lock lock(g_mutex);
            chosen = DrawPanel();
        }

        // Answer() takes g_mutex itself, so it must run outside that scope.
        if (chosen >= 0) {
            Answer(chosen);
        }
    }
}
