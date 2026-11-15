#include "UI/Prisma.h"

#include "Loc.h"          // Loc::Json, the string table this view translates itself with
#include "PrismaUI_API.h"
#include "Progression.h"  // OpenStatusPanel, to come back after a reroll
#include "Quests.h"       // Quests::Reroll from the status screen's "new task" button
#include "Shop.h"  // Shop::Open from the status screen's shop button
#include "SkillTree.h"
#include "Sounds.h"
#include "Storage.h"  // Storage::Open from the status screen's storage button
#include "System.h"   // PowerName, kMaxPerkPoints, DelayedMainThread, RebootSystem
#include "UI/Payloads.h"
#include "UI/Style.h"  // g_userScale, pushed into the view as its UI scale

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <format>
#include <mutex>
#include <string>

namespace Isekai::UI::Prisma {

    namespace {
        constexpr const char* kViewPath = "IsekaiHero/index.html";
        constexpr const char* kViewFile = "Data\\PrismaUI\\views\\IsekaiHero\\index.html";

        // Which screen the single view is showing.
        enum Screen : int { kNone = 0, kTree, kPanel, kFlourish, kStatus, kShop };

        PRISMA_UI_API::IVPrismaUI1* g_api = nullptr;
        PrismaView                  g_view = 0;
        std::atomic<bool>           g_ready{ false };
        std::atomic<int>            g_screen{ kNone };

        std::mutex               g_panelMutex;
        std::function<void(int)> g_panelOnSelect;  // guarded by g_panelMutex
        bool                     g_panelMulti = false;
        std::string              g_pendingCall;     // guarded by g_panelMutex

        void Invoke(const std::string& a_call) {
            if (!g_api) {
                return;
            }
            if (g_ready.load(std::memory_order_acquire)) {
                g_api->Invoke(g_view, a_call.c_str());
            } else {
                // A screen was requested before the DOM finished loading (a very early
                // reincarnation trigger). Remember the latest push and flush it on ready,
                // so the panel/tree isn't left blank.
                std::scoped_lock lock(g_panelMutex);
                g_pendingCall = a_call;
            }
        }

        void PushTree() { Invoke("window.isekaiShowTree(" + Payloads::Tree() + ")"); }

        void PushShop() { Invoke("window.isekaiShowShop(" + Payloads::Shop() + ")"); }

        // Unfocus + hide the whole view. MAIN THREAD.
        void HideView() {
            if (!g_api) {
                return;
            }
            g_api->Unfocus(g_view);
            g_api->Hide(g_view);
            g_screen.store(kNone, std::memory_order_release);
        }

        // --- PrismaUI callbacks (may arrive off the main thread → marshal) ---

        void OnDomReady(PrismaView) {
            g_ready.store(true, std::memory_order_release);
            logger::info("Prisma: view DOM ready");
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([]() {
                    // The interface size, before anything is drawn. Pushed here rather
                    // than at Install because Invoke only remembers ONE pending call
                    // before the DOM is ready, and a panel opening early would have
                    // replaced it — the scale would then have applied on the second
                    // screen the player ever saw.
                    if (g_api) {
                        g_api->Invoke(g_view,
                                      std::format("window.isekaiScale({})",
                                                  Style::g_userScale)
                                          .c_str());
                        // The translation table, for the same reason and before the same
                        // deadline as the scale: the view's own chrome is HTML, so it
                        // translates itself, and it has to hold the table before the
                        // pending call below paints the first screen.
                        g_api->Invoke(g_view,
                                      ("window.isekaiLoc(" + Loc::Json() + ")").c_str());
                    }
                    std::string call;
                    {
                        std::scoped_lock lock(g_panelMutex);
                        call = std::move(g_pendingCall);
                        g_pendingCall.clear();
                    }
                    if (!call.empty() && g_api) {
                        g_api->Invoke(g_view, call.c_str());
                    }
                });
            }
        }

