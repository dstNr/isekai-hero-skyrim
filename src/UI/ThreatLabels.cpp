#include "UI/ThreatLabels.h"

#include "Config.h"
#include "UI/Input.h"
#include "UI/Overlay.h"
#include "UI/Style.h"
#include "UI/Toast.h"

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <string>
#include <vector>

namespace Isekai::UI {

    namespace {
        // What the on/off key has done to the ini's setting, this session only.
        // -1 = untouched, follow the ini; 0 = forced off; 1 = forced on.
        //
        // A tri-state rather than a bool copied out of the ini, so the key works in both
        // directions: someone who left ThreatLabels = 0 can still switch them on for one
        // fight, and someone who left them on can silence them for a screenshot.
        // Read on the render thread, written on the main thread — hence the atomic.
        std::atomic<int> g_override{ -1 };

        // The verdict, and what it looks like. Four bands is enough to read as a System
        // judgement without pretending to be a combat simulator; the colours are the
        // whole point of moving this out of a text panel, so they carry the meaning
        // before the word is read.
        struct Verdict {
            const char* tag;
            ImVec4      col;
        };

        [[nodiscard]] Verdict VerdictFor(std::int32_t a_diff) {
            if (a_diff >= 20) {
                return { "LETHAL", ImVec4{ 1.00f, 0.28f, 0.28f, 1.0f } };
            }
            if (a_diff >= 5) {
                return { "DANGEROUS", ImVec4{ 1.00f, 0.68f, 0.25f, 1.0f } };
            }
            if (a_diff <= -15) {
                return { "TRIVIAL", ImVec4{ 0.80f, 0.86f, 0.92f, 1.0f } };
            }
            return { "MANAGEABLE", ImVec4{ 0.55f, 0.85f, 1.00f, 1.0f } };
        }

        // The scene camera, which is a child of the camera root rather than the root
        // itself. PlayerCamera::cameraRoot is an NiNode; the NiCamera hangs off it.
        [[nodiscard]] RE::NiCamera* SceneCamera() {
            auto* camera = RE::PlayerCamera::GetSingleton();
            if (!camera || !camera->cameraRoot) {
                return nullptr;
            }
            for (const auto& child : camera->cameraRoot->GetChildren()) {
                if (auto* cam = netimmerse_cast<RE::NiCamera*>(child.get())) {
                    return cam;
                }
            }
            return nullptr;
        }

        // World point -> screen pixels. Returns false when the point is behind the camera
        // or off screen, which is most of them most of the time.
        [[nodiscard]] bool Project(RE::NiCamera* a_cam, const RE::NiPoint3& a_world,
                                   const ImVec2& a_display, ImVec2& a_out) {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            RE::NiCamera::WorldPtToScreenPt3(a_cam->GetRuntimeData().worldToCam,
                                             a_cam->GetRuntimeData2().port, a_world, x, y, z,
                                             1e-5f);
            if (z <= 0.0f) {
                return false;  // behind the lens
            }
            // The port is normalised with y running from the bottom; screen space is the
            // other way up.
            a_out = ImVec2{ x * a_display.x, (1.0f - y) * a_display.y };
            return a_out.x > -200.0f && a_out.x < a_display.x + 200.0f && a_out.y > -100.0f &&
                   a_out.y < a_display.y + 100.0f;
        }

        // Where the label floats: over the actor's head, from its own height rather than
        // a guessed one — a mudcrab and a giant are not the same distance up.
        //
        // The height is Actor::GetHeight (the base object's bounds times its scale), and
        // the anchor is the 3D root, which is the actor's PLACEMENT — the pose hangs off
        // it. Neither moves when the actor animates.
        //
        // What this replaced was `worldBound.center + radius * 0.9`, and worldBound is
        // recomputed from the current pose: it swells when a bandit swings, when a wolf
        // leaps, when anything draws a weapon, and its centre bobs with every stride. A
        // label anchored to it hops around the head instead of sitting over it, which is
        // the "the danger level jumps about when the mob moves" this fixes. The animated
        // bound is kept only as a fallback for actors whose base carries no usable one.
        [[nodiscard]] bool HeadPoint(RE::Actor* a_actor, RE::NiPoint3& a_out) {
            auto* root = a_actor->Get3D();
            if (!root) {
                return false;
            }
            // 8 units is roughly a knee-high crate: below that the bound is missing
            // rather than small, and the label would sit inside the actor.
            if (const float height = a_actor->GetHeight(); height > 8.0f) {
                a_out = root->world.translate;
                a_out.z += height * 1.05f;
                return true;
            }
            const auto& bound = root->worldBound;
            if (bound.radius <= 0.0f) {
                return false;
            }
            a_out = bound.center;
            a_out.z += bound.radius * 0.9f;
            return true;
        }

