#include "UI/Hud.h"

#include "Config.h"
#include "FrameTick.h"
#include "Sounds.h"
#include "UI/Magelight.h"
#include "UI/Screens.h"
#include "UI/ThreatLabels.h"
#include "UI/UiLogic.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <format>
#include <iterator>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Threading:
// - ShowToast / ShowToastBanner run on any thread and touch nothing here but Magelight::Run
//   (any thread) and the game's HUD message fallback, as Toast.cpp did.
// - PlayLevelUpEffect is main thread (the sting).
// - The label pass runs on the frame tick and the menu watcher on the UI event dispatch,
//   both the main thread. The label state is still taken under g_labelMx, so a sink that
//   were ever dispatched elsewhere could not tear it. Nothing called under that lock calls
//   back into this file (Run only queues the script).
// - The hud prelude runs on the game thread outside any wrapper lock; it only raises an
//   atomic.

namespace Isekai::UI {

    namespace {

        using Json = nlohmann::json;

        // Game text (names from plugins, translations) is not always valid UTF-8; dump()
        // would throw on it. Replace the bad bytes instead.
        [[nodiscard]] std::string Dump(const Json& a_json) {
            return a_json.dump(-1, ' ', false, Json::error_handler_t::replace);
        }

        void Post(std::string a_text, std::string a_key, bool a_banner) {
            if (a_text.empty()) {
                return;
            }
            // Nowhere to draw: the engine's own corner queue rather than silence. Worse,
            // but not nothing.
            if (!Magelight::Available()) {
                RE::SendHUDMessage::ShowHUDMessage(a_text.c_str());
                return;
            }
            try {
                Json j = Json::object();
                j["text"] = std::move(a_text);
                j["key"] = std::move(a_key);
                j["banner"] = a_banner;
                j["tone"] = std::string(Logic::ToneName(Logic::Tone::Cyan));
                const std::string js = "isekai.toast(" + Dump(j) + ")";
                // The host refused a click-through Popup view, so the hud sits under our
                // own panels: while the system view holds UI mode, the toast goes there
                // INSTEAD (spec 6.2), or a purchase toast would be hidden behind the shop.
                // One view only, so the player never sees it twice.
                if (Magelight::HudBelowPanels() && Magelight::HoldsUiMode()) {
                    Magelight::Run(Magelight::View::System, js);
                } else {
                    Magelight::Run(Magelight::View::Hud, js);
                }
            } catch (const std::exception& e) {
                logger::error("Hud: toast not sent ({})", e.what());
            }
        }

        // ---- threat labels ---------------------------------------------------------

        bool g_vr = false;

        std::mutex              g_labelMx;
        Logic::LabelPushGate    g_gate;
        Logic::LabelHeartbeat   g_heartbeat;
        std::vector<Logic::LabelFrame> g_frames;  // reused: no allocation per frame once warm

        // Labels are on the page (the last push was a non-empty set). Written under
        // g_labelMx, read without it by the fast path and the menu watcher.
        std::atomic<bool> g_live{ false };

        // The hud page (re)loaded: it holds no labels, and the gate must not answer the
        // next frame with positions for frames the page has never seen.
        std::atomic<bool> g_resync{ false };

        [[nodiscard]] bool Finite(float a_v) { return std::isfinite(a_v); }

        // A pool as the page gets it: exact when its figures are shown (health with
        // ThreatLabelNumbers on), else at bar resolution, [step, kPoolSteps]: combat regen
        // would otherwise change magicka and stamina, and so the content, every frame.
        // No pool stays [0, 0], which the page draws as no bar.
        [[nodiscard]] LabelPool Sent(const LabelPool& a_pool, bool a_exact) {
            if (a_exact) {
                return a_pool;
            }
            if (a_pool.max <= 0) {
                return {};
            }
            return { Logic::PoolStep(a_pool.now, a_pool.max), Logic::kPoolSteps };
        }

        // What the page draws for one label, and therefore what the content hash covers.
        struct Shown {
            LabelPool hp;
            LabelPool mp;
            LabelPool sp;
        };

        [[nodiscard]] Shown ShownOf(const LabelData& a_label, bool a_nums, bool a_res) {
            // ThreatLabelResources off: no pools, so the page draws no strip.
            return { Sent(a_label.hp, a_nums), a_res ? Sent(a_label.mp, false) : LabelPool{},
                     a_res ? Sent(a_label.sp, false) : LabelPool{} };
        }

        [[nodiscard]] std::uint64_t ContentOf(const LabelData& a_label, bool a_nums, bool a_res) {
            const Shown shown = ShownOf(a_label, a_nums, a_res);
            Logic::ContentHash h;
            h.AddInt(a_label.level)
                .AddText(a_label.name)
                .AddInt(static_cast<std::int64_t>(a_label.verdict))
                .AddText(a_label.tag ? a_label.tag : "")
                .AddInt(a_nums ? 1 : 0)
                .AddInt(shown.hp.now)
                .AddInt(shown.hp.max)
                .AddInt(shown.mp.now)
                .AddInt(shown.mp.max)
                .AddInt(shown.sp.now)
                .AddInt(shown.sp.max);
            return h.Value();
        }

