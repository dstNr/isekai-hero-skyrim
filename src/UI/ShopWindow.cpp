#include "UI/ShopWindow.h"

#include "Shop.h"
#include "Sounds.h"
#include "System.h"
#include "UI/Overlay.h"
#include "UI/Style.h"
#include "UI/Textures.h"

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

namespace Isekai::UI {

    namespace {
        std::atomic<bool> g_open{ false };

        constexpr const char* kIconDir = "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\";

        // Design-space (1080p) metrics; everything scales with the display, same as the
        // skill tree window.
        constexpr float kCardW = 210.0f;
        constexpr float kCardH = 250.0f;
        constexpr float kCardGap = 20.0f;
        constexpr float kPad = 28.0f;
        constexpr float kHeadH = 104.0f;   // title + points row + separator
        constexpr float kFootH = 78.0f;    // hint + close button

        float g_elapsed = 0.0f;  // drives the fade-in

        // The catalog is re-read every frame rather than cached: it is three entries, and
        // a purchase changes affordability immediately. Same reasoning as the web view
        // re-pushing its JSON after every buy.
        void RequestClose() {
            g_open.store(false, std::memory_order_release);
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([]() {
                    SetGameHold(false);
                    Sounds::Play(Sounds::Sfx::WindowClose);
                });
            }
        }
    }

    bool IsShopWindowOpen() {
        return g_open.load(std::memory_order_acquire);
    }

    void DismissShopWindow() {
        if (IsShopWindowOpen()) {
            RequestClose();
        }
    }

    void ShowShopWindow() {
        g_elapsed = 0.0f;
        Sounds::Play(Sounds::Sfx::WindowOpen);
        SetGameHold(true);
        g_open.store(true, std::memory_order_release);
    }

    void DrawShopWindow() {
        if (!IsShopWindowOpen()) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        g_elapsed += io.DeltaTime;

        const auto items = Isekai::Shop::Catalog();
        const int  count = static_cast<int>(items.size());

        const float  s = Style::g_scale;
        const float  fade = std::min(g_elapsed / 0.3f, 1.0f);
        const ImVec2 screen = io.DisplaySize;

        // Width follows the catalog, so one row of cards always fits exactly.
        const float wDesign =
            kPad * 2.0f + static_cast<float>(std::max(count, 1)) * kCardW +
            static_cast<float>(std::max(count - 1, 0)) * kCardGap;
        const ImVec2 size{ wDesign * s, (kHeadH + kCardH + kFootH) * s };

        ImGui::SetNextWindowPos(ImVec2{ screen.x * 0.5f, screen.y * 0.5f }, ImGuiCond_Always,
                                ImVec2{ 0.5f, 0.5f });
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ kPad * s, 20.0f * s });
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                              ImVec4{ Style::kPanelBg.x, Style::kPanelBg.y, Style::kPanelBg.z,
                                      Style::kPanelBg.w * fade });

        constexpr auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("##IsekaiShop", nullptr, flags)) {
            ImDrawList*  dl = ImGui::GetWindowDrawList();
            const ImVec2 wMin = ImGui::GetWindowPos();
            const ImVec2 wMax{ wMin.x + size.x, wMin.y + size.y };

            Style::DrawGlowBorder(dl, wMin, wMax, Style::kAccent, fade, 7, 2.0f * s);
            Style::DrawCornerBrackets(dl, wMin, wMax, Style::kAccent, fade, 24.0f * s, 2.5f * s);

            // --- Title ---
            ImGui::PushFont(Style::g_title, Style::TitleSize());
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4{ Style::kAccent.x, Style::kAccent.y, Style::kAccent.z,
                                          fade });
            const char* title = "[ SYSTEM ] SHOP";
            const float titleW = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPosX((size.x - titleW) * 0.5f);
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
            ImGui::PopFont();

            const float sepY = ImGui::GetCursorScreenPos().y + 4.0f * s;
            dl->AddLine(ImVec2{ wMin.x + kPad * s, sepY }, ImVec2{ wMax.x - kPad * s, sepY },
                        Style::Col(Style::kAccent, 0.45f * fade), 1.0f);

            ImGui::PushFont(Style::g_body, Style::BodySize());

            const std::int32_t points = GetState().systemPoints;
            const std::string  balance = "SYSTEM POINTS   " + std::to_string(points);
            dl->AddText(ImVec2{ wMin.x + kPad * s, sepY + 12.0f * s },
                        Style::Col(Style::kAccent, fade), balance.c_str());

            // --- Cards ---
            const float cardsY = wMin.y + kHeadH * s;
            for (int i = 0; i < count; ++i) {
                const auto&  item = items[static_cast<std::size_t>(i)];
                const bool   afford = points >= item.cost;
                const float  x = wMin.x + kPad * s + static_cast<float>(i) * (kCardW + kCardGap) * s;
                const ImVec2 cMin{ x, cardsY };
                const ImVec2 cMax{ x + kCardW * s, cardsY + kCardH * s };

                ImGui::SetCursorScreenPos(cMin);
                ImGui::PushID(i);
                ImGui::InvisibleButton("##card", ImVec2{ cMax.x - cMin.x, cMax.y - cMin.y });
                const bool hovered = ImGui::IsItemHovered();
                const bool clicked = ImGui::IsItemClicked();
                ImGui::PopID();

                // Body + frame. Unaffordable cards stay readable (the price is the point)
                // but visibly out of reach, matching the web patch's .card.cant.
                const float dim = afford ? 1.0f : 0.5f;
                dl->AddRectFilled(cMin, cMax,
                                  ImGui::GetColorU32(ImVec4{ 0.04f, 0.08f, 0.13f, 0.9f * fade }));
                const ImVec4 frame = afford ? Style::kAccent : Style::kTextDim;
                if (afford && hovered) {
                    Style::DrawGlowBorder(dl, cMin, cMax, Style::kAccent, 0.5f * fade, 4, 1.5f * s);
                }
                dl->AddRect(cMin, cMax, Style::Col(frame, (afford ? 0.75f : 0.4f) * fade), 0.0f, 0,
                            (afford && hovered ? 2.0f : 1.0f) * s);

                // Icon
                const float  iconSize = 92.0f * s;
                const ImVec2 iMin{ (cMin.x + cMax.x) * 0.5f - iconSize * 0.5f, cMin.y + 20.0f * s };
                const ImVec2 iMax{ iMin.x + iconSize, iMin.y + iconSize };
                if (const auto tex = GetTexture(std::string(kIconDir) + item.icon)) {
                    const int a = static_cast<int>(255 * fade * dim);
                    dl->AddImage(tex, iMin, iMax, ImVec2{ 0, 0 }, ImVec2{ 1, 1 },
                                 IM_COL32(255, 255, 255, a));
                }

                // Name / quantity / price, each centred in the card.
                const auto centred = [&](const std::string& text, float y, ImU32 col) {
                    const ImVec2 ts = ImGui::CalcTextSize(text.c_str());
                    dl->AddText(ImVec2{ (cMin.x + cMax.x) * 0.5f - ts.x * 0.5f, y }, col,
                                text.c_str());
                };
                centred(item.name, iMax.y + 14.0f * s, Style::Col(Style::kText, fade * dim));
                centred(item.qty, iMax.y + 40.0f * s, Style::Col(Style::kTextDim, fade * dim));
                centred(std::to_string(item.cost) + " SP", iMax.y + 68.0f * s,
                        afford ? Style::Col(Style::kAccent, fade)
                               : IM_COL32(220, 90, 90, static_cast<int>(255 * fade)));

                if (clicked && afford) {
                    const int idx = i;
                    if (auto* task = SKSE::GetTaskInterface()) {
                        task->AddTask([idx]() { Isekai::Shop::Buy(idx); });
                    }
                }
            }

            if (count == 0) {
                const char*  empty = "The catalog is empty.";
                const ImVec2 ts = ImGui::CalcTextSize(empty);
                dl->AddText(ImVec2{ (wMin.x + wMax.x) * 0.5f - ts.x * 0.5f,
                                    cardsY + kCardH * 0.5f * s },
                            Style::Col(Style::kTextDim, fade), empty);
            }

            // --- Footer ---
            dl->AddText(ImVec2{ wMin.x + kPad * s, wMax.y - 52.0f * s },
                        Style::Col(Style::kTextDim, 0.8f * fade),
                        "Delivered into your Dimensional Storage");

            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                      ImVec4{ Style::kAccent.x, Style::kAccent.y, Style::kAccent.z,
                                              0.18f });
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                      ImVec4{ Style::kAccent.x, Style::kAccent.y, Style::kAccent.z,
                                              0.35f });
                ImGui::PushStyleColor(ImGuiCol_Border, Style::kAccent);
                ImGui::PushStyleColor(ImGuiCol_Text, Style::kText);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

                const ImVec2 btnSize{ 170.0f * s, 42.0f * s };
                ImGui::SetCursorScreenPos(ImVec2{ wMax.x - btnSize.x - kPad * s,
                                                  wMax.y - btnSize.y - 18.0f * s });
                if (ImGui::Button("CLOSE", btnSize)) {
                    RequestClose();
                }

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(5);
            }

            ImGui::PopFont();
        }
        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }
}
