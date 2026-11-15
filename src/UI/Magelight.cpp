#include "UI/Magelight.h"

#include "UI/Screens.h"
#include "UI/UiLogic.h"

#include <atomic>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// After the PCH (CommonLib first): the Magelight header includes <windows.h>.
#include "MagelightUI_API.h"

// Modelled on GrownUp's gu::magelight (one view there, two here). The convenience wrapper
// MagelightUI_Mod.h owns ModDesc.onEvent and forwards only a few events; this file needs
// UIModeEntered, RenderDead, ViewReloaded and the page console too, so it registers the
// mod through the raw v4 table.
namespace Isekai::UI::Magelight {

    namespace {
        namespace ml = MAGELIGHT_API;

        constexpr const char* kModId = "isekai-hero";
        constexpr const char* kDisplayName = "Isekai Hero";
        constexpr std::uint32_t kMinHost = ml::PackVersion(1, 7, 0);
        // Page files live here, relative to the game folder. Logs name pages by this
        // relative form only: the absolute one can sit under the user's folder.
        constexpr const char* kPageDir = "Data\\Magelight\\IsekaiHero\\";
        constexpr std::size_t kMaxMessage = 16 * 1024;  // page messages above this are dropped
        constexpr std::size_t kMaxLogText = 1000;

        struct Slot {
            const char* name;
            const char* file;
            std::atomic<ml::ViewId> id{ 0 };  // set in Install before g_accept/g_ready
            std::mutex mx;
            Logic::ReadyGate gate;  // mx
            std::string version;    // mx: what the page sent with its last isekai.ready
            std::function<std::vector<std::string>()> prelude;  // mx: copied out, run outside the lock
            std::size_t queued = 0;   // mx: calls queued in this not-ready period
            bool fullWarned = false;  // mx: the queue-full warning was logged in this period
        };

        // Caller holds a_slot.mx. Back to not-ready: the page has to report ready again.
        void ResetGate(Slot& a_slot) {
            a_slot.gate.Reset();
            a_slot.version.clear();
            a_slot.queued = 0;
            a_slot.fullWarned = false;
        }

        // Sent once, ungated, after a reload: a page that already reported ready before the
        // reload event was handled reports again (page contract in Magelight.h).
        constexpr const char* kHello = "window.isekai && isekai.hello && isekai.hello()";

        Slot g_slots[2]{ { "system", "system.html" }, { "hud", "hud.html" } };

        Slot& SlotOf(View a_view) { return g_slots[a_view == View::Hud ? 1 : 0]; }

        // Written once in Install (main thread, kDataLoaded) before g_accept / g_ready are
        // set; read-only afterwards.
        const ml::MagelightApi4* g_api = nullptr;
        ml::ModId g_mod = 0;
        std::string g_hostVersion;
        bool g_hudBelow = false;
        std::vector<std::string> g_userDirs;  // spellings of %USERPROFILE% scrubbed from host text

        std::atomic<bool> g_accept{ false };  // listeners may post (views exist)
        std::atomic<bool> g_ready{ false };   // Install succeeded
        std::atomic<bool> g_dead{ false };    // RenderDead: the overlay is off for the session

        const char* ResultName(ml::Result a_r) {
            switch (a_r) {
                case ml::Result::Ok: return "Ok";
                case ml::Result::HostAbsent: return "HostAbsent";
                case ml::Result::HostTooOld: return "HostTooOld";
                case ml::Result::NotReady: return "NotReady";
                case ml::Result::RenderDead: return "RenderDead";
                case ml::Result::InvalidView: return "InvalidView";
                case ml::Result::InvalidMod: return "InvalidMod";
                case ml::Result::FileNotFound: return "FileNotFound";
                case ml::Result::Busy: return "Busy";
                case ml::Result::Denied: return "Denied";
                case ml::Result::Unsupported: return "Unsupported";
                case ml::Result::InvalidArgument: return "InvalidArgument";
                case ml::Result::Internal: return "Internal";
            }
            return "?";
        }

        std::string Utf8(const std::filesystem::path& a_path) {
            const auto u = a_path.u8string();
            return std::string(reinterpret_cast<const char*>(u.data()), u.size());
        }