        [[nodiscard]] Json PoolJson(const LabelPool& a_pool) { return Json::array({ a_pool.now, a_pool.max }); }

        // isekai.labels.full([...]): the whole set, in the order the frames went through
        // the gate. Built only when the set or its content changed.
        [[nodiscard]] std::string FullJs(const std::vector<LabelData>& a_labels,
                                         const std::vector<Logic::LabelFrame>& a_frames, bool a_nums,
                                         bool a_res) {
            static constexpr std::string_view kVerdicts[] = { "trivial", "manageable", "dangerous", "lethal" };
            Json arr = Json::array();
            for (std::size_t i = 0; i < a_labels.size(); ++i) {
                const LabelData&         l = a_labels[i];
                const Logic::LabelFrame& f = a_frames[i];
                Json j = Json::object();
                j["id"] = l.id;
                j["x"] = f.x;
                j["y"] = f.y;
                j["s"] = f.scale;
                j["level"] = l.level;
                j["name"] = l.name;
                j["tag"] = std::string(l.tag ? l.tag : "");
                j["verdict"] = std::string(kVerdicts[static_cast<std::size_t>(l.verdict)]);
                j["tone"] = std::string(Logic::ToneName(Logic::ToneFor(l.verdict)));
                const Shown shown = ShownOf(l, a_nums, a_res);
                j["hp"] = PoolJson(shown.hp);
                j["mp"] = PoolJson(shown.mp);
                j["sp"] = PoolJson(shown.sp);
                j["nums"] = a_nums;
                arr.push_back(std::move(j));
            }
            return "isekai.labels.full(" + Dump(arr) + ")";
        }

        // isekai.labels.pos([[id, x, y, s], ...]): the per-frame call while labels move.
        // Numbers only, formatted in place into one buffer sized up front rather than
        // through a json tree, because this one runs every frame; the buffer is moved into
        // Run, so it is the only allocation. A non-finite value never reaches the gate
        // (OnFrame drops it), so the text is always a valid JS literal.
        [[nodiscard]] std::string PosJs(const std::vector<Logic::LabelFrame>& a_frames) {
            std::string js;
            js.reserve(24 + a_frames.size() * 40);
            js.append("isekai.labels.pos([");
            auto out = std::back_inserter(js);
            for (std::size_t i = 0; i < a_frames.size(); ++i) {
                const auto& f = a_frames[i];
                std::format_to(out, "{}[{},{:.4f},{:.4f},{:.3f}]", i ? "," : "", f.id, f.x, f.y, f.scale);
            }
            js.append("])");
            return js;
        }

        // Under g_labelMx. Runs the frames (possibly none) through the gate and sends what
        // it asks for; with nothing to send while labels are up, the heartbeat.
        // a_nums / a_res: the settings the frames' content was hashed with.
        void PushLocked(const std::vector<LabelData>& a_labels, float a_w, float a_h, bool a_nums,
                        bool a_res) {
            const auto now = Logic::LabelHeartbeat::Clock::now();
            switch (g_gate.Update(g_frames, a_w, a_h)) {
                case Logic::LabelPush::Full:
                    Magelight::Run(Magelight::View::Hud, FullJs(a_labels, g_frames, a_nums, a_res));
                    g_heartbeat.Sent(now);
                    g_live.store(!g_frames.empty());
                    break;
                case Logic::LabelPush::Positions:
                    Magelight::Run(Magelight::View::Hud, PosJs(g_frames));
                    g_heartbeat.Sent(now);
                    break;
                case Logic::LabelPush::None:
                    if (g_live.load() && g_heartbeat.KeepDue(now)) {
                        Magelight::Run(Magelight::View::Hud, "isekai.labels.keep()");
                        g_heartbeat.Sent(now);
                    }
                    break;
            }
        }

        // Under g_labelMx: a page that reloaded holds no labels.
        void ResyncLocked() {
            if (g_resync.exchange(false)) {
                g_gate.Reset();
                g_heartbeat.Reset();
                g_live.store(false);
            }
        }

        // Empty set through the gate: one labels.full([]) if the page had labels, then
        // nothing. Keeps the gate's idea of the page true.
        void ClearLabels() {
            std::scoped_lock lock(g_labelMx);
            ResyncLocked();
            g_frames.clear();
            PushLocked({}, 1.0f, 1.0f, false, false);
        }

