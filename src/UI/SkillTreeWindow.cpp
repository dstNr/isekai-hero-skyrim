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
        };

        // With HideSealedNodes set, a node gated above the current rebirth tier is not
        // drawn at all (nor its connecting lines) instead of showing greyed. Default off.
        [[nodiscard]] bool NodeHidden(const SkillTree::Node& a_node) {
            return Config::HideSealedNodes() && !SkillTree::TierMet(a_node.key);
        }

        // The always-open repeatable upgrades (no prerequisites) — grouped into their own
        // labelled column on the left, off to the side of the branch graph, so they don't
        // float there disconnected. Mirrors the web patch's utility rail.
        [[nodiscard]] bool IsUtilityNode(const SkillTree::Node& a_node) {
            return a_node.repeatable && a_node.prereq[0] == 0 && a_node.prereq[1] == 0;
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
            const bool affordable = SkillTree::Points() >= a_node.cost;

            if (unlocked) {
                return { Style::Col(Style::kAccent), 2.5f * a_s, IM_COL32_WHITE, true };
            }
            if (reachable && affordable) {
                // Breathing ring: this is the "you can take me" signal.
                const float pulse = 0.55f + 0.45f * std::sin(g_elapsed * 3.5f);
                return { Style::Col(Style::kAccent, 0.35f + 0.5f * pulse), 2.0f * a_s,
                         IM_COL32(235, 235, 235, 255), false };
            }
            if (reachable) {
                // Reachable but too poor: visible, quietly waiting.
                return { Style::Col(Style::kTextDim, 0.8f), 1.5f * a_s,
                         IM_COL32(150, 150, 150, 255), false };
            }
            if (!SkillTree::TierMet(a_node.key)) {
                // Sealed by rebirth tier — an amber lock, so it reads distinctly from a
                // plain grey "prerequisite missing" node.
                return { IM_COL32(210, 158, 96, 220), 1.5f * a_s, IM_COL32(160, 120, 74, 210),
                         false };
            }
            // Locked behind prerequisites: greyed down hard.
            return { Style::Col(Style::kTextDim, 0.35f), 1.0f * a_s,
                     IM_COL32(80, 80, 80, 200), false };
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

            // --- The graph ---
            // Utility nodes (x≈95 in the data) are nudged further left so they sit in a
            // distinct rail, clear of the branch graph's leftmost node (Thu'um at x≈160).
            constexpr float kUtilShift = 50.0f;
            const ImVec2    origin{ wMin.x, wMin.y + kCanvasTop * s };
            const auto      centerOf = [&](const SkillTree::Node& n) {
                const float x = IsUtilityNode(n) ? n.x - kUtilShift : n.x;
                return ImVec2{ origin.x + x * s, origin.y + n.y * s };
            };

            std::size_t count = 0;
            const auto* nodes = SkillTree::Nodes(count);

            // Connections first, so nodes draw over them. Endpoints are clipped to
            // the node boxes' edges: aimed at the centres, the lines ran underneath
            // the icons — and since the icon PNGs have transparent corners, they
            // stayed visible "through" the artwork.
            const float halfBox = kNodeSize * 0.5f * s + 2.0f * s;
            const auto  clipToBox = [&](ImVec2 a_from, ImVec2 a_to) {
                const float dx = a_to.x - a_from.x;
                const float dy = a_to.y - a_from.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len < 1.0f) {
                    return a_from;
                }
                const float nx = dx / len;
                const float ny = dy / len;
                // Distance from the centre to the square's edge along this direction.
                const float edge = halfBox / std::max(std::abs(nx), std::abs(ny));
                return ImVec2{ a_from.x + nx * edge, a_from.y + ny * edge };
            };

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
                    const ImVec2 p = clipToBox(a, b), q = clipToBox(b, a);
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

            // --- Utility rail: a label and a divider framing the always-open repeatable
            // column, so the three nodes read as a deliberate group rather than floaters.
            // Drawn before the nodes so they sit on top.
            {
                ImVec2 uMin{ FLT_MAX, FLT_MAX }, uMax{ -FLT_MAX, -FLT_MAX };
                int    uCount = 0;
                for (std::size_t i = 0; i < count; ++i) {
                    if (!IsUtilityNode(nodes[i]) || NodeHidden(nodes[i])) {
                        continue;
                    }
                    const ImVec2 c = centerOf(nodes[i]);
                    uMin.x = std::min(uMin.x, c.x);
                    uMin.y = std::min(uMin.y, c.y);
                    uMax.x = std::max(uMax.x, c.x);
                    uMax.y = std::max(uMax.y, c.y);
                    ++uCount;
                }
                if (uCount > 0) {
                    const float hb = kNodeSize * 0.5f * s;
                    const float divX = uMax.x + hb + 26.0f * s;
                    dl->AddLine(ImVec2{ divX, uMin.y - hb - 28.0f * s },
                                ImVec2{ divX, uMax.y + hb + 16.0f * s },
                                Style::Col(Style::kAccent, 0.16f * fade), 1.0f * s);
                    const char*  ul = "UTILITY";
                    const ImVec2 ts = ImGui::CalcTextSize(ul);
                    dl->AddText(ImVec2{ (uMin.x + uMax.x) * 0.5f - ts.x * 0.5f,
                                        uMin.y - hb - 26.0f * s },
                                Style::Col(Style::kTextDim, 0.9f * fade), ul);
                }
            }

            // Nodes.
            g_hovered = 0;
            const float half = kNodeSize * 0.5f * s;
            for (std::size_t i = 0; i < count; ++i) {
                const auto&  node = nodes[i];
                if (NodeHidden(node)) {
                    continue;
                }
                const ImVec2 c = centerOf(node);
                const ImVec2 nMin{ c.x - half, c.y - half };
                const ImVec2 nMax{ c.x + half, c.y + half };

                ImGui::SetCursorScreenPos(nMin);
                ImGui::PushID(static_cast<int>(node.key));
                ImGui::InvisibleButton("##node", ImVec2{ half * 2.0f, half * 2.0f });
                const bool hovered = ImGui::IsItemHovered();
                const bool clicked = ImGui::IsItemClicked();
                ImGui::PopID();

                if (hovered) {
                    g_hovered = node.key;
                }

                const auto visual = VisualFor(node, s);

                if (visual.glow) {
                    Style::DrawGlowBorder(dl, nMin, nMax, Style::kAccent, 0.6f * fade, 4,
                                          1.5f * s);
                }
                // Opaque backdrop: any line crossing near the node must terminate
                // visually at the box, not shimmer through the icon's transparency.
                dl->AddRectFilled(nMin, nMax,
                                  ImGui::GetColorU32(ImVec4{ Style::kPanelBg.x, Style::kPanelBg.y,
                                                             Style::kPanelBg.z, fade }));
                const std::string icon = std::string(kIconDir) + node.icon;
                if (const auto tex = GetTexture(icon)) {
                    dl->AddImage(tex, nMin, nMax, ImVec2{ 0, 0 }, ImVec2{ 1, 1 },
                                 visual.iconTint);
                }
                dl->AddRect(nMin, nMax, visual.ring, 0.0f, 0, visual.ringThickness);
                if (hovered) {
                    dl->AddRect(ImVec2{ nMin.x - 3.0f * s, nMin.y - 3.0f * s },
                                ImVec2{ nMax.x + 3.0f * s, nMax.y + 3.0f * s },
                                Style::Col(Style::kText, 0.9f), 0.0f, 0, 1.0f * s);
                }

                if (clicked && !NodeOwned(node) && SkillTree::PrereqsMet(node.key) &&
                    SkillTree::TierMet(node.key) && SkillTree::Points() >= node.cost) {
                    const auto key = node.key;
                    if (auto* task = SKSE::GetTaskInterface()) {
                        task->AddTask([key]() { SkillTree::TryUnlock(key); });
                    }
                }
            }

            // --- Hover info, bottom-left ---
            if (g_hovered != 0) {
                const SkillTree::Node* node = nullptr;
                for (std::size_t i = 0; i < count; ++i) {
                    if (nodes[i].key == g_hovered) {
                        node = &nodes[i];
                        break;
                    }
                }
                if (node) {
                    // Bottom-left, where it lived before the (since removed) soul
                    // exchange button briefly shared the corner with it.
                    const float infoX = wMin.x + 28.0f * s;
                    const float infoY = wMax.y - 118.0f * s;
                    dl->AddText(ImVec2{ infoX, infoY }, Style::Col(Style::kAccent, fade),
                                node->name);
                    dl->AddText(ImVec2{ infoX, infoY + 26.0f * s },
                                Style::Col(Style::kText, fade), node->desc);

                    // Repeatable nodes carry a "RANK n / max" suffix so the reader can see
                    // how far they've pushed it (max shown only when capped).
                    const std::string rankTag =
                        node->repeatable
                            ? "   RANK " + std::to_string(SkillTree::Rank(node->key)) +
                                  (node->maxRank > 0 ? " / " + std::to_string(node->maxRank) : "")
                            : std::string{};

                    std::string status;
                    ImU32       statusCol;
                    if (node->repeatable && NodeMaxed(*node)) {
                        status = "MAXED" + rankTag;
                        statusCol = Style::Col(Style::kAccent, fade);
                    } else if (!node->repeatable && SkillTree::IsUnlocked(node->key)) {
                        status = "UNLOCKED";
                        statusCol = Style::Col(Style::kAccent, fade);
                    } else if (!SkillTree::TierMet(node->key)) {
                        // A gift of a deeper blessing than this life took.
                        status = "SEALED — requires " +
                                 Isekai::PowerName(SkillTree::RequiredPower(node->key)) +
                                 " rebirth";
                        statusCol = IM_COL32(200, 150, 90, static_cast<int>(255 * fade));
                    } else if (!SkillTree::PrereqsMet(node->key)) {
                        status = "LOCKED — requires a connected node";
                        statusCol = Style::Col(Style::kTextDim, fade);
                    } else if (node->effect == SkillTree::Effect::kPerkPoint &&
                               SkillTree::PerkPool() >= Isekai::kMaxPerkPoints) {
                        status = "PERK POOL FULL — spend some perk points first";
                        statusCol = Style::Col(Style::kTextDim, fade);
                    } else {
                        status = "COST   " + std::to_string(node->cost) + " system point(s)" +
                                 rankTag;
                        statusCol = SkillTree::Points() >= node->cost
                                        ? Style::Col(Style::kAccent, fade)
                                        : IM_COL32(220, 90, 90, static_cast<int>(255 * fade));
                    }
                    dl->AddText(ImVec2{ infoX, infoY + 78.0f * s }, statusCol, status.c_str());
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
