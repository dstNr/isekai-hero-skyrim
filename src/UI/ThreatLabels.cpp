#include "UI/ThreatLabels.h"

#include "Config.h"
#include "UI/Overlay.h"
#include "UI/Style.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Isekai::UI {

    namespace {
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
                return { "TRIVIAL", ImVec4{ 0.55f, 0.62f, 0.68f, 1.0f } };
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

        // Where the label floats: over the actor's head, from its own bounds rather than
        // a guessed height — a mudcrab and a giant are not the same distance up.
        [[nodiscard]] bool HeadPoint(RE::Actor* a_actor, RE::NiPoint3& a_out) {
            auto* root = a_actor->Get3D();
            if (!root) {
                return false;
            }
            const auto& bound = root->worldBound;
            if (bound.radius <= 0.0f) {
                return false;
            }
            a_out = bound.center;
            a_out.z += bound.radius * 0.9f;
            return true;
        }

        // Should this actor carry a label at all?
        [[nodiscard]] bool Eligible(RE::Actor* a_actor, RE::PlayerCharacter* a_player,
                                    bool a_hostileOnly) {
            if (!a_actor || a_actor == a_player || a_actor->IsDead() || a_actor->IsDisabled()) {
                return false;
            }
            if (a_actor->IsPlayerTeammate()) {
                return false;  // your own followers are not a threat to read
            }
            if (!a_hostileOnly) {
                return true;
            }
            // "Enemies" rather than "everyone": a market square should not fill up with
            // labels over the fishmonger. IsHostileToActor covers a faction that already
            // hates you; IsInCombat covers the one that just decided to.
            return a_actor->IsHostileToActor(a_player) || a_actor->IsInCombat();
        }

        // A label the frame can draw: already projected, already judged.
        struct Label {
            ImVec2      pos;
            float       distance;
            Verdict     verdict;
            std::string name;
        };

        // A cap, because "all visible enemies" during a dragon attack on a city is not a
        // number anyone chose. The nearest ones are the ones that matter.
        constexpr std::size_t kMaxLabels = 12;
    }

    void DrawThreatLabels() {
        if (!Config::ThreatLabels() || !OverlayReady()) {
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
        const bool   hostileOnly = mode == Config::ThreatTargets::kHostile;
        const float  maxRange = static_cast<float>(Config::ThreatLabelRange());
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const auto   playerLevel = static_cast<std::int32_t>(player->GetLevel());
        const auto   playerPos = player->GetPosition();

        std::vector<Label> labels;

        const auto consider = [&](RE::Actor* actor) {
            if (!Eligible(actor, player, hostileOnly)) {
                return;
            }
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
            const auto diff = static_cast<std::int32_t>(actor->GetLevel()) - playerLevel;
            labels.push_back({ screen, distance, VerdictFor(diff),
                               actor->GetDisplayFullName() ? actor->GetDisplayFullName() : "" });
        };

        if (crosshairOnly) {
            auto* pick = RE::CrosshairPickData::GetSingleton();
            auto* ref = pick ? pick->target.get().get() : nullptr;
            consider(ref ? ref->As<RE::Actor>() : nullptr);
        } else if (auto* lists = RE::ProcessLists::GetSingleton()) {
            for (const auto& handle : lists->highActorHandles) {
                if (auto actor = handle.get()) {
                    consider(actor.get());
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

        for (const auto& label : labels) {
            // Distant labels shrink and dim, so the near ones stay dominant and a crowd
            // reads as depth rather than as noise.
            const float t = std::clamp(label.distance / maxRange, 0.0f, 1.0f);
            const float size = (19.0f - 6.0f * t) * s;
            const float alpha = 1.0f - 0.55f * t;

            const ImVec2 dim = font ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, label.verdict.tag)
                                    : ImGui::CalcTextSize(label.verdict.tag);
            const ImVec2 pos{ label.pos.x - dim.x * 0.5f, label.pos.y - dim.y };

            const ImVec2 pad{ 7.0f * s, 3.0f * s };
            dl->AddRectFilled(ImVec2{ pos.x - pad.x, pos.y - pad.y },
                              ImVec2{ pos.x + dim.x + pad.x, pos.y + dim.y + pad.y },
                              ImGui::GetColorU32(ImVec4{ 0.02f, 0.04f, 0.07f, 0.62f * alpha }));
            if (font) {
                dl->AddText(font, size, pos, Style::Col(label.verdict.col, alpha),
                            label.verdict.tag);
            } else {
                dl->AddText(pos, Style::Col(label.verdict.col, alpha), label.verdict.tag);
            }
        }
    }
}