        // The user's folder in the forms host text carries it: a Windows path, a file URL
        // path, and a file URL with its spaces escaped. Install fills this before the first
        // host call, so the log never names the user (repo rule).
        void InitUserDirs() {
            g_userDirs.clear();
            wchar_t buf[MAX_PATH]{};
            const auto n = GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH);
            if (n == 0 || n >= MAX_PATH) return;
            const std::string dir = Utf8(std::filesystem::path(buf));
            if (dir.size() < 4) return;
            std::string slash = dir;
            for (auto& c : slash) {
                if (c == '\\') c = '/';
            }
            std::string url;
            for (const char c : slash) {
                if (c == ' ')
                    url += "%20";
                else
                    url += c;
            }
            g_userDirs = { dir, slash };
            if (url != slash) g_userDirs.push_back(url);
        }

        char Lower(char a_c) { return (a_c >= 'A' && a_c <= 'Z') ? static_cast<char>(a_c - 'A' + 'a') : a_c; }

        // Case-insensitive (ASCII) replace of every user-folder spelling with %USERPROFILE%.
        std::string Scrub(std::string a_text) {
            for (const auto& dir : g_userDirs) {
                std::size_t at = 0;
                while (at + dir.size() <= a_text.size()) {
                    bool hit = true;
                    for (std::size_t k = 0; k < dir.size(); ++k) {
                        if (Lower(a_text[at + k]) != Lower(dir[k])) {
                            hit = false;
                            break;
                        }
                    }
                    if (!hit) {
                        ++at;
                        continue;
                    }
                    a_text.replace(at, dir.size(), "%USERPROFILE%");
                    at += 13;
                }
            }
            return a_text;
        }

        // Host text for the log: user folder scrubbed, clipped.
        std::string Clip(const char* a_text, std::size_t a_max = kMaxLogText) {
            std::string t = Scrub(a_text ? a_text : "");
            if (t.size() > a_max) {
                t.resize(a_max);
                t += "...";
            }
            return t;
        }

        // Read on the thread of the failing call, right after it (MagelightUI_API.h,
        // GetLastErrorMessage).
        std::string LastError() {
            if (!g_api || !g_mod) return {};
            return Clip(g_api->GetLastErrorMessage(g_mod));
        }

        // Absolute page path from the running exe (the host loads a file:/// URL).
        std::filesystem::path PagePath(const char* a_file) {
            wchar_t buf[MAX_PATH]{};
            const auto n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
            if (n == 0 || n >= MAX_PATH) return {};
            return std::filesystem::path(buf).parent_path() / L"Data" / L"Magelight" / L"IsekaiHero" / a_file;
        }

        // ---- page messages: page thread -> PostGameTask -> game thread ----

        enum class Kind : std::uint8_t { Action, Ready, Entered, Exited };

        struct Job {
            Kind kind;
            View view;
            std::string text;
        };

        // Game thread, while the gate's mutex is NOT held by this thread.
        void MarkPageReady(View a_view, std::string a_version) {
            Slot& s = SlotOf(a_view);
            // The prelude is built outside the lock: it may read game state or even call Run,
            // which takes the same lock.
            std::function<std::vector<std::string>()> build;
            {
                std::scoped_lock lock(s.mx);
                build = s.prelude;
                // A repeated ready (isekai.hello after a reload, or a page that says it twice):
                // back to not-ready, so Runs from other threads queue while the prelude is built
                // and go out after it. A ready gate's queue is empty, so nothing is lost; a gate
                // that is not ready yet keeps its queue untouched.
                if (s.gate.Ready()) ResetGate(s);
            }
            std::vector<std::string> prelude;
            if (build) {
                try {
                    prelude = build();
                } catch (const std::exception& e) {
                    logger::error("Magelight: {} prelude: {}", s.name, Clip(e.what()));
                } catch (...) {
                    logger::error("Magelight: {} prelude: unknown exception", s.name);
                }
            }
            std::size_t sent = 0;
            {
                // Under the lock, so a Run from another thread cannot slip in between: the
                // page sees the prelude first, then the queued calls in the order they were
                // made, then everything later.
                std::scoped_lock lock(s.mx);
                s.version = a_version;
                const auto id = s.id.load();
                for (const auto& js : prelude) g_api->InvokeJS(id, js.c_str());
                const auto calls = s.gate.MarkReady();
                s.queued = 0;
                s.fullWarned = false;
                for (const auto& js : calls) g_api->InvokeJS(id, js.c_str());
                sent = calls.size();
            }
            logger::info("Magelight: {} page ready, version '{}', {} prelude and {} queued call(s) sent", s.name,
                         Clip(a_version.c_str(), 64), prelude.size(), sent);
            Screens::detail::OnPageReady(a_view, std::move(a_version));
        }