        // Is this actor actually FIGHTING — and fighting us, rather than a mudcrab it
        // found on the way?
        //
        // IsHostileToActor alone is not that question. It is true of every bandit,
        // draugr and wolf in the cell the moment it loads, asleep or not, which is what
        // made the default read as "a label over everything I walk past". Aggro is a
        // state, not a disposition: the actor is in combat AND we (or someone of ours)
        // are what it is in combat with.
        [[nodiscard]] bool FightingUs(RE::Actor* a_actor, RE::PlayerCharacter* a_player) {
            if (!a_actor->IsInCombat()) {
                return false;
            }
            const auto target = a_actor->GetActorRuntimeData().currentCombatTarget.get();
            if (!target) {
                // In combat with something we cannot resolve. Err towards showing it if
                // it hates us anyway — a hostile that has drawn a weapon is exactly the
                // case this mode exists for, and losing it would be the worse mistake.
                return a_actor->IsHostileToActor(a_player);
            }
            return target.get() == a_player || target->IsPlayerTeammate();
        }

        // Should this actor carry a label at all?
        [[nodiscard]] bool Eligible(RE::Actor* a_actor, RE::PlayerCharacter* a_player,
                                    Config::ThreatTargets a_mode) {
            if (!a_actor || a_actor == a_player || a_actor->IsDead() || a_actor->IsDisabled()) {
                return false;
            }
            if (a_actor->IsPlayerTeammate()) {
                return false;  // your own followers are not a threat to read
            }
            switch (a_mode) {
            case Config::ThreatTargets::kAll:
                return true;
            case Config::ThreatTargets::kHostile:
                // Every enemy in range, noticed or not — the old default, kept for anyone
                // who wants to read a room before walking into it.
                return a_actor->IsHostileToActor(a_player) || a_actor->IsInCombat();
            case Config::ThreatTargets::kCrosshair:
                return true;  // the crosshair already picked exactly one
            default:
                // kAggro: the crosshair contributes separately, so all this decides is
                // whether something is worth labelling unprompted.
                return FightingUs(a_actor, a_player);
            }
        }

        // A label the frame can draw: already projected, already judged.
        struct Label {
            ImVec2       pos;
            float        distance;
            Verdict      verdict;
            std::int32_t level;
            float        health;  // 0..1
            std::string  name;
        };

        // Current health as a fraction. The frame draws a bar, and a bar without data
        // behind it is decoration pretending to be information.
        [[nodiscard]] float HealthFraction(RE::Actor* a_actor) {
            auto* av = a_actor->AsActorValueOwner();
            if (!av) {
                return 1.0f;
            }
            const float now = av->GetActorValue(RE::ActorValue::kHealth);
            const float max = av->GetPermanentActorValue(RE::ActorValue::kHealth);
            return max > 0.0f ? std::clamp(now / max, 0.0f, 1.0f) : 1.0f;
        }

        // A cap, because "all visible enemies" during a dragon attack on a city is not a
        // number anyone chose. The nearest ones are the ones that matter.
        constexpr std::size_t kMaxLabels = 12;
    }

    void InstallThreatLabels() {
        if (REL::Module::IsVR()) {
            logger::info("UI: no threat-label key in VR — the overlay that draws them is "
                         "SE/AE only");
            return;
        }
        const auto key = Config::ThreatLabelKey();
        if (key == 0) {
            logger::info("UI: no threat-label key (ThreatLabelKey = 0)");
            return;
        }
        RegisterHotkey(key, []() { ToggleThreatLabels(); });
        logger::info("UI: {} toggles the threat labels (currently {})", Config::KeyName(key),
                     ThreatLabelsVisible() ? "on" : "off");
    }

    bool ThreatLabelsVisible() {
        const int forced = g_override.load(std::memory_order_acquire);
        return forced < 0 ? Config::ThreatLabels() : forced == 1;
    }

    void ToggleThreatLabels() {
        const bool on = !ThreatLabelsVisible();
        g_override.store(on ? 1 : 0, std::memory_order_release);
        logger::info("UI: threat labels switched {}", on ? "on" : "off");
        ShowToast(on ? "Threat display  ON" : "Threat display  OFF", "threat");
    }

