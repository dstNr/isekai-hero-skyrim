#include "UI/SystemWindow.h"

#include "Config.h"
#include "Loc.h"
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
            Emblem                   emblem;  // optional header insignia; empty = none
            std::function<void(int)> onSelect;
            float                    revealCharsPerSec = 45.0f;
            float                    width = 720.0f;  // at 1080p; scaled with display
            float                    elapsed = 0.0f;  // seconds open; drives the reveal
        };

        std::mutex        g_mutex;
        WindowState       g_win;
        std::atomic<bool> g_open{ false };

        constexpr float kFadeInSeconds = 0.30f;

        // Fast enough that no body finishes anything but instantly, without being an
        // infinity that arithmetic downstream has to survive.
        constexpr float kInstantReveal = 1.0e6f;

        // A caller's typing speed after Config::TextSpeed. Callers that already want an
        // instant panel (the status ledger, the self-test report) pass a huge rate and
        // stay instant at any multiplier.
        [[nodiscard]] float RevealRate(float a_charsPerSec) {
            const float speed = Config::TextSpeed();
            return speed <= 0.0f ? kInstantReveal : a_charsPerSec * speed;
        }

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

        // How the bottom button row lays itself out. Decided ONCE for the whole row and
        // BEFORE the window opens, because the panel's height is computed up front and
        // has to include whatever this comes back with.
        struct ButtonRow {
            float height = 0.0f;
            float iconSize = 0.0f;  // 0 = draw the labels alone
            bool  stacked = false;  // icon above the label rather than beside it
        };

        // Icon and label are drawn by hand over a blank button, and nothing used to
        // measure that pair against the button carrying it. The blessing panel is where
        // that shows: five choices split the row, "ASCENDED" is a long word, and a 34px
        // icon beside it is wider than one fifth of the panel — so the content ran out
        // over the frame into its neighbours.
        //
        // Side by side is tried first because it is the calmer shape. When it does not
        // fit, the icon moves ABOVE the label instead of shrinking: stacking gives the
        // label the button's whole width, which is exactly what it was short of. Only if
        // even the bare label will not fit does the row go without icons — a 10px smudge
        // over a clipped word helps nobody.
        //
        // One size and one arrangement for the entire row: buttons of differing shapes
        // standing side by side read as a rendering fault, not as a fit.
        [[nodiscard]] ButtonRow PlanButtonRow(float a_btnW, float a_widestLabel, float a_s) {
            constexpr float kBaseH = 46.0f;
            const float     pad = 10.0f * a_s;
            const float     gap = 8.0f * a_s;
            const float     lineH = ImGui::GetTextLineHeight();

            ButtonRow row{ kBaseH * a_s, 0.0f, false };
            if (a_widestLabel <= 0.0f) {
                return row;  // nothing in this row carries an icon
            }

            const float inline_ = (kBaseH - 12.0f) * a_s;
            if (a_widestLabel + gap + inline_ + 2.0f * pad <= a_btnW) {
                row.iconSize = inline_;
                return row;
            }

            const float stacked = std::min(30.0f * a_s, a_btnW - 2.0f * pad);
            if (stacked >= 18.0f * a_s && a_widestLabel + 2.0f * pad <= a_btnW) {
                row.stacked = true;
                row.iconSize = stacked;
                row.height = stacked + gap + lineH + 16.0f * a_s;
            }
            return row;
        }

        // --- Body colouring -------------------------------------------------------
        //
        // Every panel body is one monospace string built in Progression.cpp, and it was
        // drawn in one flat colour — a wall of identical text in which the numbers you
        // actually opened the panel for are no easier to find than the words around them.
        //
        // Colour comes from the text's own SHAPE rather than from markup: the strings
        // stay plain, nothing has to be escaped, and a new panel is styled the day it is
        // written without anyone remembering to tag it. Two rules cover everything the
        // mod writes — a "LABEL   value" row, and an all-caps line on its own.

        [[nodiscard]] bool AllCaps(std::string_view a_text) {
            bool letter = false;
            for (const unsigned char c : a_text) {
                if (std::islower(c)) {
                    return false;
                }
                letter = letter || std::isalpha(c) != 0;
            }
            return letter;
        }

        // Where a row's label ends and its value begins: the first run of two or more
        // spaces after some content. npos when the line is not that shape.
        [[nodiscard]] std::size_t ValueStart(std::string_view a_line) {
            const auto first = a_line.find_first_not_of(' ');
            if (first == std::string_view::npos) {
                return std::string_view::npos;
            }
            const auto gap = a_line.find("  ", first);
            if (gap == std::string_view::npos) {
                return std::string_view::npos;
            }
            const auto value = a_line.find_first_not_of(' ', gap);
            if (value == std::string_view::npos) {
                return std::string_view::npos;  // trailing whitespace, not a column
            }
            // A label is short. Prose that happens to contain a double space is not a
            // table row, and colouring half a sentence would look like a bug.
            return (gap - first) > 22 ? std::string_view::npos : value;
        }

        void DrawBody(const std::string& a_body, float a_fade) {
            const ImVec4 label{ Style::kAccent.x, Style::kAccent.y, Style::kAccent.z, a_fade };
            const ImVec4 value{ Style::kText.x, Style::kText.y, Style::kText.z, a_fade };

            // Every line is its own widget now, and ImGui puts ItemSpacing.y between
            // consecutive widgets — so the body silently grew by four pixels per line the
            // moment it stopped being one TextUnformatted. The panel's height is measured
            // with CalcTextSize, which knows nothing about that, and a dozen lines of it
            // was enough to overflow the panel and raise a scrollbar.
            //
            // Zeroing the vertical spacing makes the drawn block exactly as tall as the
            // measured one again. Do not remove without giving the height calculation the
            // same figure.
            const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ spacing.x, 0.0f });

            std::size_t at = 0;
            while (at <= a_body.size()) {
                const auto             eol = a_body.find('\n', at);
                const std::string_view line{ a_body.data() + at,
                                             (eol == std::string::npos ? a_body.size() : eol) - at };
                at = (eol == std::string::npos ? a_body.size() : eol) + 1;

                if (line.empty()) {
                    ImGui::NewLine();
                    continue;
                }

                const auto split = ValueStart(line);
                if (split == std::string_view::npos) {
                    // A heading stands alone in accent; ordinary prose reads as text.
                    ImGui::TextColored(AllCaps(line) ? label : value, "%.*s",
                                       static_cast<int>(line.size()), line.data());
                    continue;
                }

                // "%.*s" throughout: body text carries '%' (percentage bonuses) and would
                // otherwise be eaten as a format specifier.
                ImGui::TextColored(label, "%.*s", static_cast<int>(split), line.data());
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextColored(value, "%.*s", static_cast<int>(line.size() - split),
                                   line.data() + split);
            }

            ImGui::PopStyleVar();
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
            //
            // A click while it is still typing skips to the full text. The buttons are
            // hidden until the reveal finishes, so that click can never also press one.
            const float fullReveal =
                static_cast<float>(g_win.body.size() + 1) / g_win.revealCharsPerSec;
            if (g_win.elapsed < fullReveal && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                g_win.elapsed = fullReveal;
            }

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

            // The button row, planned before the window opens: its height feeds the
            // panel's. Planned against the content width MINUS a scrollbar, which the
            // ledger panels do grow — planning on the wider figure and then drawing into
            // the narrower one is how a layout that "fits" ends up one button short.
            // Actual widths are taken from the real content region below; those can only
            // be wider than what was planned for, never narrower.
            std::size_t textCount = 0;
            for (const auto& c : g_win.choices) {
                textCount += c.iconOnly ? 0 : 1;
            }
            ImGui::PushFont(Style::g_title, Style::BodySize());
            const float rowSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float planW = wrapW - ImGui::GetStyle().ScrollbarSize;
            const float btnW = textCount > 0
                ? (planW - rowSpacing * (static_cast<float>(textCount) - 1.0f)) /
                      static_cast<float>(textCount)
                : planW;
            float widestLabel = 0.0f;
            for (const auto& c : g_win.choices) {
                if (!c.iconOnly && !c.icon.empty()) {
                    widestLabel = std::max(widestLabel, ImGui::CalcTextSize(c.label.c_str()).x);
                }
            }
            const ButtonRow row = PlanButtonRow(btnW, widestLabel, s);
            ImGui::PopFont();

            const float headH = 28.0f * s + Style::TitleSize() + 32.0f * s;  // pad+title+sep zone
            const float footH = row.height + 48.0f * s;                      // buttons + padding
            const float iconsH = iconActions > 0
                ? 16.0f * s + static_cast<float>(iconActions) * 86.0f * s
                : 0.0f;

            // 40 rather than the old 24: slack, so a body that measures a hair taller than
            // it draws — a font fallback, a rounding step — grows the panel instead of
            // raising a scrollbar over a panel that was one line short. Scrolling is right
            // for the 79-milestone ledger, which cannot fit on any screen; it is never
            // right for a panel that missed by four pixels.
            float height = headH + std::max(bodySize.y + 40.0f * s, iconsH) + footH;
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

                // --- Header emblem (the System Rank insignia and its caption), in the
                // left margin. Drawn straight onto the draw list rather than through the
                // layout, so it cannot push the centred title off-centre or feed the
                // panel's height calculation. Every title we use is short enough that the
                // margin is free; a missing PNG simply draws nothing.
                if (!g_win.emblem.icon.empty()) {
                    if (const ImTextureID emblem = GetTexture(g_win.emblem.icon)) {
                        const float band = sepY - wMin.y;
                        const float side = std::min(56.0f * s, band - 8.0f * s);
                        if (side > 0.0f) {
                            const float x = wMin.x + 20.0f * s;
                            const float cy = (wMin.y + sepY) * 0.5f;
                            dl->AddImage(emblem, ImVec2{ x, cy - side * 0.5f },
                                         ImVec2{ x + side, cy + side * 0.5f }, ImVec2{ 0, 0 },
                                         ImVec2{ 1, 1 },
                                         IM_COL32(255, 255, 255, static_cast<int>(fade * 255.0f)));

                            // The caption beside the crest, in the rank's own colour.
                            if (!g_win.emblem.caption.empty()) {
                                const float capSize = Style::BodySize() * 0.9f;
                                ImFont*     capFont = Style::g_title;
                                const float capH =
                                    capFont ? capFont->CalcTextSizeA(capSize, FLT_MAX, 0.0f,
                                                                     g_win.emblem.caption.c_str())
                                                  .y
                                            : ImGui::GetTextLineHeight();
                                Style::DrawTextShadowed(
                                    dl, capFont, capSize,
                                    ImVec2{ x + side + 10.0f * s, cy - capH * 0.5f },
                                    g_win.emblem.captionColor, g_win.emblem.caption.c_str(), fade);
                            }
                        }
                    }
                }

                // --- Body, typewriter reveal ---
                ImGui::PushFont(Style::g_body, Style::BodySize());
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner);
                DrawBody(shownBody, fade);
                ImGui::PopTextWrapPos();
                ImGui::PopFont();

                ImGui::Spacing();
                ImGui::Spacing();

                // --- Choices, only once the text has finished typing ---
                if (bodyComplete && !g_win.choices.empty()) {
                    // Pin the row to the bottom of the (fixed-height) panel. When the
                    // content is taller than the panel and scrolls, the natural cursor
                    // is already past this point and the row simply follows the flow.
                    const float footY = height - row.height - 44.0f * s;
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

                    // Icon-only choices live at the top-right, not in this row. The row's
                    // SHAPE was settled before the window opened (PlanButtonRow) because
                    // the panel's height depends on it; the widths come from the real
                    // content region, which is what a scrollbar actually shrinks.
                    const float rowW = textCount > 0
                        ? (inner - rowSpacing * (static_cast<float>(textCount) - 1.0f)) /
                              static_cast<float>(textCount)
                        : inner;
                    const float btnH = row.height;
                    const float iconGap = 8.0f * s;

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

                        const ImTextureID icon = (choice.icon.empty() || row.iconSize <= 0.0f)
                            ? ImTextureID{}
                            : GetTexture(choice.icon);

                        if (icon) {
                            // ImGui has no icon+text button, so: an empty button does the
                            // hit-testing and hover colors, and icon plus label are drawn
                            // over it by hand.
                            if (ImGui::Button("##choice", ImVec2{ rowW, btnH })) {
                                chosen = static_cast<int>(i);
                            }
                            const ImVec2 bMin = ImGui::GetItemRectMin();
                            const ImVec2 bMax = ImGui::GetItemRectMax();
                            const ImVec2 textSize = ImGui::CalcTextSize(choice.label.c_str());
                            const float  cx = (bMin.x + bMax.x) * 0.5f;

                            // Clipped to the frame as a last resort. The layout above is
                            // chosen so this never bites for the widest label in the row —
                            // but a font fallback that measures differently from what it
                            // draws must still spill nothing into the button next door.
                            // Plate then art, in that order. No border on it — the button
                            // it sits in is already framed, and a box inside a box reads
                            // as clutter rather than as structure.
                            const auto plated = [&](ImVec2 a_min, ImVec2 a_max) {
                                Style::DrawIconPlate(dl, a_min, a_max, 1.0f, /*a_border=*/false);
                                dl->AddImage(icon, a_min, a_max);
                            };

                            dl->PushClipRect(bMin, bMax, true);
                            if (row.stacked) {
                                const float blockH = row.iconSize + iconGap + textSize.y;
                                const float top = (bMin.y + bMax.y) * 0.5f - blockH * 0.5f;
                                plated(ImVec2{ cx - row.iconSize * 0.5f, top },
                                       ImVec2{ cx + row.iconSize * 0.5f, top + row.iconSize });
                                dl->AddText(ImVec2{ cx - textSize.x * 0.5f,
                                                    top + row.iconSize + iconGap },
                                            Style::Col(Style::kText), choice.label.c_str());
                            } else {
                                const float totalW = row.iconSize + iconGap + textSize.x;
                                const float x = cx - totalW * 0.5f;
                                const float cy = (bMin.y + bMax.y) * 0.5f;
                                plated(ImVec2{ x, cy - row.iconSize * 0.5f },
                                       ImVec2{ x + row.iconSize, cy + row.iconSize * 0.5f });
                                dl->AddText(
                                    ImVec2{ x + row.iconSize + iconGap, cy - textSize.y * 0.5f },
                                    Style::Col(Style::kText), choice.label.c_str());
                            }
                            dl->PopClipRect();
                        } else if (ImGui::Button(choice.label.c_str(), ImVec2{ rowW, btnH })) {
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

                        // The plate goes down BEFORE the button, so the art lands on top
                        // of it. Its rect is the button's: the cursor is the item's
                        // top-left, and the frame padding above is added on each side.
                        {
                            const ImVec2 pMin = ImGui::GetCursorScreenPos();
                            const float  side = iconSize + 4.0f * s;
                            Style::DrawIconPlate(dl, pMin, ImVec2{ pMin.x + side, pMin.y + side },
                                                 fade);
                        }

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
        RE::SendHUDMessage::ShowHUDMessage(L("vr.needsPrismaShort",
                                "[ SYSTEM ] Needs the PrismaUI patch (1.5.0 VR build) to show "
                                "its menu in VR."));
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
                          float a_width, Emblem a_emblem) {
        // The one place the speed setting is applied, so it reaches both renderers.
        a_revealCharsPerSec = RevealRate(a_revealCharsPerSec);

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