        // Game thread (PostGameTask runs it as an SKSE task): RE:: is safe here.
        void RunJob(void* a_user) {
            std::unique_ptr<Job> job(static_cast<Job*>(a_user));
            if (!job || !g_ready.load(std::memory_order_acquire)) return;
            try {
                switch (job->kind) {
                    case Kind::Ready:
                        MarkPageReady(job->view, std::move(job->text));
                        break;
                    case Kind::Action:
                        Screens::detail::OnAction(job->text);
                        break;
                    case Kind::Entered:
                        Screens::detail::OnEntered();
                        break;
                    case Kind::Exited: {
                        // ReleaseUIMode and Escape leave the view on screen; hide it unless a
                        // new entry already took UI mode again (a quick reopen, whose Entered
                        // job follows this one).
                        // Hide first: OnExited may send a held request at once (our own exit
                        // arrived), and that screen must not end up in UI mode while hidden.
                        const bool reopened = g_api->GetUIModeOwner() == g_mod;
                        if (!reopened) g_api->ShowView(SlotOf(View::System).id.load(), false);
                        Screens::detail::OnExited(reopened);
                        break;
                    }
                }
            } catch (const std::exception& e) {
                logger::error("Magelight: page message: {}", e.what());
            } catch (...) {
                logger::error("Magelight: page message: unknown exception");
            }
        }

        // Magelight's page thread (or the render thread inside Present): copy the argument,
        // post it, nothing else. No RE::, no SKSE AddTask here (threading contract in
        // MagelightUI_API.h).
        void Post(Kind a_kind, View a_view, const char* a_arg) {
            try {
                if (!g_accept.load(std::memory_order_acquire)) return;
                std::string text = a_arg ? a_arg : "";
                if (text.size() > kMaxMessage) {
                    logger::warn("Magelight: {} page message of {} bytes dropped", SlotOf(a_view).name,
                                 text.size());
                    return;
                }
                auto* job = new Job{ a_kind, a_view, std::move(text) };
                const auto r = g_api->PostGameTask(&RunJob, job);
                if (r != ml::Result::Ok) {
                    delete job;
                    logger::warn("Magelight: PostGameTask refused ({}): {}", ResultName(r), LastError());
                }
            } catch (const std::exception& e) {
                logger::error("Magelight: accepting a page message: {}", e.what());
            } catch (...) {
                logger::error("Magelight: accepting a page message: unknown exception");
            }
        }

        void OnSystemAction(ml::ViewId, const char* a_arg, void*) { Post(Kind::Action, View::System, a_arg); }
        void OnSystemReady(ml::ViewId, const char* a_arg, void*) { Post(Kind::Ready, View::System, a_arg); }
        void OnHudReady(ml::ViewId, const char* a_arg, void*) { Post(Kind::Ready, View::Hud, a_arg); }

        // ---- host events: CallbackThread::GameThread, delivered as SKSE tasks ----

        void OnEvent(const ml::EventData* a_ev, void*) {
            if (!a_ev) return;
            try {
                const auto systemId = SlotOf(View::System).id.load();
                const auto hudId = SlotOf(View::Hud).id.load();
                if (a_ev->view != 0 && a_ev->view != systemId && a_ev->view != hudId) return;  // not ours
                Slot* slot = a_ev->view == 0 ? nullptr : &SlotOf(a_ev->view == hudId ? View::Hud : View::System);
                const char* name = slot ? slot->name : "?";
                // Only the system view can hold UI mode (the hud is click-through).
                const bool uiModeView = a_ev->view == 0 || a_ev->view == systemId;

                switch (a_ev->type) {
                    // Entered and Exited go through PostGameTask, like the page messages, so
                    // all of them keep one order.
                    case ml::Event::UIModeEntered:
                        if (uiModeView) Post(Kind::Entered, View::System, nullptr);
                        break;
                    case ml::Event::UIModeExited:
                        if (uiModeView) Post(Kind::Exited, View::System, nullptr);
                        break;
                    case ml::Event::UIModeRefused:
                        if (uiModeView) Screens::detail::OnRefused(Clip(a_ev->detail));
                        break;
                    case ml::Event::ViewLoadFailed:
                        logger::error("Magelight: {} page failed to load: {}", name, Clip(a_ev->detail));
                        break;
                    case ml::Event::RenderDead:
                        g_dead.store(true);
                        logger::error("Magelight: the overlay is off for this session, no Isekai Hero UI");
                        break;
                    case ml::Event::ViewDomReady:
                        logger::info("Magelight: {} page loaded", name);
                        break;
                    case ml::Event::ViewReloaded:
                        // The page was replaced: its state is gone, so calls wait for its
                        // next isekai.ready again. That ready may already have been handled
                        // (page messages and page events keep no common order), so ask the
                        // page once, ungated, to send it again; a second ready is harmless.
                        if (slot) {
                            std::scoped_lock lock(slot->mx);
                            ResetGate(*slot);
                            g_api->InvokeJS(slot->id.load(), kHello);
                        }
                        logger::info("Magelight: {} page reloaded, asked it to report ready again", name);
                        break;
                    case ml::Event::FocusDenied:
                        logger::info("Magelight: {} asked for UI mode while Isekai Hero holds it", Clip(a_ev->detail));
                        break;
                    case ml::Event::ConsoleMessage:
                        if (a_ev->x >= 1)
                            logger::warn("Magelight: {} page (line {}): {}", name, a_ev->y, Clip(a_ev->detail));
                        break;
                    default:
                        break;
                }
            } catch (const std::exception& e) {
                logger::error("Magelight: event {}: {}", static_cast<std::uint32_t>(a_ev->type), e.what());
            } catch (...) {
                logger::error("Magelight: event {}: unknown exception", static_cast<std::uint32_t>(a_ev->type));
            }
        }