    void DrawThreatLabels() {
        if (!ThreatLabelsVisible() || !OverlayReady()) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) {
            return;
        }
        // Nothing to read while a menu owns the screen, and drawing world-anchored labels
        // over an inventory would be nonsense — the camera is elsewhere.
        if (auto* ui = RE::UI::GetSingleton(); !ui || ui->GameIsPaused()) {
            return;
        }
        auto* cam = SceneCamera();
        if (!cam) {
            return;
        }

        const auto   mode = Config::ThreatLabelTargets();
        const bool   crosshairOnly = mode == Config::ThreatTargets::kCrosshair;
        const float  maxRange = static_cast<float>(Config::ThreatLabelRange());
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const auto   playerLevel = static_cast<std::int32_t>(player->GetLevel());
        const auto   playerPos = player->GetPosition();

        std::vector<Label>      labels;
        std::vector<RE::Actor*> seen;  // the crosshair target is usually also in the sweep

        const auto considerAs = [&](RE::Actor* actor, Config::ThreatTargets rule) {
            if (!Eligible(actor, player, rule)) {
                return;
            }
            if (std::find(seen.begin(), seen.end(), actor) != seen.end()) {
                return;
            }
            seen.push_back(actor);
            const float distance = playerPos.GetDistance(actor->GetPosition());
            if (distance > maxRange) {
                return;
            }
            RE::NiPoint3 head{};
            if (!HeadPoint(actor, head)) {
                return;
            }
            ImVec2 screen{};
            if (!Project(cam, head, display, screen)) {
                return;
            }
            const auto level = static_cast<std::int32_t>(actor->GetLevel());
            const char* name = actor->GetDisplayFullName();
            labels.push_back({ screen, distance, VerdictFor(level - playerLevel), level,
                               HealthFraction(actor), name ? name : "" });
        };

        const auto consider = [&](RE::Actor* actor) { considerAs(actor, mode); };

        // What you are aiming at is labelled in both crosshair modes, and it bypasses the
        // sweep's filter on purpose: pointing at something IS the request to read it,
        // whether or not that thing has noticed you yet.
        //
        // Found by projection rather than by CrosshairPickData: that only resolves a
        // target within ACTIVATION range, so it never sees the wolf you are lining up
        // across a clearing — which is exactly when you want the reading.
        if (crosshairOnly || mode == Config::ThreatTargets::kAggro) {
            // ponytail: fixed screen-space cone, not a real ray. Picks the wrong actor
            // only when two overlap near the centre; raycast if that ever matters.
            float      best = display.y * 0.08f;
            RE::Actor* aimed = nullptr;
            if (auto* lists = RE::ProcessLists::GetSingleton()) {
                for (const auto& handle : lists->highActorHandles) {
                    auto actor = handle.get();
                    if (!actor || !Eligible(actor.get(), player, Config::ThreatTargets::kAll) ||
                        playerPos.GetDistance(actor->GetPosition()) > maxRange) {
                        continue;
                    }
                    RE::NiPoint3 head{};
                    ImVec2       screen{};
                    if (!HeadPoint(actor.get(), head) || !Project(cam, head, display, screen)) {
                        continue;
                    }
                    const float d = std::hypot(screen.x - display.x * 0.5f,
                                               screen.y - display.y * 0.5f);
                    if (d < best) {
                        best = d;
                        aimed = actor.get();
                    }
                }
            }
            considerAs(aimed, Config::ThreatTargets::kCrosshair);
        }
        if (!crosshairOnly) {
            if (auto* lists = RE::ProcessLists::GetSingleton()) {
                for (const auto& handle : lists->highActorHandles) {
                    if (auto actor = handle.get()) {
                        consider(actor.get());
                    }
                }
            }
        }

        if (labels.empty()) {
            return;
        }
        // Nearest first, then cut — under a crowd the far ones are the ones to lose.
        std::sort(labels.begin(), labels.end(),
                  [](const Label& a, const Label& b) { return a.distance < b.distance; });
        if (labels.size() > kMaxLabels) {
            labels.resize(kMaxLabels);
        }

        const float s = Style::g_scale;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        ImFont*     font = Style::g_body;