        // The frame tick: main thread, only while the player's update runs.
        void OnFrame() {
            const bool on = ThreatLabelsVisible();
            // Off and nothing on the page: no work at all.
            if (!on && !g_live.load(std::memory_order_relaxed) && !g_resync.load(std::memory_order_relaxed)) {
                return;
            }
            if (!Magelight::Available()) {
                return;
            }
            // Until the hud page is ready every call queues, and a label push each frame
            // would evict the queued toasts (ReadyGate keeps the newest 64). The ready
            // raises g_resync, so the first frame after it sends the full set.
            if (!Magelight::PageReady(Magelight::View::Hud)) {
                return;
            }
            if (!on) {
                ClearLabels();
                return;
            }

            std::vector<LabelData> labels = SelectLabels(/*a_project=*/!g_vr);

            float w = 1.0f;
            float h = 1.0f;
            const float range = static_cast<float>(Config::ThreatLabelRange());
            const bool  nums = Config::ThreatLabelNumbers();
            const bool  res = Config::ThreatLabelResources();
            if (!g_vr) {
                LabelScreenSize(w, h);
                // Flat screens place labels by id, not by rank: two actors trading places
                // in distance must not turn into a full push. The 12 nearest were already
                // chosen.
                std::sort(labels.begin(), labels.end(),
                          [](const LabelData& a, const LabelData& b) { return a.id < b.id; });
            }

            std::scoped_lock lock(g_labelMx);
            ResyncLocked();
            g_frames.clear();
            std::size_t kept = 0;
            for (std::size_t i = 0; i < labels.size(); ++i) {
                const LabelData& l = labels[i];
                // VR: no positions, the scan list is ordered nearest first and only a set or
                // content change is worth a push.
                const float x = g_vr ? 0.0f : l.pos.x;
                const float y = g_vr ? 0.0f : l.pos.y;
                const float s = g_vr ? 1.0f : Logic::LabelDistanceScale(l.distance, range);
                if (!Finite(x) || !Finite(y) || !Finite(s)) {
                    continue;
                }
                if (kept != i) {
                    labels[kept] = std::move(labels[i]);
                }
                g_frames.push_back({ labels[kept].id, x, y, s, ContentOf(labels[kept], nums, res) });
                ++kept;
            }
            labels.resize(kept);
            PushLocked(labels, w, h, nums, res);
        }

        // ---- the menu watcher ------------------------------------------------------

        // A menu that pauses the game, or the console, opens: the frame tick stops, and
        // labels left on the page would hang over the menu until the page's 300 ms
        // staleness hides them. Clear them at once instead.
        class MenuWatcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            static MenuWatcher* GetSingleton() {
                static MenuWatcher instance;
                return &instance;
            }

            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
                                                  RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (!a_event || !a_event->opening || !g_live.load()) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                try {
                    const std::string_view name = a_event->menuName.c_str();
                    bool pauses = name == RE::Console::MENU_NAME;
                    if (!pauses) {
                        if (auto* ui = RE::UI::GetSingleton()) {
                            const auto menu = ui->GetMenu(name);
                            pauses = menu && menu->PausesGame();
                        }
                    }
                    if (pauses) {
                        ClearLabels();
                    }
                } catch (const std::exception& e) {
                    logger::error("Hud: menu watcher ({})", e.what());
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        // Every fresh hud page gets the settings first, then, in VR, the scan-list switch.
        std::vector<std::string> HudPrelude() {
            auto calls = Screens::SettingsCalls();
            if (g_vr) {
                calls.push_back("isekai.vr(" + Json(true).dump() + ")");
            }
            g_resync.store(true);
            return calls;
        }
    }

    void ShowToast(std::string a_text, std::string a_key) {
        Post(std::move(a_text), std::move(a_key), /*a_banner=*/false);
    }

    void ShowToastBanner(std::string a_text, std::string a_key) {
        Post(std::move(a_text), std::move(a_key), /*a_banner=*/true);
    }

    void PlayLevelUpEffect(std::string a_title, std::string a_subtitle) {
        // The sting belongs to the flourish, not to whoever triggers it — keeping it
        // here means a new call site cannot forget it, and cannot get the timing
        // wrong. Delayed: the flourish almost always follows a panel choice, i.e. the
        // frame in which the System view lets go and the game leaves menu-pause, and a
        // sting started in that frame is discarded by the engine's resume pass.
        Sounds::PlayDelayed(Sounds::Sfx::LevelUp, Sounds::kResumeGraceMs);
        // No fallback without Magelight: every path here (the blessing, a node bought in
        // the tree) needs the system view, which is not there either (spec 6.5).
        if (!Magelight::Available()) {
            return;
        }
        try {
            Json j = Json::object();
            j["title"] = std::move(a_title);
            j["subtitle"] = std::move(a_subtitle);
            Magelight::Run(Magelight::View::Hud, "isekai.flourish(" + Dump(j) + ")");
        } catch (const std::exception& e) {
            logger::error("Hud: flourish not sent ({})", e.what());
        }
    }

    namespace Hud {

        void Install() {
            g_vr = REL::Module::IsVR();
            Magelight::SetPrelude(Magelight::View::Hud, &HudPrelude);
            if (!Magelight::Available()) {
                logger::info("Hud: no Magelight UI, toasts go to the game's corner notification");
                return;
            }
            FrameTick::Add(&OnFrame);
            if (auto* ui = RE::UI::GetSingleton()) {
                ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatcher::GetSingleton());
            }
            logger::info("Hud: threat labels on the frame tick ({})",
                         g_vr ? "VR scan list" : "projected over heads");
        }

    }  // namespace Hud

}  // namespace Isekai::UI
