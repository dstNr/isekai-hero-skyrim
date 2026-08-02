#include "UI/SkillTreeWindow.h"

#include "Config.h"
#include "SkillTree.h"
#include "Sounds.h"
#include "System.h"
#include "UI/Overlay.h"
#include "UI/Style.h"
#include "UI/Textures.h"

#include <imgui.h>

#include <atomic>
#include <cmath>
#include <string>

namespace Isekai::UI {

    namespace {
        std::atomic<bool> g_open{ false };

        constexpr const char* kIconDir = "Data\\SKSE\\Plugins\\IsekaiHero\\icons\\";

        // Design-space (1080p) metrics; everything scales with the display.
        constexpr float kWindowW = 1120.0f;
        constexpr float kWindowH = 760.0f;
        constexpr float kCanvasTop = 70.0f;  // below title + separator
        constexpr float kNodeSize = 64.0f;

        // Render-thread state.
        float         g_elapsed = 0.0f;   // drives fade-in and the "affordable" pulse
        std::uint32_t g_hovered = 0;      // node key under the cursor, 0 = none
        // Screen rect of that node, so the hover card can be placed beside it rather
        // than in a band at the bottom of the window.
        ImVec2        g_hoverMin{}, g_hoverMax{};
        float         g_respecArmedUntil = 0.0f;  // respec waits for a confirming second click

        // Close from the render thread: hand the unpause to the main thread, same
        // pattern as SystemWindow::Answer.
        void RequestClose() {
            g_open.store(false, std::memory_order_release);
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([]() {
                    SetGameHold(false);
                    Sounds::Play(Sounds::Sfx::WindowClose);
                });
            }
        }

        struct NodeVisual {
            ImU32 ring;
            float ringThickness;
            ImU32 iconTint;
            bool  glow;
            ImU32 label;  // the always-on name under the tile
        };

        // With HideSealedNodes set, a node gated above the current rebirth tier is not
        // drawn at all (nor its connecting lines) instead of showing greyed. Default off.
        [[nodiscard]] bool NodeHidden(const SkillTree::Node& a_node) {
            return Config::HideSealedNodes() && !SkillTree::TierMet(a_node.key);
        }

        // The mastery stats — grouped into their own labelled column on the left, off to
        // the side of the branch graph, so they don't float there disconnected. Mirrors
        // the web patch's utility rail. Reads the node's declared Zone rather than
        // re-deriving "repeatable and no prereqs", so both renderers and the data agree.
        [[nodiscard]] bool IsUtilityNode(const SkillTree::Node& a_node) {
            return a_node.zone == SkillTree::Zone::kMastery;
        }

        // A repeatable node is "done" only once it hits its rank cap; below that it keeps
        // showing as buyable. One-shot nodes are done the moment they're owned.
        [[nodiscard]] bool NodeMaxed(const SkillTree::Node& a_node) {
            return a_node.repeatable && a_node.maxRank > 0 &&
                   SkillTree::Rank(a_node.key) >= a_node.maxRank;
        }

        [[nodiscard]] bool NodeOwned(const SkillTree::Node& a_node) {
            return a_node.repeatable ? NodeMaxed(a_node) : SkillTree::IsUnlocked(a_node.key);
        }

        [[nodiscard]] NodeVisual VisualFor(const SkillTree::Node& a_node, float a_s) {
            const bool unlocked = NodeOwned(a_node);
            // A node gated by rebirth tier can never be taken this life — treat it as
            // hard-locked regardless of prerequisites or points.
            const bool reachable =
                SkillTree::PrereqsMet(a_node.key) && SkillTree::TierMet(a_node.key);
            // NextCost, not the flat table cost — mastery nodes (capped repeatables)
            // rise per tier, so what buying costs right now depends on the rank owned.
            const bool affordable = SkillTree::Points() >= SkillTree::NextCost(a_node.key);

            if (unlocked) {
                return { Style::Col(Style::kAccent), 2.5f * a_s, IM_COL32_WHITE, true,
                         Style::Col(Style::kAccent) };
            }
            if (reachable && affordable) {
                // Breathing ring: this is the "you can take me" signal.
                const float pulse = 0.55f + 0.45f * std::sin(g_elapsed * 3.5f);
                return { Style::Col(Style::kAccent, 0.35f + 0.5f * pulse), 2.0f * a_s,
                         IM_COL32(235, 235, 235, 255), false, Style::Col(Style::kText) };
            }
            if (reachable) {
                // Reachable but too poor: visible, quietly waiting.
                return { Style::Col(Style::kTextDim, 0.8f), 1.5f * a_s,
                         IM_COL32(150, 150, 150, 255), false, Style::Col(Style::kTextDim, 0.85f) };
            }
            if (!SkillTree::TierMet(a_node.key)) {
                // Sealed by rebirth tier — an amber lock, so it reads distinctly from a
                // plain grey "prerequisite missing" node.
                return { IM_COL32(210, 158, 96, 220), 1.5f * a_s, IM_COL32(160, 120, 74, 210),
                         false, IM_COL32(200, 150, 90, 200) };
            }
            // Locked behind prerequisites: greyed down hard.
            return { Style::Col(Style::kTextDim, 0.35f), 1.0f * a_s,
                     IM_COL32(80, 80, 80, 200), false, Style::Col(Style::kTextDim, 0.45f) };
        }