        // A target frame rather than two lines of text: an angular plate leaning right,
        // the level in a disc on the left, the name across the top and a health bar under
        // it, with the verdict closing the right end. Same information as before — the
        // shape is what makes it readable at a glance instead of something you parse.
        //
        // All of it is draw-list geometry, no textures: the frame has to scale with
        // distance, and stretched art at a dozen sizes looks worse than lines do.
        const auto measure = [&](float a_size, const char* a_text) {
            return font ? font->CalcTextSizeA(a_size, FLT_MAX, 0.0f, a_text).x
                        : ImGui::CalcTextSize(a_text).x;
        };

        for (const auto& label : labels) {
            // Distant frames shrink and dim, so the near ones stay dominant and a crowd
            // reads as depth rather than as noise.
            const float t = std::clamp(label.distance / maxRange, 0.0f, 1.0f);
            const float k = (1.0f - 0.32f * t) * s;
            const float alpha = 1.0f - 0.35f * t;

            const float nameSize = 20.0f * k;
            const float tagSize = 12.0f * k;
            const float lvlSize = 15.0f * k;
            const float discR = 17.0f * k;
            const float barH = 6.0f * k;
            const float skew = 9.0f * k;

            const std::string lvl = std::to_string(label.level);
            const char*       name = label.name.empty() ? "Unknown" : label.name.c_str();
            const float       nameW = measure(nameSize, name);
            const float       tagW = measure(tagSize, label.verdict.tag);

            const float plateW =
                discR * 2.0f + 14.0f * k + std::max(nameW, 120.0f * k) + 12.0f * k + tagW +
                14.0f * k;
            const float cx = label.pos.x;
            const float y1 = label.pos.y;              // sits just above the head point
            const float y0 = y1 - discR * 2.0f;
            const float x0 = cx - plateW * 0.5f;
            const float x1 = cx + plateW * 0.5f;

            // The plate: a parallelogram, not a rectangle. The lean is the whole reason
            // this reads as a banner rather than as a tooltip.
            dl->AddQuadFilled(ImVec2{ x0 + skew, y0 }, ImVec2{ x1, y0 },
                              ImVec2{ x1 - skew, y1 }, ImVec2{ x0, y1 },
                              Style::Col(Style::kPanelBg, 0.72f * alpha));
            dl->AddLine(ImVec2{ x0 + skew, y0 }, ImVec2{ x1, y0 },
                        Style::Col(label.verdict.col, 0.55f * alpha), 1.5f * k);
            dl->AddLine(ImVec2{ x0, y1 }, ImVec2{ x1 - skew, y1 },
                        Style::Col(label.verdict.col, 0.35f * alpha), 1.0f * k);

            // Level disc on the left, in the verdict's colour — the one element you can
            // identify without reading anything.
            const ImVec2 disc{ x0 + discR + 3.0f * k, (y0 + y1) * 0.5f };
            dl->AddCircleFilled(disc, discR, Style::Col(Style::kPanelBg, 0.95f * alpha), 24);
            dl->AddCircle(disc, discR, Style::Col(label.verdict.col, alpha), 24, 2.0f * k);
            const float lvlW = measure(lvlSize, lvl.c_str());
            Style::DrawTextOutlined(dl, font, lvlSize,
                                    ImVec2{ disc.x - lvlW * 0.5f, disc.y - lvlSize * 0.62f },
                                    label.verdict.col, lvl.c_str(), alpha);

            const float textX = disc.x + discR + 11.0f * k;
            const float barR = x1 - 12.0f * k - tagW - 10.0f * k;

            Style::DrawTextOutlined(dl, font, nameSize, ImVec2{ textX, y0 + 2.0f * k },
                                    Style::kText, name, alpha);

            // Health. Empty track first so a nearly dead target still shows the frame.
            const float barY = y1 - 9.0f * k;
            dl->AddRectFilled(ImVec2{ textX, barY }, ImVec2{ barR, barY + barH },
                              Style::Col(Style::kPanelBg, 0.9f * alpha));
            dl->AddRectFilled(
                ImVec2{ textX, barY },
                ImVec2{ textX + (barR - textX) * label.health, barY + barH },
                Style::Col(label.verdict.col, 0.85f * alpha));
            dl->AddRect(ImVec2{ textX, barY }, ImVec2{ barR, barY + barH },
                        Style::Col(label.verdict.col, 0.45f * alpha), 0.0f, 0, 1.0f * k);

            Style::DrawTextOutlined(dl, font, tagSize,
                                    ImVec2{ barR + 10.0f * k, barY + barH * 0.5f - tagSize * 0.6f },
                                    label.verdict.col, label.verdict.tag, alpha);
        }
    }
}
