#include "UI/SystemWindow.h"

#include "UI/Style.h"

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
            std::vector<std::string> choices;
            std::function<void(int)> onSelect;
            float                    elapsed = 0.0f;  // seconds open; drives the reveal
        };

        std::mutex        g_mutex;
        WindowState       g_win;
        std::atomic<bool> g_open{ false };

        constexpr float kRevealCharsPerSec = 45.0f;
        constexpr float kFadeInSeconds = 0.30f;
        constexpr float kPanelWidth = 640.0f;

        using UEFlag = RE::ControlMap::UEFlag;

        // CommonLibSSE only defines bitwise ops for stl::enumeration, not for the
        // bare enum class, so combine the flags by hand.
        template <class... Args>
        [[nodiscard]] constexpr UEFlag CombineFlags(Args... a_flags) {
            using U = std::underlying_type_t<UEFlag>;
            return static_cast<UEFlag>((static_cast<U>(a_flags) | ...));
        }

        // While the panel is up the player must not walk, swing, or open menus.
        // Skyrim reads input through DirectInput, not the window message queue, so
        // suppressing WndProc alone would not be enough — the control map is.
        constexpr auto kBlockedControls =
            CombineFlags(UEFlag::kMovement, UEFlag::kLooking, UEFlag::kActivate,
                         UEFlag::kMenu, UEFlag::kFighting, UEFlag::kSneaking,
                         UEFlag::kJumping, UEFlag::kMainFour, UEFlag::kWheelZoom,
                         UEFlag::kPOVSwitch);

        // Main thread only.
        void SetGameInputEnabled(bool a_enable) {
            if (auto* controls = RE::ControlMap::GetSingleton()) {
                controls->ToggleControls(kBlockedControls, a_enable);
            }
        }

        // Called from the render thread once a button was clicked. Hands the answer
        // back to the main thread, where touching game state is legal.
        void Answer(int a_index) {
            std::function<void(int)> fn;
            {
                std::scoped_lock lock(g_mutex);
                fn = std::move(g_win.onSelect);
                g_win = WindowState{};
            }
            g_open.store(false, std::memory_order_release);

            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([fn = std::move(fn), a_index]() {
                    SetGameInputEnabled(true);
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
            const auto revealed = static_cast<std::size_t>(
                std::max(0.0f, (g_win.elapsed - kFadeInSeconds) * kRevealCharsPerSec));
            const bool  bodyComplete = revealed >= g_win.body.size();
            std::string shownBody = bodyComplete ? g_win.body : g_win.body.substr(0, revealed);
            if (!bodyComplete) {
                shownBody.push_back('_');  // cursor, while it types itself out
            }

            const ImVec2 screen = io.DisplaySize;
            ImGui::SetNextWindowPos(ImVec2{ screen.x * 0.5f, screen.y * 0.45f }, ImGuiCond_Always,
                                    ImVec2{ 0.5f, 0.5f });
            ImGui::SetNextWindowSize(ImVec2{ kPanelWidth, 0.0f }, ImGuiCond_Always);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 28.0f, 24.0f });
            ImGui::PushStyleColor(ImGuiCol_WindowBg,
                                  ImVec4{ Style::kPanelBg.x, Style::kPanelBg.y, Style::kPanelBg.z,
                                          Style::kPanelBg.w * fade });

            constexpr auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_AlwaysAutoResize;

            int chosen = -1;

            if (ImGui::Begin("##IsekaiSystem", nullptr, flags)) {
                ImDrawList*  dl = ImGui::GetWindowDrawList();
                const ImVec2 wMin = ImGui::GetWindowPos();
                const ImVec2 size = ImGui::GetWindowSize();
                const ImVec2 wMax{ wMin.x + size.x, wMin.y + size.y };

                Style::DrawGlowBorder(dl, wMin, wMax, Style::kAccent, fade);
                Style::DrawCornerBrackets(dl, wMin, wMax, Style::kAccent, fade);

                const float inner = ImGui::GetContentRegionAvail().x;

                // --- Title, centred ---
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ImVec4{ Style::kAccent.x, Style::kAccent.y, Style::kAccent.z, fade });
                const float titleW = ImGui::CalcTextSize(g_win.title.c_str()).x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (inner - titleW) * 0.5f);
                ImGui::TextUnformatted(g_win.title.c_str());
                ImGui::PopStyleColor();

                ImGui::Spacing();
                const float sepY = ImGui::GetCursorScreenPos().y;
                dl->AddLine(ImVec2{ wMin.x + 20.0f, sepY }, ImVec2{ wMax.x - 20.0f, sepY },
                            Style::Col(Style::kAccent, 0.45f * fade), 1.0f);
                ImGui::Spacing();
                ImGui::Spacing();

                // --- Body, typewriter reveal ---
                ImGui::PushStyleColor(
                    ImGuiCol_Text, ImVec4{ Style::kText.x, Style::kText.y, Style::kText.z, fade });
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner);
                ImGui::TextUnformatted(shownBody.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::Spacing();

                // --- Choices, only once the text has finished typing ---
                if (bodyComplete && !g_win.choices.empty()) {
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

                    const auto  count = static_cast<float>(g_win.choices.size());
                    const float spacing = ImGui::GetStyle().ItemSpacing.x;
                    const float btnW = (inner - spacing * (count - 1.0f)) / count;

                    for (std::size_t i = 0; i < g_win.choices.size(); ++i) {
                        if (i > 0) {
                            ImGui::SameLine();
                        }
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::Button(g_win.choices[i].c_str(), ImVec2{ btnW, 40.0f })) {
                            chosen = static_cast<int>(i);
                        }
                        ImGui::PopID();
                    }

                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(5);
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

    void ShowSystemWindow(std::string a_title, std::string a_body,
                          std::vector<std::string> a_choices,
                          std::function<void(int)> a_onSelect) {
        {
            std::scoped_lock lock(g_mutex);
            g_win.title = std::move(a_title);
            g_win.body = std::move(a_body);
            g_win.choices = std::move(a_choices);
            g_win.onSelect = std::move(a_onSelect);
            g_win.elapsed = 0.0f;
        }
        SetGameInputEnabled(false);
        g_open.store(true, std::memory_order_release);
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