        // Host log lines about this mod, on the thread of the failing call (any thread).
        void OnHostLog(int a_level, const char* a_message, void*) {
            try {
                const auto text = Clip(a_message);
                if (a_level >= 2)
                    logger::error("Magelight host: {}", text);
                else if (a_level == 1)
                    logger::warn("Magelight host: {}", text);
                else
                    logger::info("Magelight host: {}", text);
            } catch (...) {
            }
        }

        // Undo a partial Install: the host destroys the views with the mod.
        bool Abandon() {
            g_accept.store(false);
            if (g_api && g_mod) g_api->UnregisterMod(g_mod);
            g_mod = 0;
            for (auto& s : g_slots) s.id.store(0);
            return false;
        }

        bool Create(const ml::MagelightApi4* a_api, Slot& a_slot, const std::string& a_path, ml::Layer a_layer,
                    bool a_clickThrough, ml::Result& a_result) {
            ml::ViewDesc vd{};
            vd.size = sizeof(vd);
            vd.name = a_slot.name;
            vd.htmlPath = a_path.c_str();
            vd.anchor = ml::Anchor::TopLeft;
            vd.fullscreen = true;
            vd.clickThrough = a_clickThrough;
            vd.startVisible = false;  // the hud is shown once its listener is in place
            vd.layer = a_layer;
            vd.uiScale = 0.0f;
            vd.onDomReady = nullptr;
            ml::ViewId id = 0;
            a_result = a_api->CreateViewEx(g_mod, &vd, &id);
            if (a_result != ml::Result::Ok || !id) return false;
            a_slot.id.store(id);
            return true;
        }