        void OnBuy(const char* a_arg) {
            std::uint32_t key = 0;
            try {
                key = static_cast<std::uint32_t>(std::stoul(a_arg ? a_arg : "0"));
            } catch (...) {
                return;
            }
            if (key == 0) {
                return;
            }
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([key]() {
                    SkillTree::TryUnlock(key);
                    PushTree();
                });
            }
        }

        void OnRespec(const char*) {
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([]() {
                    SkillTree::Respec();
                    PushTree();  // reflect the refunded points and cleared nodes
                });
            }
        }

        void OnCloseTree(const char*) {
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([]() {
                    HideView();
                    Sounds::PlayDelayed(Sounds::Sfx::WindowClose, Sounds::kResumeGraceMs);
                });
            }
        }

        // A shop card was clicked. The argument is an index into Shop::Catalog(); the
        // re-push afterwards is what keeps the balance and the cards' affordability
        // current without the player reopening anything.
        void OnShopBuy(const char* a_arg) {
            int idx = -1;
            try {
                idx = std::stoi(a_arg ? a_arg : "-1");
            } catch (...) {
                return;
            }
            if (idx < 0) {
                return;
            }
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([idx]() {
                    Isekai::Shop::Buy(idx);
                    PushShop();
                });
            }
        }

        void OnShopClose(const char*) {
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([]() {
                    HideView();
                    Sounds::PlayDelayed(Sounds::Sfx::WindowClose, Sounds::kResumeGraceMs);
                });
            }
        }

        // The status screen's footer buttons. One listener, an action string — tree /
        // storage / shop / reroll / reboot / close.
        void OnStatusAction(const char* a_arg) {
            const std::string action = a_arg ? a_arg : "";
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([action]() {
                    if (action == "tree") {
                        OpenTree();  // switches screens within the view; no game menu
                    } else if (action == "storage") {
                        // The container is a real game menu, so the view must let go first.
                        HideView();
                        Sounds::PlayDelayed(Sounds::Sfx::WindowClose, Sounds::kResumeGraceMs);
                        Storage::Open();
                    } else if (action == "shop") {
                        // Switches to our own kShop screen from here; like "tree" above it
                        // stays inside the view, so no HideView and no game menu involved.
                        Isekai::Shop::Open();
                    } else if (action == "reroll") {
                        // Stays inside the view: swap the objective, then reopen the same
                        // status screen so the new one is simply there. Progression owns
                        // the status JSON, so the reopen goes back through it.
                        Quests::Reroll();
                        Progression::OpenStatusPanel();
                    } else if (action == "reboot") {
                        // Re-opens the blessing choice, which brings up its own panel
                        // (ShowPanel takes the view over from here) — no HideView needed.
                        Isekai::RebootSystem();
                    } else {  // "close"
                        HideView();
                        Sounds::PlayDelayed(Sounds::Sfx::WindowClose, Sounds::kResumeGraceMs);
                    }
                });
            }
        }

        void OnChoose(const char* a_arg) {
            int idx = -1;
            try {
                idx = std::stoi(a_arg ? a_arg : "-1");
            } catch (...) {
                return;
            }
            if (idx < 0) {
                return;
            }
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([idx]() {
                    std::function<void(int)> fn;
                    bool                     multi = false;
                    {
                        std::scoped_lock lock(g_panelMutex);
                        fn = std::move(g_panelOnSelect);
                        multi = g_panelMulti;
                        g_panelOnSelect = nullptr;
                    }
                    HideView();  // also unpauses (Unfocus)
                    // Selecting clicks; dismissing a single-button panel whooshes — same
                    // rule the ImGui panel uses, so chained panels don't double up sounds.
                    // Delayed past the unpause frame, or the engine's resume pass eats it.
                    Sounds::PlayDelayed(multi ? Sounds::Sfx::ButtonClick : Sounds::Sfx::WindowClose,
                                        Sounds::kResumeGraceMs);
                    if (fn) {
                        fn(idx);
                    }
                });
            }
        }
    }

    void Install() {
        // #35: a VR crash landed between the overlay hook and the next line we always log,
        // and every exit below logs only once it is done. These mark each call into
        // PrismaUI, so a crash inside one of them names it.
        logger::info("Prisma: requesting the API");
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
        if (!g_api) {
            logger::info("Prisma: PrismaUI not loaded — using the ImGui UI");
            return;
        }

        std::error_code ec;
        if (!std::filesystem::exists(kViewFile, ec)) {
            logger::info("Prisma: PrismaUI present but the view patch is not installed "
                         "({}) — using the ImGui UI",
                         kViewFile);
            g_api = nullptr;
            return;
        }

        logger::info("Prisma: creating the view");
        g_view = g_api->CreateView(kViewPath, OnDomReady);
        logger::info("Prisma: view {} created, registering listeners", g_view);
        if (!g_api->IsValid(g_view)) {
            logger::error("Prisma: CreateView failed — falling back to the ImGui UI");
            g_api = nullptr;
            return;
        }

        g_api->RegisterJSListener(g_view, "isekaiBuy", OnBuy);
        g_api->RegisterJSListener(g_view, "isekaiRespec", OnRespec);
        g_api->RegisterJSListener(g_view, "isekaiCloseTree", OnCloseTree);
        g_api->RegisterJSListener(g_view, "isekaiChoose", OnChoose);
        g_api->RegisterJSListener(g_view, "isekaiStatusAction", OnStatusAction);
        g_api->RegisterJSListener(g_view, "isekaiShopBuy", OnShopBuy);
        g_api->RegisterJSListener(g_view, "isekaiShopClose", OnShopClose);
        g_api->Hide(g_view);

        logger::info("Prisma: web UI active (view {})", g_view);
    }

    bool Active() {
        return g_api != nullptr && g_api->IsValid(g_view);
    }

    bool IsBusy() {
        const int s = g_screen.load(std::memory_order_acquire);
        return s == kTree || s == kPanel || s == kStatus || s == kShop;
    }

    void OpenTree() {
        if (!Active()) {
            return;
        }
        g_screen.store(kTree, std::memory_order_release);
        PushTree();
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/true);
        Sounds::Play(Sounds::Sfx::WindowOpen);
    }

    void OpenShop() {
        if (!Active()) {
            return;
        }
        g_screen.store(kShop, std::memory_order_release);
        PushShop();
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/true);
        Sounds::Play(Sounds::Sfx::WindowOpen);
    }

    void ShowPanel(std::string a_title, std::string a_body, std::vector<Choice> a_choices,
                   std::function<void(int)> a_onSelect, float a_revealCharsPerSec,
                   float a_width) {
        if (!Active()) {
            return;
        }
        const bool multi = a_choices.size() > 1;
        const std::string json = Payloads::Panel(
            { .title = a_title,
              .body = a_body,
              .choices = a_choices,
              .revealCharsPerSec = std::max(a_revealCharsPerSec, 1.0f),
              .widthHint = std::max(a_width, 300.0f),
              .tone = Logic::Tone::Cyan,
              .emblemTone = Logic::Tone::Cyan });
        {
            std::scoped_lock lock(g_panelMutex);
            g_panelOnSelect = std::move(a_onSelect);
            g_panelMulti = multi;
        }
        g_screen.store(kPanel, std::memory_order_release);
        Invoke("window.isekaiShowPanel(" + json + ")");
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/true);
        Sounds::Play(Sounds::Sfx::WindowOpen);
    }

    void ShowStatus(std::string a_json) {
        if (!Active()) {
            return;
        }
        g_screen.store(kStatus, std::memory_order_release);
        Invoke("window.isekaiShowStatus(" + a_json + ")");
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/true);
        Sounds::Play(Sounds::Sfx::WindowOpen);
    }

    void SendGamepad(const char* a_button) {
        if (!Active() || !a_button || !g_api || !g_api->HasFocus(g_view)) {
            return;
        }
        // Called from the game's input thread; every other call into the view runs on the
        // main thread, so this one queues rather than making that assumption twice.
        std::string call = std::string("window.isekaiPad('") + a_button + "')";
        if (auto* task = SKSE::GetTaskInterface()) {
            task->AddTask([call = std::move(call)]() { Invoke(call); });
        }
    }

    void Flourish(std::string a_title, std::string a_subtitle) {
        if (!Active()) {
            return;
        }
        std::string json = "{\"title\":\"" + Payloads::Escape(a_title.c_str()) + "\",\"subtitle\":\"" +
                           Payloads::Escape(a_subtitle.c_str()) + "\"}";
        Invoke("window.isekaiFlourish(" + json + ")");

        // A flourish is decoration, not a screen. When a real one already owns the view
        // it plays over it and must touch neither the screen id nor the visibility —
        // the view is on screen already, and the overlay times itself out.
        //
        // Claiming both is what broke buying the last rank of a mastery node: the
        // Grandmaster flourish set kFlourish while the tree was open, the tree re-push
        // that follows the purchase put the tree back but not the screen id, and the
        // flourish's own timer below then found kFlourish still set and hid the tree the
        // player was still using. Close, reopen, close.
        if (IsBusy()) {
            return;
        }

        g_screen.store(kFlourish, std::memory_order_release);
        g_api->Show(g_view);  // no Focus: purely visual, plays over gameplay

        // Auto-hide, but only if a real screen (tree/panel) hasn't taken the view over
        // in the meantime — otherwise the flourish timer would yank a panel off screen.
        Isekai::DelayedMainThread(2400, []() {
            if (g_screen.load(std::memory_order_acquire) == kFlourish) {
                HideView();
            }
        });
    }
}
