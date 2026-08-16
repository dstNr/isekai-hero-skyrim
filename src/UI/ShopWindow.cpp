#include "UI/ShopWindow.h"

#include "Loc.h"
#include "Shop.h"
#include "Sounds.h"
#include "System.h"
#include "UI/Overlay.h"
#include "UI/Style.h"
#include "UI/SystemWindow.h"  // BuiltInUiCanDisplay
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
        constexpr float kCardW = 216.0f;  // wide enough that "Alchemy Ingredients" fits one line
        // Sized for icon / name / quantity / price. The effect list briefly lived on the
        // card and pushed this to 302 — four lines on every elixir, and a card that is
        // mostly small grey text. It hovers instead, the same as the skill tree's nodes.
        constexpr float kCardH = 240.0f;
        constexpr float kCardGap = 18.0f;
        constexpr float kPad = 28.0f;
        constexpr float kHeadH = 104.0f;  // title + points row + separator
        constexpr float kFootH = 78.0f;   // hint + close button
        constexpr float kRailW = 196.0f;  // category list down the left
        constexpr float kRailGap = 26.0f;
        constexpr float kRailRowH = 44.0f;
        // Cards per row WITHIN a category. Three, because the biggest shelf holds six —
        // so every category is two rows and the window never resizes as you switch. The
        // flat catalog used to wrap at six across three rows (1442x938); grouping it
        // buys back that width for the rail and still fits a 720p display once `s`
        // scales it down. tools/check.mjs asserts the result stays inside 1080p.
        constexpr int kCols = 3;

        float g_elapsed = 0.0f;  // drives the fade-in
        int   g_shelf = 0;       // which category the rail has selected

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
        if (!BuiltInUiCanDisplay()) {
            return;  // VR without PrismaUI — would open an invisible window
        }
        g_elapsed = 0.0f;
        g_shelf = 0;  // always reopen on the first shelf, never on a stale one
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
        const auto shelves = Isekai::Shop::Shelves();
        const int  count = static_cast<int>(items.size());

        const float  s = Style::g_scale;
        const float  fade = std::min(g_elapsed / 0.3f, 1.0f);
        const ImVec2 screen = io.DisplaySize;

        // Which catalog entries the selected shelf holds. Indices are the ORIGINAL
        // Catalog() positions, because that is what Buy() takes — renumbering them per
        // shelf would sell whatever happens to sit at that offset in the full list.
        g_shelf = std::clamp(g_shelf, 0, static_cast<int>(shelves.size()) - 1);
        std::vector<int> shown;
        for (int i = 0; i < count; ++i) {
            if (items[static_cast<std::size_t>(i)].shelf == shelves[static_cast<std::size_t>(g_shelf)]) {
                shown.push_back(i);
            }
        }

        // The window is sized for the BIGGEST shelf, not the selected one, so switching
        // category never resizes or re-centres it under the cursor.
        std::size_t widest = 1;
        for (const auto& shelf : shelves) {
            widest = std::max(widest, static_cast<std::size_t>(std::count_if(
                                          items.begin(), items.end(),
                                          [&](const auto& it) { return it.shelf == shelf; })));
        }
        const int cols = std::max(1, kCols);
        const int rows = std::max(1, static_cast<int>((widest + cols - 1) / cols));

        const float wDesign = kPad * 2.0f + kRailW + kRailGap +
                              static_cast<float>(cols) * kCardW +
                              static_cast<float>(cols - 1) * kCardGap;
        const float hDesign = kHeadH + static_cast<float>(rows) * kCardH +
                              static_cast<float>(rows - 1) * kCardGap + kFootH;
        const ImVec2 size{ wDesign * s, hDesign * s };

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
            const std::string  balance = LF("tree.points", "SYSTEM POINTS   {}", points);
            dl->AddText(ImVec2{ wMin.x + kPad * s, sepY + 12.0f * s },
                        Style::Col(Style::kAccent, fade), balance.c_str());

            const float cardsY = wMin.y + kHeadH * s;
            const float gridL = wMin.x + kPad * s + (kRailW + kRailGap) * s;

            // --- Category rail ---
            // A vertical list rather than tabs across the top: it reads the same way as
            // the skill tree's mastery rail, and it can carry each shelf's count without
            // the header becoming a wall of text.
            {
                const float railL = wMin.x + kPad * s;
                const float railR = railL + kRailW * s;
                // The divider does the separating, so the rows need no frames of their own.
                dl->AddLine(ImVec2{ railR + kRailGap * 0.5f * s, cardsY },
                            ImVec2{ railR + kRailGap * 0.5f * s,
                                    wMax.y - kFootH * 0.75f * s },
                            Style::Col(Style::kAccent, 0.18f * fade), 1.0f * s);

                for (std::size_t z = 0; z < shelves.size(); ++z) {
                    const auto& shelf = shelves[z];
                    const int   held = static_cast<int>(
                        std::count_if(items.begin(), items.end(),
                                      [&](const auto& it) { return it.shelf == shelf; }));
                    const bool  active = static_cast<int>(z) == g_shelf;
                    const float y = cardsY + static_cast<float>(z) * kRailRowH * s;
                    const ImVec2 rMin{ railL, y };
                    const ImVec2 rMax{ railR, y + (kRailRowH - 6.0f) * s };

                    ImGui::SetCursorScreenPos(rMin);
                    ImGui::PushID(static_cast<int>(z) + 5000);
                    ImGui::InvisibleButton("##shelf", ImVec2{ rMax.x - rMin.x, rMax.y - rMin.y });
                    const bool hovered = ImGui::IsItemHovered();
                    if (ImGui::IsItemClicked()) {
                        g_shelf = static_cast<int>(z);
                        Sounds::Play(Sounds::Sfx::ButtonClick);
                    }
                    ImGui::PopID();

                    if (active) {
                        dl->AddRectFilled(rMin, rMax,
                                          Style::Col(Style::kAccent, 0.12f * fade));
                        // A bright edge on the selected row, pointing at the grid.
                        dl->AddRectFilled(ImVec2{ rMin.x, rMin.y },
                                          ImVec2{ rMin.x + 3.0f * s, rMax.y },
                                          Style::Col(Style::kAccent, 0.95f * fade));
                    } else if (hovered) {
                        dl->AddRectFilled(rMin, rMax, Style::Col(Style::kAccent, 0.06f * fade));
                    }

                    ImFont*     font = ImGui::GetFont();
                    const float nameSz = std::max(13.0f * s, ImGui::GetFontSize() * 0.85f);
                    const ImVec4 col = active ? Style::kAccent
                                              : (hovered ? Style::kText : Style::kTextDim);
                    dl->AddText(font, nameSz, ImVec2{ rMin.x + 14.0f * s, rMin.y + 10.0f * s },
                                Style::Col(col, fade), shelf.c_str());

                    const std::string tally = std::to_string(held);
                    const ImVec2      ts = font->CalcTextSizeA(nameSz, FLT_MAX, 0.0f, tally.c_str());
                    dl->AddText(font, nameSz,
                                ImVec2{ rMax.x - 12.0f * s - ts.x, rMin.y + 10.0f * s },
                                Style::Col(Style::kTextDim, 0.8f * fade), tally.c_str());
                }
            }

            // --- Cards of the selected shelf ---
            for (std::size_t slot = 0; slot < shown.size(); ++slot) {
                const int    i = shown[slot];
                const auto&  item = items[static_cast<std::size_t>(i)];
                const bool   afford = points >= item.cost;
                const int    col = static_cast<int>(slot) % cols;
                const int    row = static_cast<int>(slot) / cols;
                const float  x = gridL + static_cast<float>(col) * (kCardW + kCardGap) * s;
                const float  y = cardsY + static_cast<float>(row) * (kCardH + kCardGap) * s;
                const ImVec2 cMin{ x, y };
                const ImVec2 cMax{ x + kCardW * s, y + kCardH * s };

                ImGui::SetCursorScreenPos(cMin);
                ImGui::PushID(i + 1);  // 0 is free for the rail's own IDs
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
                const float  iconSize = 88.0f * s;
                const ImVec2 iMin{ (cMin.x + cMax.x) * 0.5f - iconSize * 0.5f, cMin.y + 16.0f * s };
                const ImVec2 iMax{ iMin.x + iconSize, iMin.y + iconSize };
                if (const auto tex = GetTexture(std::string(kIconDir) + item.icon)) {
                    const int a = static_cast<int>(255 * fade * dim);
                    // The card is already framed, so the plate under the art carries no
                    // border of its own — it is there to lift a dark-bodied icon off a
                    // dark card, nothing more.
                    Style::DrawIconPlate(dl, iMin, iMax, fade * dim, /*a_border=*/false);
                    dl->AddImage(tex, iMin, iMax, ImVec2{ 0, 0 }, ImVec2{ 1, 1 },
                                 IM_COL32(255, 255, 255, a));
                }

                // Name / quantity / price, centred and WRAPPED inside the card. Drawn
                // unwrapped at full body size, the longer names ("Alchemy Ingredients")
                // were wider than the card and ran across its border.
                ImFont*     font = ImGui::GetFont();
                const float fs = ImGui::GetFontSize();
                const float wrapW = kCardW * s - 20.0f * s;
                const auto  centred = [&](const std::string& text, float size, float y, ImU32 col) {
                    const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, wrapW, text.c_str());
                    dl->AddText(font, size,
                                ImVec2{ (cMin.x + cMax.x) * 0.5f - ts.x * 0.5f, y }, col,
                                text.c_str(), nullptr, wrapW);
                    return ts.y;
                };

                const float nameSz = std::max(13.0f * s, fs * 0.82f);
                const float subSz = std::max(11.0f * s, fs * 0.7f);

                float ty = iMax.y + 12.0f * s;
                ty += centred(item.name, nameSz, ty, Style::Col(Style::kText, fade * dim)) +
                      6.0f * s;
                ty += centred(item.qty, subSz, ty, Style::Col(Style::kTextDim, fade * dim)) +
                      8.0f * s;

                // The price sits at a FIXED distance from the bottom rather than after
                // whatever came before it, so it lines up across the grid however many
                // effect lines the cards above it happen to carry.
                const std::string price = LF("shop.price", "{} SP", item.cost);
                const float priceH = font->CalcTextSizeA(nameSz, FLT_MAX, wrapW, price.c_str()).y;
                const float priceY = cMax.y - 14.0f * s - priceH;
                centred(price, nameSz, priceY,
                        afford ? Style::Col(Style::kAccent, fade)
                               : IM_COL32(220, 90, 90, static_cast<int>(255 * fade)));

                // What the item does, on hover. ImGui sizes the tooltip to its text, so a
                // four-effect elixir costs the card nothing — and unlike a fixed block on
                // the card, a translation that runs longer than English cannot overflow
                // anything. The web patch does the same on its own cards.
                if (hovered) {
                    ImGui::SetTooltip(
                        "%s\n%s", item.name.c_str(),
                        item.effects.empty()
                            ? L("shop.noEffects", "No magical effect — goods, not a potion.")
                            : item.effects.c_str());
                }

                if (clicked && afford) {
                    const int idx = i;
                    if (auto* task = SKSE::GetTaskInterface()) {
                        task->AddTask([idx]() { Isekai::Shop::Buy(idx); });
                    }
                }
            }

            if (shown.empty()) {
                // A shelf can legitimately be empty: the potions only appear once their
                // ESP records exist, so an older plugin shows MATERIALS and WEALTH only.
                const char*  empty = count == 0
                                         ? L("shop.catalogEmpty", "The catalog is empty.")
                                         : L("shop.shelfEmpty", "Nothing on this shelf yet.");
                const ImVec2 ts = ImGui::CalcTextSize(empty);
                dl->AddText(ImVec2{ (gridL + wMax.x - kPad * s) * 0.5f - ts.x * 0.5f,
                                    cardsY + kCardH * 0.5f * s },
                            Style::Col(Style::kTextDim, fade), empty);
            }

            // --- Footer ---
            dl->AddText(ImVec2{ wMin.x + kPad * s, wMax.y - 52.0f * s },
                        Style::Col(Style::kTextDim, 0.8f * fade),
                        L("shop.footer", "Delivered into your Dimensional Storage"));

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
                if (ImGui::Button(L("button.close", "CLOSE"), btnSize)) {
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