        bool Listen(ml::ViewId a_view, const char* a_viewName, const char* a_name, ml::JsListenerFn4 a_fn) {
            const auto r = g_api->RegisterJSListenerEx(a_view, a_name, a_fn, nullptr);
            if (r == ml::Result::Ok) return true;
            logger::error("Magelight: listener {} on the {} view refused ({}): {}, no Isekai Hero UI this session",
                          a_name, a_viewName, ResultName(r), LastError());
            return false;
        }
    }  // namespace

    bool Install() {
        try {
            if (g_ready.load()) return true;
            InitUserDirs();

            // Each step into the host logs first, so a crash inside one names it.
            logger::info("Magelight: requesting the API");
            const auto* api = ml::RequestApi4();
            if (!api) {
                logger::info("Magelight: Magelight UI not loaded, no Isekai Hero UI this session");
                return false;
            }
            const char* hostVersion = api->hostVersion ? api->hostVersion : "?";
            if (api->hostVersionNumber < kMinHost) {
                logger::warn("Magelight: Magelight UI {} is too old (1.7.0 or newer needed), no Isekai Hero UI this session",
                             hostVersion);
                return false;
            }

            // The header does not promise that CreateViewEx checks the file, and the system
            // view's load can wait until it is shown, so a missing page is caught here.
            std::string paths[2];
            for (std::size_t i = 0; i < 2; ++i) {
                const auto path = PagePath(g_slots[i].file);
                std::error_code ec;
                if (path.empty() || !std::filesystem::exists(path, ec)) {
                    logger::error("Magelight: page {}{} not found, no Isekai Hero UI this session", kPageDir,
                                  g_slots[i].file);
                    return false;
                }
                paths[i] = Utf8(path);
            }

            ml::ModDesc d{};
            d.size = sizeof(d);
            d.modId = kModId;
            d.displayName = kDisplayName;
            d.modVersion = 0;
            d.minHostVersion = kMinHost;
            d.callbackThread = ml::CallbackThread::GameThread;
            d.onEvent = &OnEvent;
            d.onLog = &OnHostLog;
            d.user = nullptr;
            d.sessionName = nullptr;  // isolated storage jar
            ml::ModId mod = 0;
            auto r = api->RegisterMod(&d, &mod);
            if (r != ml::Result::Ok || !mod) {
                // No ModId on failure, so there is no per-mod error text to read.
                logger::error("Magelight: mod {} not registered ({}), no Isekai Hero UI this session", kModId,
                              ResultName(r));
                return false;
            }
            g_api = api;
            g_mod = mod;
            g_hostVersion = hostVersion;
            g_accept.store(true, std::memory_order_release);

            // ---- system: every interactive screen ----
            Slot& sys = SlotOf(View::System);
            logger::info("Magelight: creating view system ({}{})", kPageDir, sys.file);
            if (!Create(api, sys, paths[0], ml::Layer::Panel, /*clickThrough=*/false, r)) {
                logger::error("Magelight: view system not created ({}): {}, no Isekai Hero UI this session",
                              ResultName(r), LastError());
                return Abandon();
            }
            const auto sysId = sys.id.load();
            // Opened often: paint it while hidden so the first open is not one long frame.
            if (api->QueryCapability("prepaint") == 1) {
                r = api->SetViewPrepaint(sysId, true);
                if (r != ml::Result::Ok) logger::warn("Magelight: SetViewPrepaint ({}): {}", ResultName(r), LastError());
            }
            if (api->QueryCapability("gamepad") == 1) {
                ml::GamepadConfig pad{};
                pad.size = sizeof(pad);
                pad.enabled = true;
                // The pad as keys (A Enter, B Escape, d-pad and left stick the arrows): the
                // page already moves focus on the arrows and presses on Enter itself, and
                // attaches no navigation scopes, so the navigation style would move nothing.
                pad.style = ml::kGamepadStyleKeys;
                pad.allowCursor = true;  // right-stick click: a pointer, the fallback for the tree
                pad.startInCursorMode = false;
                r = api->SetViewGamepad(sysId, &pad);
                if (r != ml::Result::Ok) logger::warn("Magelight: SetViewGamepad ({}): {}", ResultName(r), LastError());
            } else {
                logger::info("Magelight: the gamepad layer is off (VR, or switched off by the player), "
                             "the system view takes no pad navigation");
            }
            if (!Listen(sysId, sys.name, "isekai.action", &OnSystemAction) ||
                !Listen(sysId, sys.name, "isekai.ready", &OnSystemReady))
                return Abandon();

            // ---- hud: toasts, flourish, threat labels ----
            // Popup, not Hud: Hud is the lowest tier, under Panel, and a toast has to stay
            // visible over our own open screens (spec 6.2).
            Slot& hud = SlotOf(View::Hud);
            logger::info("Magelight: creating view hud ({}{}) on the Popup layer", kPageDir, hud.file);
            if (!Create(api, hud, paths[1], ml::Layer::Popup, /*clickThrough=*/true, r)) {
                const bool policy = r == ml::Result::Denied || r == ml::Result::InvalidArgument ||
                                    r == ml::Result::Unsupported;
                if (!policy) {
                    logger::error("Magelight: view hud not created ({}): {}, no Isekai Hero UI this session",
                                  ResultName(r), LastError());
                    return Abandon();
                }
                logger::warn("Magelight: view hud refused on the Popup layer ({}): {}", ResultName(r), LastError());
                logger::info("Magelight: creating view hud ({}{}) on the Hud layer", kPageDir, hud.file);
                if (!Create(api, hud, paths[1], ml::Layer::Hud, /*clickThrough=*/true, r)) {
                    logger::error("Magelight: view hud not created ({}): {}, no Isekai Hero UI this session",
                                  ResultName(r), LastError());
                    return Abandon();
                }
                g_hudBelow = true;
            }
            const auto hudId = hud.id.load();
            if (!Listen(hudId, hud.name, "isekai.ready", &OnHudReady)) return Abandon();
            // VR: glued to the head, toasts and the flourish in the upper field of view (spec 8).
            if (api->QueryCapability("vr") == 1) {
                ml::VRPlacementDesc place{};
                place.size = sizeof(place);
                place.mode = 0;  // HeadLocked; zero distance and width keep the host defaults
                r = api->SetViewVRPlacement(hudId, &place);
                if (r != ml::Result::Ok)
                    logger::warn("Magelight: SetViewVRPlacement on hud ({}): {}", ResultName(r), LastError());
            }
            api->ShowView(hudId, true);

            g_ready.store(true, std::memory_order_release);
            logger::info("Magelight UI {}: views system and hud ready{}", hostVersion,
                         g_hudBelow ? " (hud below panels)" : "");
            return true;
        } catch (const std::exception& e) {
            logger::error("Magelight: setup: {}", e.what());
        } catch (...) {
            logger::error("Magelight: setup: unknown exception");
        }
        return Abandon();
    }

    bool Available() { return g_ready.load(std::memory_order_acquire) && !g_dead.load(); }

    std::string_view HostVersion() {
        return g_ready.load(std::memory_order_acquire) ? std::string_view(g_hostVersion) : std::string_view{};
    }

    bool ViewCreated(View a_view) { return SlotOf(a_view).id.load() != 0; }

    bool PageReady(View a_view) {
        Slot& s = SlotOf(a_view);
        std::scoped_lock lock(s.mx);
        return s.gate.Ready();
    }

    std::string PageVersion(View a_view) {
        Slot& s = SlotOf(a_view);
        std::scoped_lock lock(s.mx);
        return s.version;
    }

    bool HudBelowPanels() { return g_ready.load(std::memory_order_acquire) && g_hudBelow; }

    void SetPrelude(View a_view, std::function<std::vector<std::string>()> a_build) {
        Slot& s = SlotOf(a_view);
        std::scoped_lock lock(s.mx);
        s.prelude = std::move(a_build);
    }

    void Run(View a_view, std::string a_js) {
        if (!Available()) return;
        try {
            Slot& s = SlotOf(a_view);
            // InvokeJS under the lock keeps the order across threads (see MarkPageReady).
            // InvokeJS only queues the script for the page; nothing it calls back into takes
            // this lock.
            std::scoped_lock lock(s.mx);
            if (auto now = s.gate.Submit(std::move(a_js))) {
                g_api->InvokeJS(s.id.load(), now->c_str());
                return;
            }
            // Queued. Past kMaxPending the gate drops the oldest call: say so once per
            // not-ready period.
            if (++s.queued > Logic::ReadyGate::kMaxPending && !s.fullWarned) {
                s.fullWarned = true;
                logger::warn("Magelight: {} page not ready and {} calls queued, dropping the oldest from now on",
                             s.name, Logic::ReadyGate::kMaxPending);
            }
        } catch (const std::exception& e) {
            logger::error("Magelight: Run: {}", e.what());
        } catch (...) {
            logger::error("Magelight: Run: unknown exception");
        }
    }

    bool RequestOpen() {
        if (!Available()) return false;
        const auto r = g_api->RequestUIMode(SlotOf(View::System).id.load(), ml::kUIModeFlagPause | ml::kUIModeFlagQueue);
        if (r == ml::Result::Ok) return true;
        if (r == ml::Result::RenderDead) g_dead.store(true);
        logger::warn("Magelight: system view not opened ({}): {}", ResultName(r), LastError());
        return false;
    }

    void Close() {
        if (!g_ready.load(std::memory_order_acquire)) return;
        // An entry that has not run yet is withdrawn instead (host 1.1.0, "uiModeCancel"):
        // no UIModeEntered follows, so the view is hidden here.
        if (g_api->QueryCapability("uiModeCancel") == 1) {
            const auto c = g_api->CancelUIModeRequest(g_mod);
            if (c == ml::Result::Ok) {
                g_api->ShowView(SlotOf(View::System).id.load(), false);
                return;
            }
            // Denied = nothing waiting to withdraw: the entry already ran, release below.
            if (c != ml::Result::Denied)
                logger::info("Magelight: CancelUIModeRequest ({}): {}", ResultName(c), LastError());
        }
        const auto r = g_api->ReleaseUIMode(g_mod);
        if (r != ml::Result::Ok) logger::info("Magelight: ReleaseUIMode ({}): {}", ResultName(r), LastError());
    }

    bool HoldsUiMode() { return g_ready.load(std::memory_order_acquire) && g_api->GetUIModeOwner() == g_mod; }

}  // namespace Isekai::UI::Magelight
