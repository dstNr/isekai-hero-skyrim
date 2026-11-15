#include "UI/ThreatLabels.h"

#include "Config.h"
#include "Loc.h"
#include "UI/Hud.h"
#include "UI/Input.h"

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
        // Written on the main thread, read by the frame tick and the hotkey path.
        std::atomic<int> g_override{ -1 };

        // The verdict word. The verdict itself is Logic::VerdictFor (four bands, enough to
        // read as a System judgement without pretending to be a combat simulator); the
        // colour it carries is the page's, from Logic::ToneFor(Verdict).
        [[nodiscard]] const char* VerdictTag(Logic::Verdict a_verdict) {
            switch (a_verdict) {
                case Logic::Verdict::Lethal: return L("threat.lethal", "LETHAL");
                case Logic::Verdict::Dangerous: return L("threat.dangerous", "DANGEROUS");
                case Logic::Verdict::Trivial: return L("threat.trivial", "TRIVIAL");
                case Logic::Verdict::Manageable: break;
            }
            return L("threat.manageable", "MANAGEABLE");
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

        // The screen, in pixels, for everything below that is measured in them.
        struct Display {
            float w;
            float h;
        };

        // World point -> page space (0..1, y down). Returns false when the point is behind
        // the camera or off screen, which is most of them most of the time.
        [[nodiscard]] bool Project(RE::NiCamera* a_cam, const RE::NiPoint3& a_world,
                                   const Display& a_display, ScreenPt& a_out) {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            RE::NiCamera::WorldPtToScreenPt3(a_cam->GetRuntimeData().worldToCam,
                                             a_cam->GetRuntimeData2().port, a_world, x, y, z,
                                             1e-5f);
            if (z <= 0.0f) {
                return false;  // behind the lens
            }
            // The port is normalised with y running from the bottom; the page's runs down.
            a_out = ScreenPt{ x, Logic::PageY(y) };
            return Logic::LabelOnScreen(a_out.x, a_out.y, a_display.w, a_display.h);
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

        // Can the player actually SEE this actor?
        //
        // Nothing asked this before, so a label sat happily on a bandit through a hillside
        // — a reporter read Thadgeir's threat level through a tree and the ground, from
        // outside Falkreath. Projection only answers "is it on screen", and a wall is on
        // screen too.
        //
        // The game's own line-of-sight is the right answer rather than our own raycast: it
        // is what the AI uses to decide whether it can see you, so the label agrees with
        // the thing it is a label for.
        //
        // ponytail: one LOS call per candidate per frame. It runs last, after the mode
        // filter, the range check and the projection have already dropped nearly
        // everything, so in practice this is a handful of calls. If a crowded cell ever
        // shows up in a frame profile, cache the answer for a few frames before reaching
        // for anything cleverer — a label that lags occlusion by 50ms is unnoticeable.
        [[nodiscard]] bool Visible(RE::PlayerCharacter* a_player, RE::Actor* a_actor) {
            bool unused = false;
            return a_player->HasLineOfSight(a_actor, unused);
        }

        // Current and maximum of one pool. The page draws a bar from them, and a bar
        // without data behind it is decoration pretending to be information.
        [[nodiscard]] LabelPool PoolOf(RE::ActorValueOwner* a_av, RE::ActorValue a_stat) {
            if (!a_av) {
                return {};
            }
            const float now = a_av->GetActorValue(a_stat);
            const float max = a_av->GetPermanentActorValue(a_stat);
            return { static_cast<std::int32_t>(std::lround(std::max(now, 0.0f))),
                     static_cast<std::int32_t>(std::lround(std::max(max, 0.0f))) };
        }

        // A cap, because "all visible enemies" during a dragon attack on a city is not a
        // number anyone chose. The nearest ones are the ones that matter.
        constexpr std::size_t kMaxLabels = 12;

        [[nodiscard]] Display ScreenDisplay() {
            Display d{ 1920.0f, 1080.0f };
            LabelScreenSize(d.w, d.h);
            return d;
        }
    }

    void BindThreatLabelHotkeys(bool a_log) {
        // The key is armed in VR too: the labels exist there as the scan list, so the
        // toggle has something to toggle. A VR player without a keyboard simply never
        // presses it.
        const auto key = Config::ThreatLabelKey();
        if (key == 0) {
            if (a_log) {
                logger::info("UI: no threat-label key (ThreatLabelKey = 0)");
            }
            return;
        }
        RegisterHotkey(key, []() { ToggleThreatLabels(); });
        if (a_log) {
            logger::info("UI: {} toggles the threat labels (currently {})", Config::KeyName(key),
                         ThreatLabelsVisible() ? "on" : "off");
        }
    }

    bool ThreatLabelsVisible() {
        const int forced = g_override.load(std::memory_order_acquire);
        return forced < 0 ? Config::ThreatLabels() : forced == 1;
    }

    void ToggleThreatLabels() {
        const bool on = !ThreatLabelsVisible();
        g_override.store(on ? 1 : 0, std::memory_order_release);
        logger::info("UI: threat labels switched {}", on ? "on" : "off");
        ShowToast(on ? L("threat.toggleOn", "Threat display  ON")
                     : L("threat.toggleOff", "Threat display  OFF"),
                  "threat");
    }

    void LabelScreenSize(float& a_width, float& a_height) {
        a_width = 1920.0f;
        a_height = 1080.0f;
        if (REL::Module::IsVR()) {
            return;  // never projected there, and State's layout is the flat one
        }
        if (auto* state = RE::BSGraphics::State::GetSingleton();
            state && state->screenWidth > 0 && state->screenHeight > 0) {
            a_width = static_cast<float>(state->screenWidth);
            a_height = static_cast<float>(state->screenHeight);
        }
    }

    std::vector<LabelData> SelectLabels(bool a_project) {
        std::vector<LabelData> labels;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) {
            return labels;
        }
        // Nothing to read while a menu owns the screen, and world-anchored labels over an
        // inventory would be nonsense — the camera is elsewhere.
        if (auto* ui = RE::UI::GetSingleton(); !ui || ui->GameIsPaused()) {
            return labels;
        }
        // No camera means VR (or none yet): range plus line of sight are the whole filter.
        RE::NiCamera* cam = nullptr;
        if (a_project) {
            cam = SceneCamera();
            if (!cam) {
                return labels;
            }
        }

        const auto    mode = Config::ThreatLabelTargets();
        const bool    crosshairOnly = mode == Config::ThreatTargets::kCrosshair;
        const float   maxRange = static_cast<float>(Config::ThreatLabelRange());
        const Display display = cam ? ScreenDisplay() : Display{ 1920.0f, 1080.0f };
        const auto    playerLevel = static_cast<std::int32_t>(player->GetLevel());
        const auto    playerPos = player->GetPosition();

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
            ScreenPt screen{};
            if (cam && !Project(cam, head, display, screen)) {
                return;
            }
            // Last, because it is the most expensive question here — and it covers the
            // aimed actor too, which arrives through this same lambda.
            if (!Visible(player, actor)) {
                return;
            }
            const auto  level = static_cast<std::int32_t>(actor->GetLevel());
            const auto  verdict = Logic::VerdictFor(level - playerLevel);
            const char* name = actor->GetDisplayFullName();
            auto*       av = actor->AsActorValueOwner();
            LabelData   label;
            label.id = actor->GetFormID();
            label.pos = screen;
            label.distance = distance;
            label.verdict = verdict;
            label.tag = VerdictTag(verdict);
            label.level = level;
            label.hp = PoolOf(av, RE::ActorValue::kHealth);
            label.mp = PoolOf(av, RE::ActorValue::kMagicka);
            label.sp = PoolOf(av, RE::ActorValue::kStamina);
            label.name = name ? name : "";
            labels.push_back(std::move(label));
        };

        const auto consider = [&](RE::Actor* actor) { considerAs(actor, mode); };

        // What you are aiming at is labelled in both crosshair modes, and it bypasses the
        // sweep's filter on purpose: pointing at something IS the request to read it,
        // whether or not that thing has noticed you yet.
        //
        // Found by projection rather than by CrosshairPickData: that only resolves a
        // target within ACTIVATION range, so it never sees the wolf you are lining up
        // across a clearing — which is exactly when you want the reading.
        //
        // Skipped entirely without a camera (VR): the cone is measured in screen pixels
        // around the centre of one flat viewport, and there is no such thing in a headset.
        // kCrosshair alone would then produce nothing at all, so it falls through to the
        // sweep below instead of leaving the player with no labels.
        if (cam && (crosshairOnly || mode == Config::ThreatTargets::kAggro)) {
            // ponytail: screen-space cone, not a real ray. Picks the wrong actor only when
            // two overlap near the centre; raycast if that ever matters.
            //
            // Measured against the actor's whole BODY — the segment from its feet to the
            // head point — not against the head alone. Against the head, the only place
            // that produced a reading was the head: at conversational range a person's
            // chest is hundreds of pixels below their skull, so aiming at someone's face
            // worked and aiming at them did not. The radius below is the sideways
            // tolerance, and the ini owns it. Everything here is in pixels.
            float      best = display.h * Config::ThreatLabelAimRadius() * 0.01f;
            RE::Actor* aimed = nullptr;
            if (auto* lists = RE::ProcessLists::GetSingleton()) {
                const float cx = display.w * 0.5f;
                const float cy = display.h * 0.5f;
                for (const auto& handle : lists->highActorHandles) {
                    auto actor = handle.get();
                    if (!actor || !Eligible(actor.get(), player, Config::ThreatTargets::kAll) ||
                        playerPos.GetDistance(actor->GetPosition()) > maxRange) {
                        continue;
                    }
                    RE::NiPoint3 head{};
                    ScreenPt     top{};
                    if (!HeadPoint(actor.get(), head) || !Project(cam, head, display, top)) {
                        continue;
                    }
                    // The feet: the 3D root's own placement, the same anchor HeadPoint
                    // measures up from. Off screen (behind the camera on a steep look-down)
                    // it simply falls back to the head, which is the old behaviour.
                    ScreenPt bottom = top;
                    if (auto* root = actor->Get3D()) {
                        static_cast<void>(Project(cam, root->world.translate, display, bottom));
                    }
                    // Distance from the crosshair to the segment top..bottom, in pixels.
                    const float tx = top.x * display.w;
                    const float ty = top.y * display.h;
                    const float dx = (bottom.x - top.x) * display.w;
                    const float dy = (bottom.y - top.y) * display.h;
                    const float len2 = dx * dx + dy * dy;
                    const float t =
                        len2 > 1.0f ? std::clamp(((cx - tx) * dx + (cy - ty) * dy) / len2, 0.0f, 1.0f)
                                    : 0.0f;
                    const float d = std::hypot(cx - (tx + dx * t), cy - (ty + dy * t));
                    if (d < best) {
                        best = d;
                        aimed = actor.get();
                    }
                }
            }
            considerAs(aimed, Config::ThreatTargets::kCrosshair);
        }
        if (!crosshairOnly || !cam) {
            if (auto* lists = RE::ProcessLists::GetSingleton()) {
                for (const auto& handle : lists->highActorHandles) {
                    if (auto actor = handle.get()) {
                        consider(actor.get());
                    }
                }
            }
        }

        // Nearest first, then cut — under a crowd the far ones are the ones to lose.
        std::sort(labels.begin(), labels.end(),
                  [](const LabelData& a, const LabelData& b) { return a.distance < b.distance; });
        if (labels.size() > kMaxLabels) {
            labels.resize(kMaxLabels);
        }
        return labels;
    }
}