        // Mastery tiers (Novice..Grandmaster), roman numerals for the rank pip — mirrors
        // the web patch's ROMAN array.
        constexpr const char* kRomanTiers[5] = { "I", "II", "III", "IV", "V" };

        // Rough mirror of the web patch's .rank.tierN palette: a warm bronze climb to
        // gold/white at Grandmaster, distinct from the plain cyan rank pip everything
        // else uses.
        [[nodiscard]] ImU32 TierPipColor(std::int32_t a_tier, float a_fade) {
            const int a = static_cast<int>(255 * a_fade);
            switch (a_tier) {
            case 1: return IM_COL32(217, 172, 124, a);
            case 2: return IM_COL32(227, 200, 143, a);
            case 3: return IM_COL32(224, 168, 96, a);
            case 4: return IM_COL32(245, 210, 122, a);
            case 5: return IM_COL32(255, 255, 255, a);
            default: return Style::Col(Style::kAccent, a_fade);
            }
        }
    }

    bool IsSkillTreeOpen() {
        return g_open.load(std::memory_order_acquire);
    }

    void DismissSkillTree() {
        if (IsSkillTreeOpen()) {
            RequestClose();
        }
    }

    void ShowSkillTree() {
        g_elapsed = 0.0f;
        g_hovered = 0;
        g_respecArmedUntil = 0.0f;
        Sounds::Play(Sounds::Sfx::WindowOpen);
        SetGameHold(true);
        g_open.store(true, std::memory_order_release);
    }

    void DrawSkillTree() {
        if (!IsSkillTreeOpen()) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        g_elapsed += io.DeltaTime;

        const float  s = Style::g_scale;
        const float  fade = std::min(g_elapsed / 0.3f, 1.0f);
        const ImVec2 screen = io.DisplaySize;
        const ImVec2 size{ kWindowW * s, kWindowH * s };

        ImGui::SetNextWindowPos(ImVec2{ screen.x * 0.5f, screen.y * 0.5f }, ImGuiCond_Always,
                                ImVec2{ 0.5f, 0.5f });
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 24.0f * s, 20.0f * s });
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                              ImVec4{ Style::kPanelBg.x, Style::kPanelBg.y, Style::kPanelBg.z,
                                      Style::kPanelBg.w * fade });

        constexpr auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("##IsekaiSkillTree", nullptr, flags)) {
            ImDrawList*  dl = ImGui::GetWindowDrawList();
            const ImVec2 wMin = ImGui::GetWindowPos();
            const ImVec2 wMax{ wMin.x + size.x, wMin.y + size.y };

            Style::DrawGlowBorder(dl, wMin, wMax, Style::kAccent, fade, 7, 2.0f * s);
            Style::DrawCornerBrackets(dl, wMin, wMax, Style::kAccent, fade, 24.0f * s, 2.5f * s);

            // --- Title + souls counter ---
            ImGui::PushFont(Style::g_title, Style::TitleSize());
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4{ Style::kAccent.x, Style::kAccent.y, Style::kAccent.z,
                                          fade });
            const char* title = "[ SYSTEM ] SKILL TREE";
            const float titleW = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPosX((size.x - titleW) * 0.5f);
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
            ImGui::PopFont();

            const float sepY = ImGui::GetCursorScreenPos().y + 4.0f * s;
            dl->AddLine(ImVec2{ wMin.x + 24.0f * s, sepY }, ImVec2{ wMax.x - 24.0f * s, sepY },
                        Style::Col(Style::kAccent, 0.45f * fade), 1.0f);

            ImGui::PushFont(Style::g_body, Style::BodySize());

            const std::string points = "SYSTEM POINTS   " + std::to_string(SkillTree::Points());
            dl->AddText(ImVec2{ wMin.x + 28.0f * s, sepY + 12.0f * s },
                        Style::Col(Style::kAccent, fade), points.c_str());
            const std::string perks = "PERK POINTS     " + std::to_string(SkillTree::PerkPool()) +
                                      " / " + std::to_string(Isekai::kMaxPerkPoints);
            dl->AddText(ImVec2{ wMin.x + 28.0f * s, sepY + 38.0f * s },
                        Style::Col(Style::kTextDim, fade), perks.c_str());

            // Everything below starts under the points block. Deriving this instead of
            // assuming a fixed kCanvasTop is what stops the mastery rail's header from
            // being drawn straight over "PERK POINTS", which is exactly what happened.
            const float headBottom =
                sepY + 38.0f * s + ImGui::GetTextLineHeight() + 12.0f * s;

            // --- The graph ---
            // The node table's x/y are a DESIGN layout, not window offsets: they are
            // fitted (uniform scale, centred) into the area right of the mastery rail.
            // Used literally, as they were, the graph could not be re-arranged without
            // re-tuning it against the window size. Mirrors the web view's layout().
            std::size_t count = 0;
            const auto* nodes = SkillTree::Nodes(count);

            constexpr float kRailW = 216.0f;    // mastery rail, design px
            constexpr float kMaxScale = 1.45f;  // the largest Node::scale in the table

            const float boxL = wMin.x + (kRailW + 20.0f) * s;
            const float boxR = wMax.x - 26.0f * s;
            const float boxT = headBottom + 8.0f * s;
            // Only the footer buttons are reserved now. The hover readout used to own a
            // 150px band down here, and the graph paid for it: on a 760px-tall window
            // that band plus the header left barely 45% of the height for the tree, which
            // is why everything was squeezed and the labels collided. It floats by the
            // cursor instead, the same as the web view's tooltip.
            const float boxB = wMax.y - 88.0f * s;

            // Reserve inside that box for everything drawn AROUND a node centre. These
            // are in `s` units while their contents scale with `fit`, so each has to be
            // large enough at the resulting fit — a circular relationship that was solved
            // numerically rather than guessed. At 1080p this lands at fit ~0.68 with
            // ~4px to spare on the tightest of the three:
            //   sideways : half a landmark tile + the zone frame pad + half a node name
            //   above    : half the hub tile + the zone frame pad + the zone header text
            //   below    : half a landmark tile + the zone frame pad + a two-line name
            // Raising any of them shrinks `fit` (smaller icons); lowering one clips what
            // it was reserving for.
            const float padX = (kNodeSize * 0.5f * kMaxScale + 62.0f) * s;
            const float padT = (kNodeSize * 0.5f * kMaxScale + 22.0f) * s;
            const float padB = (kNodeSize * 0.5f * kMaxScale + 48.0f) * s;

            float gMinX = FLT_MAX, gMaxX = -FLT_MAX, gMinY = FLT_MAX, gMaxY = -FLT_MAX;
            for (std::size_t i = 0; i < count; ++i) {
                if (IsUtilityNode(nodes[i]) || NodeHidden(nodes[i])) {
                    continue;
                }
                gMinX = std::min(gMinX, nodes[i].x);
                gMaxX = std::max(gMaxX, nodes[i].x);
                gMinY = std::min(gMinY, nodes[i].y);
                gMaxY = std::max(gMaxY, nodes[i].y);
            }
            const float gW = std::max(1.0f, gMaxX - gMinX);
            const float gH = std::max(1.0f, gMaxY - gMinY);
            const float availW = std::max(1.0f, (boxR - boxL) - padX * 2.0f);
            const float availH = std::max(1.0f, (boxB - boxT) - padT - padB);
            const float fit = std::min(availW / gW, availH / gH);
            const float offX = boxL + padX + (availW - gW * fit) * 0.5f - gMinX * fit;
            const float offY = boxT + padT + (availH - gH * fit) * 0.5f - gMinY * fit;

            const auto centerOf = [&](const SkillTree::Node& n) {
                return ImVec2{ offX + n.x * fit, offY + n.y * fit };
            };
            // Tiles scale with `fit`, exactly like the positions and the zone padding.
            // They used to scale with `s` alone — so when the graph was compressed to fit
            // the window (fit < s), the gaps between nodes shrank while the tiles did not,
            // and labels, frames and icons grew into each other. Every piece of the graph
            // now shares one factor, which is what makes the clearances in SkillTree.cpp's
            // layout note hold. Mirrors the web view's radOf().
            const auto halfOf = [&](const SkillTree::Node& n) {
                return kNodeSize * 0.5f * n.scale * fit;
            };

            // --- Zone frames: a tinted box and a header behind each branch, so CORE /
            // MIGHT / SHADOW / ARCANA read as deliberate groups rather than one web of
            // lines. Drawn first, so lines and nodes sit on top.
            {
                const struct {
                    SkillTree::Zone zone;
                    ImVec4          col;
                } kZoneStyles[] = {
                    { SkillTree::Zone::kCore, ImVec4{ 0.35f, 0.80f, 1.00f, 1.0f } },
                    { SkillTree::Zone::kMight, ImVec4{ 0.88f, 0.54f, 0.38f, 1.0f } },
                    { SkillTree::Zone::kShadow, ImVec4{ 0.47f, 0.78f, 0.71f, 1.0f } },
                    { SkillTree::Zone::kArcana, ImVec4{ 0.59f, 0.51f, 1.00f, 1.0f } },
                };
                // Padding in DESIGN units, scaled by the same `fit` as the positions and
                // the node radii. Unscaled padding was the bug behind overlapping frames:
                // the gaps between bands shrink with the fit while a fixed pad does not,
                // so below fit ~0.9 the frames grew into each other. See the layout note
                // in SkillTree.cpp for the clearances this relies on.
                const float zpX = 16.0f * fit;
                const float zpT = 18.0f * fit;
                const float zpB = 44.0f * fit;  // clears a two-line node name

                struct Box {
                    ImVec2 mn, mx;
                    bool   used;
                };
                Box boxes[std::size(kZoneStyles)]{};
                for (std::size_t z = 0; z < std::size(kZoneStyles); ++z) {
                    ImVec2 zMin{ FLT_MAX, FLT_MAX }, zMax{ -FLT_MAX, -FLT_MAX };
                    int    members = 0;
                    for (std::size_t i = 0; i < count; ++i) {
                        const auto& nd = nodes[i];
                        if (nd.zone != kZoneStyles[z].zone || IsUtilityNode(nd) || NodeHidden(nd)) {
                            continue;
                        }
                        const ImVec2 c = centerOf(nd);
                        const float  h = halfOf(nd);
                        zMin.x = std::min(zMin.x, c.x - h);
                        zMin.y = std::min(zMin.y, c.y - h);
                        zMax.x = std::max(zMax.x, c.x + h);
                        zMax.y = std::max(zMax.y, c.y + h);
                        ++members;
                    }
                    boxes[z] = { zMin, zMax, members > 0 };
                }

                // The three branches are siblings, so they share a top and bottom. Hugging
                // their own content made them ragged — MIGHT holds one row fewer than
                // SHADOW, so its frame ended far higher and read as a glitch.
                {
                    float top = FLT_MAX, bot = -FLT_MAX;
                    int   branches = 0;
                    for (std::size_t z = 0; z < std::size(kZoneStyles); ++z) {
                        if (!boxes[z].used || kZoneStyles[z].zone == SkillTree::Zone::kCore) {
                            continue;
                        }
                        top = std::min(top, boxes[z].mn.y);
                        bot = std::max(bot, boxes[z].mx.y);
                        ++branches;
                    }
                    if (branches > 1) {
                        for (std::size_t z = 0; z < std::size(kZoneStyles); ++z) {
                            if (!boxes[z].used || kZoneStyles[z].zone == SkillTree::Zone::kCore) {
                                continue;
                            }
                            boxes[z].mn.y = top;
                            boxes[z].mx.y = bot;
                        }
                    }
                }

                for (std::size_t z = 0; z < std::size(kZoneStyles); ++z) {
                    if (!boxes[z].used) {
                        continue;  // every node of this zone is hidden by HideSealedNodes
                    }
                    const auto&  zs = kZoneStyles[z];
                    const ImVec2 bMin{ boxes[z].mn.x - zpX, boxes[z].mn.y - zpT };
                    const ImVec2 bMax{ boxes[z].mx.x + zpX, boxes[z].mx.y + zpB };
                    dl->AddRectFilled(bMin, bMax, Style::Col(zs.col, 0.05f * fade), 10.0f * s);
                    dl->AddRect(bMin, bMax, Style::Col(zs.col, 0.20f * fade), 10.0f * s, 0, 1.0f * s);

                    const char*  zn = SkillTree::ZoneName(zs.zone);
                    const ImVec2 ts = ImGui::CalcTextSize(zn);
                    const ImVec2 tp{ (bMin.x + bMax.x) * 0.5f - ts.x * 0.5f, bMin.y - ts.y * 0.5f };
                    // Punch the label out of the frame line so the text stays readable.
                    dl->AddRectFilled(ImVec2{ tp.x - 8.0f * s, tp.y },
                                      ImVec2{ tp.x + ts.x + 8.0f * s, tp.y + ts.y },
                                      ImGui::GetColorU32(ImVec4{ Style::kPanelBg.x, Style::kPanelBg.y,
                                                                 Style::kPanelBg.z, fade }));
                    dl->AddText(tp, Style::Col(zs.col, 0.9f * fade), zn);
                }
            }

            // Connections. Endpoints are clipped to the node boxes' edges: aimed at the
            // centres, the lines ran underneath the icons — and since the icon PNGs have
            // transparent corners, they stayed visible "through" the artwork.
            const auto clipToBox = [&](ImVec2 a_from, ImVec2 a_to, float a_half) {
                const float dx = a_to.x - a_from.x;
                const float dy = a_to.y - a_from.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len < 1.0f) {
                    return a_from;
                }
                const float nx = dx / len;
                const float ny = dy / len;
                // Distance from the centre to the square's edge along this direction.
                const float edge = (a_half + 2.0f * s) / std::max(std::abs(nx), std::abs(ny));
                return ImVec2{ a_from.x + nx * edge, a_from.y + ny * edge };
            };

            // Mastery nodes never carry a prereq (that is their definition), so this
            // loop only ever draws branch-graph connections — nothing to skip.
            for (std::size_t i = 0; i < count; ++i) {
                const auto& node = nodes[i];
                if (NodeHidden(node)) {
                    continue;
                }
                for (const auto prereqKey : node.prereq) {
                    if (prereqKey == 0) {
                        continue;
                    }
                    const SkillTree::Node* from = nullptr;
                    for (std::size_t j = 0; j < count; ++j) {
                        if (nodes[j].key == prereqKey) {
                            from = &nodes[j];
                            break;
                        }
                    }
                    if (!from || NodeHidden(*from)) {
                        continue;
                    }
                    const bool   litFrom = SkillTree::IsUnlocked(from->key);
                    const bool   litBoth = litFrom && SkillTree::IsUnlocked(node.key);
                    const ImVec2 a = centerOf(*from);
                    const ImVec2 b = centerOf(node);
                    const ImVec2 p = clipToBox(a, b, halfOf(*from));
                    const ImVec2 q = clipToBox(b, a, halfOf(node));
                    if (litBoth) {
                        // A fully-unlocked path: a wide faint halo under a bright core,
                        // so active connections read as glowing energy.
                        dl->AddLine(p, q, Style::Col(Style::kAccent, 0.18f * fade), 6.0f * s);
                        dl->AddLine(p, q, Style::Col(Style::kAccent, 0.95f * fade), 2.4f * s);
                    } else {
                        dl->AddLine(p, q, Style::Col(Style::kAccent, (litFrom ? 0.4f : 0.15f) * fade),
                                    1.5f * s);
                    }
                }
            }

            // Draws one node into a_dl (the caller's active draw list — the window's for
            // the branch graph, the rail child's for mastery rows, so scrolling actually
            // clips them) and dispatches its purchase. The hit rect and the icon rect are
            // separate so a mastery row can be clickable across its whole width while its
            // icon stays a small square. Shared by both, so the look and the buy rule can
            // never drift apart.
            g_hovered = 0;
            const auto drawNode = [&](ImDrawList* a_dl, const SkillTree::Node& node, ImVec2 hitMin,
                                      ImVec2 hitMax, ImVec2 nMin, ImVec2 nMax, bool withLabel) {
                ImGui::SetCursorScreenPos(hitMin);
                ImGui::PushID(static_cast<int>(node.key));
                ImGui::InvisibleButton("##node", ImVec2{ hitMax.x - hitMin.x, hitMax.y - hitMin.y });
                const bool hovered = ImGui::IsItemHovered();
                const bool clicked = ImGui::IsItemClicked();
                ImGui::PopID();

                if (hovered) {
                    g_hovered = node.key;
                    g_hoverMin = hitMin;
                    g_hoverMax = hitMax;
                }

                const auto visual = VisualFor(node, s);

                if (visual.glow) {
                    Style::DrawGlowBorder(a_dl, nMin, nMax, Style::kAccent, 0.6f * fade, 4,
                                          1.5f * s);
                }
                // Opaque backdrop: any line crossing near the node must terminate
                // visually at the box, not shimmer through the icon's transparency.
                a_dl->AddRectFilled(nMin, nMax,
                                    ImGui::GetColorU32(ImVec4{ Style::kPanelBg.x, Style::kPanelBg.y,
                                                               Style::kPanelBg.z, fade }));
                const std::string icon = std::string(kIconDir) + node.icon;
                if (const auto tex = GetTexture(icon)) {
                    a_dl->AddImage(tex, nMin, nMax, ImVec2{ 0, 0 }, ImVec2{ 1, 1 },
                                   visual.iconTint);
                }
                a_dl->AddRect(nMin, nMax, visual.ring, 0.0f, 0, visual.ringThickness);
                if (hovered) {
                    a_dl->AddRect(ImVec2{ hitMin.x - 3.0f * s, hitMin.y - 3.0f * s },
                                  ImVec2{ hitMax.x + 3.0f * s, hitMax.y + 3.0f * s },
                                  Style::Col(Style::kText, 0.9f), 0.0f, 0, 1.0f * s);
                }

                // The always-on name under a graph tile. Hovering for a tooltip to find
                // out what any of 15 identical dark squares even is was the single
                // biggest readability problem the tree had.
                if (withLabel) {
                    ImFont*     font = ImGui::GetFont();
                    const float lblSize = std::max(11.0f * s, ImGui::GetFontSize() * 0.68f);
                    const float wrapW = 124.0f * s;
                    const ImVec2 ls = font->CalcTextSizeA(lblSize, FLT_MAX, wrapW, node.name);
                    a_dl->AddText(font, lblSize,
                                  ImVec2{ (nMin.x + nMax.x) * 0.5f - ls.x * 0.5f, nMax.y + 7.0f * s },
                                  visual.label, node.name, nullptr, wrapW);
                }

                // Mastery-tier pip, bottom-right corner — the ImGui equivalent of the web
                // patch's escalating rank badge. Non-mastery repeatables (Perk Synthesis)
                // fall back to the plain purchase count.
                if (node.repeatable) {
                    if (const std::int32_t rank = SkillTree::Rank(node.key); rank > 0) {
                        const std::int32_t tier = SkillTree::Tier(node.key);
                        const std::string  label =
                            tier > 0 ? std::string(kRomanTiers[tier - 1]) : std::to_string(rank);
                        const ImU32  pipCol = TierPipColor(tier, fade);
                        const ImVec2 txt = ImGui::CalcTextSize(label.c_str());
                        const ImVec2 pipSize{ std::max(20.0f * s, txt.x + 10.0f * s), 20.0f * s };
                        const ImVec2 pMax{ nMax.x + 7.0f * s, nMax.y + 7.0f * s };
                        const ImVec2 pMin{ pMax.x - pipSize.x, pMax.y - pipSize.y };
                        a_dl->AddRectFilled(pMin, pMax, ImGui::GetColorU32(ImVec4{ 0.05f, 0.09f, 0.13f, fade }),
                                            3.0f * s);
                        a_dl->AddRect(pMin, pMax, pipCol, 3.0f * s, 0, 1.0f * s);
                        a_dl->AddText(ImVec2{ pMin.x + (pipSize.x - txt.x) * 0.5f,
                                              pMin.y + (pipSize.y - txt.y) * 0.5f },
                                      pipCol, label.c_str());
                    }
                }

                if (clicked && !NodeOwned(node) && SkillTree::PrereqsMet(node.key) &&
                    SkillTree::TierMet(node.key) &&
                    SkillTree::Points() >= SkillTree::NextCost(node.key)) {
                    const auto key = node.key;
                    if (auto* task = SKSE::GetTaskInterface()) {
                        task->AddTask([key]() { SkillTree::TryUnlock(key); });
                    }
                }
            };

            // Branch-graph nodes.
            for (std::size_t i = 0; i < count; ++i) {
                const auto& node = nodes[i];
                if (NodeHidden(node) || IsUtilityNode(node)) {
                    continue;
                }
                const ImVec2 c = centerOf(node);
                const float  h = halfOf(node);
                const ImVec2 nMin{ c.x - h, c.y - h }, nMax{ c.x + h, c.y + h };
                drawNode(dl, node, nMin, nMax, nMin, nMax, /*withLabel=*/true);
            }

            // --- Mastery rail: the always-open repeatable stats, as named rows in their
            // own scrolling column. Bare icons in a fixed coordinate list (the old
            // approach) told the player neither what a stat was nor how far along it
            // was, and ran off the 760px canvas as more were added; rows in a scrolling
            // child have neither problem.
            {
                const float railL = wMin.x + 22.0f * s;
                const float railR = wMin.x + kRailW * s;
                const float railTop = headBottom;         // under the points block, not over it
                const float railBot = wMax.y - 88.0f * s;  // same footer reserve as the graph

                const char*  head = "MASTERY";
                const ImVec2 hs = ImGui::CalcTextSize(head);
                dl->AddText(ImVec2{ railL + 2.0f * s, railTop },
                            Style::Col(Style::kTextDim, 0.9f * fade), head);
                dl->AddLine(ImVec2{ railR + 6.0f * s, railTop }, ImVec2{ railR + 6.0f * s, railBot },
                            Style::Col(Style::kAccent, 0.16f * fade), 1.0f * s);

                const float childTop = railTop + hs.y + 10.0f * s;
                ImGui::SetCursorScreenPos(ImVec2{ railL, childTop });
                ImGui::BeginChild("##masteryRail", ImVec2{ railR - railL, railBot - childTop },
                                  false);
                ImDrawList* railDl = ImGui::GetWindowDrawList();
                ImFont*     font = ImGui::GetFont();
                const float nameSize = std::max(11.0f * s, ImGui::GetFontSize() * 0.68f);
                const float iconSz = 38.0f * s;
                const float rowH = iconSz + 16.0f * s;

                float y = ImGui::GetCursorScreenPos().y;
                for (std::size_t i = 0; i < count; ++i) {
                    const auto& node = nodes[i];
                    if (NodeHidden(node) || !IsUtilityNode(node)) {
                        continue;
                    }
                    const ImVec2 hitMin{ railL, y };
                    const ImVec2 hitMax{ railR - 6.0f * s, y + iconSz };
                    const ImVec2 nMin{ railL + 2.0f * s, y };
                    const ImVec2 nMax{ nMin.x + iconSz, y + iconSz };
                    drawNode(railDl, node, hitMin, hitMax, nMin, nMax, /*withLabel=*/false);

                    const auto  visual = VisualFor(node, s);
                    const float tx = nMax.x + 10.0f * s;
                    railDl->AddText(font, nameSize, ImVec2{ tx, y + 1.0f * s }, visual.label,
                                    node.name);

                    // Ten rank pips, split into the five tiers by a wider gap — the same
                    // read as the web rail's .mPips.
                    const std::int32_t rank = SkillTree::Rank(node.key);
                    const std::int32_t maxRank = node.maxRank > 0 ? node.maxRank : 10;
                    // Narrow enough that pips + the tier tag both fit the rail width.
                    const float        pipW = 5.0f * s, pipH = 4.0f * s, pipGap = 1.0f * s;
                    float              px = tx;
                    const float        py = y + iconSz - 13.0f * s;
                    for (std::int32_t r = 0; r < maxRank; ++r) {
                        const bool on = r < rank;
                        railDl->AddRectFilled(
                            ImVec2{ px, py }, ImVec2{ px + pipW, py + pipH },
                            on ? Style::Col(Style::kAccent, fade)
                               : Style::Col(Style::kTextDim, 0.3f * fade),
                            1.0f);
                        px += pipW + pipGap + ((r % 2 == 1 && r != maxRank - 1) ? 3.0f * s : 0.0f);
                    }

                    // Tier name once bought; the next price while still at rank 0.
                    const std::int32_t tier = SkillTree::Tier(node.key);
                    const std::string  tag =
                        rank > 0 ? std::string(SkillTree::TierName(tier))
                                 : (std::to_string(SkillTree::NextCost(node.key)) + " SP");
                    const ImVec2 tgs = font->CalcTextSizeA(nameSize, FLT_MAX, 0.0f, tag.c_str());
                    railDl->AddText(font, nameSize,
                                    ImVec2{ hitMax.x - tgs.x - 2.0f * s, py - 3.0f * s },
                                    rank > 0 ? TierPipColor(tier, fade)
                                             : Style::Col(Style::kTextDim, 0.8f * fade),
                                    tag.c_str());

                    y += rowH;
                }
                // Registers the scrolled-past extent, so the scrollbar range matches the
                // real content even when the last row is below the fold.
                ImGui::SetCursorScreenPos(ImVec2{ railL, y });
                ImGui::Dummy(ImVec2{ 1.0f, 1.0f });
                ImGui::EndChild();
            }

            // --- Hover card, floating beside the node ---
            // Was a fixed band along the bottom of the window, which cost the graph 150px
            // of height for something only visible on hover. Placed by the node instead,
            // like the web view's tooltip.
            if (g_hovered != 0) {
                const SkillTree::Node* node = nullptr;
                for (std::size_t i = 0; i < count; ++i) {
                    if (nodes[i].key == g_hovered) {
                        node = &nodes[i];
                        break;
                    }
                }
                if (node) {
                    // Repeatable nodes carry a "RANK n / max" suffix so the reader can see
                    // how far they've pushed it (max shown only when capped). Mastery
                    // nodes (capped repeatables) additionally name their tier.
                    const std::int32_t tier = SkillTree::Tier(node->key);
                    const std::string  rankTag =
                        node->repeatable
                            ? "   RANK " + std::to_string(SkillTree::Rank(node->key)) +
                                  (node->maxRank > 0 ? " / " + std::to_string(node->maxRank) : "") +
                                  (tier > 0 ? std::string("  [") + SkillTree::TierName(tier) + "]"
                                            : std::string{})
                            : std::string{};
                    const std::int32_t nextCost = SkillTree::NextCost(node->key);

                    std::string status;
                    ImU32       statusCol;
                    if (node->repeatable && NodeMaxed(*node)) {
                        status = (tier > 0 ? "GRANDMASTER" : "MAXED") + rankTag;
                        statusCol = Style::Col(Style::kAccent, fade);
                    } else if (!node->repeatable && SkillTree::IsUnlocked(node->key)) {
                        status = "UNLOCKED";
                        statusCol = Style::Col(Style::kAccent, fade);
                    } else if (!SkillTree::TierMet(node->key)) {
                        // A gift of a deeper blessing than this life took — or, for a
                        // dormant one, than it has grown into YET.
                        const auto req = SkillTree::RequiredPower(node->key);
                        const auto at = Isekai::AwakeningLevelFor(req);
                        status = at > 0
                                     ? "SEALED — awakens at level " + std::to_string(at)
                                     : "SEALED — requires " + Isekai::PowerName(req) + " rebirth";
                        statusCol = IM_COL32(200, 150, 90, static_cast<int>(255 * fade));
                    } else if (!SkillTree::PrereqsMet(node->key)) {
                        status = "LOCKED — requires a connected node";
                        statusCol = Style::Col(Style::kTextDim, fade);
                    } else if (node->effect == SkillTree::Effect::kPerkPoint &&
                               SkillTree::PerkPool() >= Isekai::kMaxPerkPoints) {
                        status = "PERK POOL FULL — spend some perk points first";
                        statusCol = Style::Col(Style::kTextDim, fade);
                    } else {
                        status = "COST   " + std::to_string(nextCost) + " system point(s)" +
                                 rankTag;
                        statusCol = SkillTree::Points() >= nextCost
                                        ? Style::Col(Style::kAccent, fade)
                                        : IM_COL32(220, 90, 90, static_cast<int>(255 * fade));
                    }

                    // Lay the card out from its own contents, then place it beside the
                    // node — flipped to the other side or clamped when it would leave the
                    // window, so a node near an edge still shows a readable card.
                    const float padCard = 14.0f * s;
                    const float wrapW = 300.0f * s;
                    ImFont*     font = ImGui::GetFont();
                    const float fs = ImGui::GetFontSize();
                    const float nameSz = fs;
                    const float bodySz = std::max(12.0f * s, fs * 0.8f);

                    const ImVec2 nameDim = font->CalcTextSizeA(nameSz, FLT_MAX, wrapW, node->name);
                    const ImVec2 descDim = font->CalcTextSizeA(bodySz, FLT_MAX, wrapW, node->desc);
                    const ImVec2 statDim =
                        font->CalcTextSizeA(bodySz, FLT_MAX, wrapW, status.c_str());

                    const float cardW =
                        std::max({ nameDim.x, descDim.x, statDim.x }) + padCard * 2.0f;
                    const float cardH = nameDim.y + descDim.y + statDim.y + padCard * 2.0f +
                                        16.0f * s;

                    float cx = g_hoverMax.x + 16.0f * s;
                    if (cx + cardW > wMax.x - 12.0f * s) {
                        cx = g_hoverMin.x - 16.0f * s - cardW;  // flip to the left
                    }
                    cx = std::clamp(cx, wMin.x + 12.0f * s, wMax.x - cardW - 12.0f * s);
                    float cy = (g_hoverMin.y + g_hoverMax.y) * 0.5f - cardH * 0.5f;
                    cy = std::clamp(cy, wMin.y + 12.0f * s, wMax.y - cardH - 12.0f * s);

                    const ImVec2 cMin{ cx, cy };
                    const ImVec2 cMax{ cx + cardW, cy + cardH };
                    dl->AddRectFilled(cMin, cMax,
                                      ImGui::GetColorU32(ImVec4{ 0.04f, 0.08f, 0.14f, 0.97f * fade }),
                                      6.0f * s);
                    dl->AddRect(cMin, cMax, Style::Col(Style::kAccent, 0.45f * fade), 6.0f * s, 0,
                                1.0f * s);

                    float ty = cMin.y + padCard;
                    dl->AddText(font, nameSz, ImVec2{ cMin.x + padCard, ty },
                                Style::Col(Style::kAccent, fade), node->name, nullptr, wrapW);
                    ty += nameDim.y + 8.0f * s;
                    dl->AddText(font, bodySz, ImVec2{ cMin.x + padCard, ty },
                                Style::Col(Style::kText, 0.92f * fade), node->desc, nullptr, wrapW);
                    ty += descDim.y + 8.0f * s;
                    dl->AddText(font, bodySz, ImVec2{ cMin.x + padCard, ty }, statusCol,
                                status.c_str(), nullptr, wrapW);
                }
            }

            // --- Close button, bottom-right ---
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
                ImGui::SetCursorScreenPos(ImVec2{ wMax.x - btnSize.x - 24.0f * s,
                                                  wMax.y - btnSize.y - 20.0f * s });
                if (ImGui::Button("CLOSE", btnSize)) {
                    RequestClose();
                }

                // Respec, left of CLOSE. Two-click confirm rather than a nested dialog:
                // the first press arms it, a second within a few seconds commits.
                if (const std::int32_t refund = SkillTree::RespecRefund(); refund > 0) {
                    const bool        armed = g_elapsed < g_respecArmedUntil;
                    const std::string label =
                        armed ? "CONFIRM?" : ("RESPEC  +" + std::to_string(refund));
                    ImGui::SetCursorScreenPos(ImVec2{ wMax.x - btnSize.x * 2.0f - 34.0f * s,
                                                      wMax.y - btnSize.y - 20.0f * s });
                    if (ImGui::Button(label.c_str(), btnSize)) {
                        if (armed) {
                            g_respecArmedUntil = 0.0f;
                            if (auto* task = SKSE::GetTaskInterface()) {
                                task->AddTask([]() { SkillTree::Respec(); });
                            }
                        } else {
                            g_respecArmedUntil = g_elapsed + 3.0f;
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(
                            "Refunds the System Points spent on stat nodes.\n"
                            "Knowledge unlocks and Perk Synthesis stay — the System\n"
                            "cannot un-teach what you already know.");
                    }
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
