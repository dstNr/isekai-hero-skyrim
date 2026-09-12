#include "UI/ThreatLabels.h"

#include "Config.h"
#include "Loc.h"
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
                return { L("threat.lethal", "LETHAL"), ImVec4{ 1.00f, 0.28f, 0.28f, 1.0f } };
            }
            if (a_diff >= 5) {
                return { L("threat.dangerous", "DANGEROUS"), ImVec4{ 1.00f, 0.68f, 0.25f, 1.0f } };
            }
            if (a_diff <= -15) {
                return { L("threat.trivial", "TRIVIAL"), ImVec4{ 0.80f, 0.86f, 0.92f, 1.0f } };
            }
            return { L("threat.manageable", "MANAGEABLE"), ImVec4{ 0.55f, 0.85f, 1.00f, 1.0f } };
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

        // One resource bar's worth of numbers.
        struct Pool {
            float        fraction;  // 0..1, for the bar
            std::int32_t now;
            std::int32_t max;      // 0 = this actor has none, so draw no bar
        };

        // A label the frame can draw: already judged, and carrying both anchors — the
        // screen point the flat overlay draws at, and the world point VR needs, because
        // there the helper does the projection and wants game units.
        struct Label {
            ImVec2       pos;    // screen pixels; unset (and unused) on the VR path
            RE::NiPoint3 world;  // the head point itself, for a world-anchored billboard
            float        distance;
            Verdict      verdict;
            std::int32_t level;
            Pool         hp;
            Pool         mp;
            Pool         sp;
            std::string  name;
        };

        // Current health as a fraction. The frame draws a bar, and a bar without data
        // behind it is decoration pretending to be information.
        [[nodiscard]] Pool PoolOf(RE::ActorValueOwner* a_av, RE::ActorValue a_stat) {
            if (!a_av) {
                return { 1.0f, 0, 0 };
            }
            const float now = a_av->GetActorValue(a_stat);
            const float max = a_av->GetPermanentActorValue(a_stat);
            return { max > 0.0f ? std::clamp(now / max, 0.0f, 1.0f) : 1.0f,
                     static_cast<std::int32_t>(std::lround(std::max(now, 0.0f))),
                     static_cast<std::int32_t>(std::lround(std::max(max, 0.0f))) };
        }

        // Magicka and stamina, in the colours every MMO uses for them — blue and green.
        // Saturated, because they share one five-pixel strip: a muted fill at that height
        // is a smudge. They still lose to the verdict colour, which owns the badge, the
        // plate's edges and the whole health bar — far more area than these two get.
        constexpr ImVec4 kMagickaCol{ 0.30f, 0.60f, 1.00f, 1.0f };
        constexpr ImVec4 kStaminaCol{ 0.36f, 0.90f, 0.42f, 1.0f };

        // A cap, because "all visible enemies" during a dragon attack on a city is not a
        // number anyone chose. The nearest ones are the ones that matter.
        constexpr std::size_t kMaxLabels = 12;
    }

    void InstallThreatLabels() {
        // The key is armed in VR too now. It used to be skipped because VR had no overlay
        // to draw on at all; the labels exist there as world billboards, so the toggle has
        // something to toggle. A VR player without a keyboard simply never presses it —
        // that is a reason to also offer a controller binding, not to withhold the key.
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
        ShowToast(on ? L("threat.toggleOn", "Threat display  ON")
                     : L("threat.toggleOff", "Threat display  OFF"),
                  "threat");
    }

    namespace {
        // Everything up to "which actors get a label, and what does each one say".
        //
        // Split out because VR needs the same answer through a different lens: there the
        // helper projects the billboards itself, so a_cam is null and the screen-space
        // steps — the projection filter and the crosshair cone that depends on it — are
        // skipped rather than fed a single eye's matrix, which would be neither eye.
        [[nodiscard]] std::vector<Label> Collect(RE::PlayerCharacter* a_player,
                                                 RE::NiCamera* a_cam, const ImVec2& a_display) {
        const auto   mode = Config::ThreatLabelTargets();
        const bool   crosshairOnly = mode == Config::ThreatTargets::kCrosshair;
        const float  maxRange = static_cast<float>(Config::ThreatLabelRange());
        const ImVec2 display = a_display;
        const auto   playerLevel = static_cast<std::int32_t>(a_player->GetLevel());
        const auto   playerPos = a_player->GetPosition();
        auto*        player = a_player;
        auto*        cam = a_cam;

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
            // No camera means VR: the helper owns the projection, so "is it on screen"
            // is not ours to answer and range plus line of sight are the whole filter.
            if (cam && !Project(cam, head, display, screen)) {
                return;
            }
            // Last, because it is the most expensive question here — and it covers the
            // aimed actor too, which arrives through this same lambda.
            if (!Visible(player, actor)) {
                return;
            }
            const auto  level = static_cast<std::int32_t>(actor->GetLevel());
            const char* name = actor->GetDisplayFullName();
            auto*       av = actor->AsActorValueOwner();
            labels.push_back({ screen, head, distance, VerdictFor(level - playerLevel), level,
                               PoolOf(av, RE::ActorValue::kHealth),
                               PoolOf(av, RE::ActorValue::kMagicka),
                               PoolOf(av, RE::ActorValue::kStamina), name ? name : "" });
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
            // worked and aiming at them did not. The radius below is now the sideways
            // tolerance it was always meant to be, and the ini owns it.
            float      best = display.y * Config::ThreatLabelAimRadius() * 0.01f;
            RE::Actor* aimed = nullptr;
            if (auto* lists = RE::ProcessLists::GetSingleton()) {
                const ImVec2 centre{ display.x * 0.5f, display.y * 0.5f };
                for (const auto& handle : lists->highActorHandles) {
                    auto actor = handle.get();
                    if (!actor || !Eligible(actor.get(), player, Config::ThreatTargets::kAll) ||
                        playerPos.GetDistance(actor->GetPosition()) > maxRange) {
                        continue;
                    }
                    RE::NiPoint3 head{};
                    ImVec2       top{};
                    if (!HeadPoint(actor.get(), head) || !Project(cam, head, display, top)) {
                        continue;
                    }
                    // The feet: the 3D root's own placement, the same anchor HeadPoint
                    // measures up from. Off screen (behind the camera on a steep look-down)
                    // it simply falls back to the head, which is the old behaviour.
                    ImVec2 bottom = top;
                    if (auto* root = actor->Get3D()) {
                        static_cast<void>(Project(cam, root->world.translate, display, bottom));
                    }
                    // Distance from the crosshair to the segment top..bottom.
                    const float dx = bottom.x - top.x;
                    const float dy = bottom.y - top.y;
                    const float len2 = dx * dx + dy * dy;
                    const float t =
                        len2 > 1.0f ? std::clamp(((centre.x - top.x) * dx + (centre.y - top.y) * dy) /
                                                     len2,
                                                 0.0f, 1.0f)
                                    : 0.0f;
                    const float d = std::hypot(centre.x - (top.x + dx * t),
                                               centre.y - (top.y + dy * t));
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
                  [](const Label& a, const Label& b) { return a.distance < b.distance; });
        if (labels.size() > kMaxLabels) {
            labels.resize(kMaxLabels);
        }
        return labels;
        }

        // One target frame, drawn with its BOTTOM CENTRE at a_anchor. Returns the size it
        // occupied, which the VR path needs: the billboard's sub-rect has to be the frame
        // rather than the cell it was drawn in, or every label would be stretched to the
        // cell's aspect ratio.
        //
        // a_k scales the whole frame and a_alpha fades it. The flat overlay derives both
        // from distance so a crowd reads as depth; VR passes them fixed, because there the
        // billboard itself shrinks with distance and doing it twice would make anything
        // more than a few metres away unreadable.
        ImVec2 DrawFrame(ImDrawList* dl, const Label& label, const ImVec2& a_anchor, float k,
                         float alpha) {
            ImFont* font = Style::g_body;

            // One line of text over one underline, and the underline IS the health bar.
            //
            // The plate this replaced drew the verdict colour three times over: the level
            // badge, the plate's two edges and the whole bar, with magicka blue and stamina
            // green competing underneath. Four saturated things at once is what the 0.8.0
            // feedback meant by "too many high-contrast elements competing", and no amount
            // of restyling a plate fixes a frame that says the same thing four ways.
            //
            // So: exactly ONE coloured element. The bar carries the verdict colour, which
            // keeps the meaning that colour always had - red is lethal, not low health -
            // and everything else is dim. The text is outlined rather than backed by a
            // panel, which is what lets the panel go away without the name disappearing
            // over snow or firelight.
            const float nameSize = 18.0f * k;
            const float smallSize = 12.0f * k;
            const float barH = 4.0f * k;
            const float gap = 3.0f * k;
            const float resH = 2.5f * k;
            const float resGap = 2.0f * k;
            const float pad = 10.0f * k;

            const auto measure = [&](float a_size, const char* a_text) {
                return font ? font->CalcTextSizeA(a_size, FLT_MAX, 0.0f, a_text).x
                            : ImGui::CalcTextSize(a_text).x;
            };

            const std::string lvl = "LV " + std::to_string(label.level);
            const char*       name =
                label.name.empty() ? L("threat.unknown", "Unknown") : label.name.c_str();

            const bool  wantRes = Config::ThreatLabelResources();
            const bool  showMp = wantRes && label.mp.max > 0;
            const bool  showSp = wantRes && label.sp.max > 0;
            const float extra = (showMp || showSp) ? resH + resGap : 0.0f;

            // The health figure joins the text row rather than sitting on the bar: at four
            // pixels there is no room on it, and a second row is exactly the height this
            // frame was asked to stop taking.
            std::string hpText;
            if (Config::ThreatLabelNumbers() && label.hp.max > 0) {
                hpText = std::to_string(label.hp.now) + " / " + std::to_string(label.hp.max);
            }

            const float lvlW = measure(smallSize, lvl.c_str());
            const float nameW = measure(nameSize, name);
            const float tagW = measure(smallSize, label.verdict.tag);
            const float hpGap = hpText.empty() ? 0.0f : 8.0f * k;
            const float hpW = hpText.empty() ? 0.0f : measure(smallSize, hpText.c_str());

            const float totalW =
                std::max(140.0f * k, lvlW + pad + nameW + hpGap + hpW + pad + tagW);

            const float cx = a_anchor.x;
            const float x0 = cx - totalW * 0.5f;
            const float x1 = cx + totalW * 0.5f;

            // Pinned to the head point and growing upward, so a taller frame never drops
            // over the actor's face.
            const float y1 = a_anchor.y;
            const float barY = y1 - extra - barH;
            const float textY = barY - gap - nameSize;
            const float y0 = textY;

            // Small type sits on the name's baseline rather than its top, or the row reads
            // as three things at three different heights.
            const float smallY = textY + (nameSize - smallSize) * 0.70f;

            Style::DrawTextOutlined(dl, font, smallSize, ImVec2{ x0, smallY }, Style::kTextDim,
                                    lvl.c_str(), alpha);
            Style::DrawTextOutlined(dl, font, nameSize, ImVec2{ x0 + lvlW + pad, textY },
                                    Style::kText, name, alpha);
            if (!hpText.empty()) {
                Style::DrawTextOutlined(dl, font, smallSize,
                                        ImVec2{ x0 + lvlW + pad + nameW + hpGap, smallY },
                                        Style::kTextDim, hpText.c_str(), alpha);
            }
            // The verdict, right-aligned: the one piece of text allowed to be coloured, and
            // it names in a word what the bar underneath already said in a colour.
            Style::DrawTextOutlined(dl, font, smallSize, ImVec2{ x1 - tagW, smallY },
                                    label.verdict.col, label.verdict.tag, alpha);

            // The underline. Empty track first, so a nearly dead target still shows a line
            // instead of disappearing at the moment it matters most.
            dl->AddRectFilled(ImVec2{ x0, barY }, ImVec2{ x1, barY + barH },
                              Style::Col(Style::kPanelBg, 0.85f * alpha));
            dl->AddRectFilled(ImVec2{ x0, barY },
                              ImVec2{ x0 + totalW * label.hp.fraction, barY + barH },
                              Style::Col(label.verdict.col, 0.90f * alpha));

            // Magicka and stamina, each filling from its own edge, as one hairline beneath
            // the health line. Colour only: at two pixels a figure is a smudge, and health
            // is the number that decides the fight anyway. Dimmer than the health line on
            // purpose - they are context, not the verdict.
            if (showMp || showSp) {
                const float resY = y1 - resH;
                const auto  strip = [&](float a_x0, float a_x1, const Pool& pool,
                                       const ImVec4& col) {
                    if (a_x1 - a_x0 < 2.0f) {
                        return;
                    }
                    dl->AddRectFilled(ImVec2{ a_x0, resY }, ImVec2{ a_x1, resY + resH },
                                      Style::Col(Style::kPanelBg, 0.85f * alpha));
                    dl->AddRectFilled(
                        ImVec2{ a_x0, resY },
                        ImVec2{ a_x0 + (a_x1 - a_x0) * pool.fraction, resY + resH },
                        Style::Col(col, 0.65f * alpha));
                };
                if (showMp && showSp) {
                    // A gap between them, not a shared border: touching, the two fills read
                    // as one bar that changes colour partway along.
                    const float split = 4.0f * k;
                    strip(x0, cx - split * 0.5f, label.mp, kMagickaCol);
                    strip(cx + split * 0.5f, x1, label.sp, kStaminaCol);
                } else if (showMp) {
                    strip(x0, x1, label.mp, kMagickaCol);
                } else {
                    strip(x0, x1, label.sp, kStaminaCol);
                }
            }

            return ImVec2{ totalW, y1 - y0 };
        }
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

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const auto   labels = Collect(player, cam, display);
        if (labels.empty()) {
            return;
        }

        const float maxRange = static_cast<float>(Config::ThreatLabelRange());
        const float s = Style::g_scale;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        for (const auto& label : labels) {
            // Distant frames shrink and dim, so the near ones stay dominant and a crowd
            // reads as depth rather than as noise.
            const float t = std::clamp(label.distance / maxRange, 0.0f, 1.0f);
            static_cast<void>(DrawFrame(dl, label, label.pos, (1.0f - 0.32f * t) * s,
                                        1.0f - 0.35f * t));
        }
    }

    void DrawThreatLabelsVR(const ImVec2&                                   a_panel,
                            std::vector<ImGuiVRHelperPluginAPI::WorldQuad>& a_out) {
        // Cleared unconditionally, and the caller submits the result even when it is
        // empty: SubmitWorldQuads replaces the previous list wholesale, and a frame that
        // simply does not call it keeps the last one on screen. "No labels" has to be
        // said out loud, or switching the feature off would leave the last set floating.
        a_out.clear();

        if (!ThreatLabelsVisible() || a_panel.x <= 0.0f || a_panel.y <= 0.0f) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) {
            return;
        }
        if (auto* ui = RE::UI::GetSingleton(); !ui || ui->GameIsPaused()) {
            return;
        }

        // No camera: in VR the helper projects the billboards itself.
        const auto labels = Collect(player, nullptr, a_panel);
        if (labels.empty()) {
            return;
        }

        const float bandH = a_panel.y / static_cast<float>(kMaxLabels);
        const float height = Config::VRThreatLabelHeight();

        // UiScale has to reach this surface too — it was the one place it did not (#26).
        // Not Style::g_scale: in VR that already carries the helper's own panel scale
        // (VROverlay.cpp sets it), so using it here would apply that factor twice.
        //
        // Clamped so a frame can never grow out of its band into the one above. The frame
        // grows upward from its baseline by badgeRY * 2 + extra, which is 17 * 2 * k plus
        // an 8 * k resource row when one is shown — 42 * k at its tallest. The 4 px is the
        // same clearance the baseline below already leaves.
        constexpr float kFrameHeightAt1x = 42.0f;
        const float     scale =
            std::clamp(Style::g_userScale, 0.5f, std::max(0.5f, (bandH - 4.0f) / kFrameHeightAt1x));
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        for (std::size_t i = 0; i < labels.size(); ++i) {
            // Bottom-centre of this label's band, a couple of pixels clear of the edge so
            // the frame's lower border is not clipped by the sub-rect.
            const float  baseline = static_cast<float>(i + 1) * bandH - 4.0f;
            const ImVec2 anchor{ a_panel.x * 0.5f, baseline };

            // The player's own scale, but no distance falloff and full opacity, unlike
            // the flat path: the billboard already shrinks with distance because it is a
            // fixed size in the world, and fading it as well would make anything past a
            // few metres unreadable.
            const ImVec2 size = DrawFrame(dl, labels[i], anchor, scale, 1.0f);
            if (size.x <= 0.0f || size.y <= 0.0f) {
                continue;
            }

            ImGuiVRHelperPluginAPI::WorldQuad quad{};
            quad.u0 = std::clamp((anchor.x - size.x * 0.5f) / a_panel.x, 0.0f, 1.0f);
            quad.u1 = std::clamp((anchor.x + size.x * 0.5f) / a_panel.x, 0.0f, 1.0f);
            quad.v0 = std::clamp((baseline - size.y) / a_panel.y, 0.0f, 1.0f);
            quad.v1 = std::clamp(baseline / a_panel.y, 0.0f, 1.0f);
            // Skyrim world units, NOT tracking space: the helper converts at submit time
            // from the same pose it builds the eye projection with, and converting here
            // instead is what makes a billboard jitter while the player is moving.
            quad.pos[0] = labels[i].world.x;
            quad.pos[1] = labels[i].world.y;
            quad.pos[2] = labels[i].world.z;
            quad.height_m = height;
            a_out.push_back(quad);
        }
    }
}
