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

        // --- JSON helpers ---

        [[nodiscard]] std::string Esc(const char* a_s) {
            std::string out;
            for (const char* p = a_s; p && *p; ++p) {
                switch (*p) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(*p) >= 0x20) {
                        out += *p;
                    }
                    break;
                }
            }
            return out;
        }

        [[nodiscard]] const char* Bool(bool a_b) { return a_b ? "true" : "false"; }

        // Just the file name of an icon path ("...\\icons\\x.png" -> "x.png"); the web
        // view loads icons relative to itself as "icons/<name>".
        [[nodiscard]] std::string IconFile(const std::string& a_path) {
            const auto pos = a_path.find_last_of("\\/");
            return pos == std::string::npos ? a_path : a_path.substr(pos + 1);
        }

        // Whole tree + header. Reads game state (perk pool) — MAIN THREAD ONLY.
        [[nodiscard]] std::string BuildTreeJson() {
            std::size_t count = 0;
            const auto* nodes = SkillTree::Nodes(count);

            std::string j = "{";
            j += "\"points\":" + std::to_string(SkillTree::Points()) + ",";
            j += "\"perks\":" + std::to_string(SkillTree::PerkPool()) + ",";
            j += "\"perkMax\":" + std::to_string(Isekai::kMaxPerkPoints) + ",";
            j += "\"respec\":" + std::to_string(SkillTree::RespecRefund()) + ",";
            j += "\"nodes\":[";
            for (std::size_t i = 0; i < count; ++i) {
                const auto&        n = nodes[i];
                const std::int32_t rank = SkillTree::Rank(n.key);
                const bool         maxed = n.repeatable && n.maxRank > 0 && rank >= n.maxRank;
                const bool         owned = n.repeatable ? maxed : SkillTree::IsUnlocked(n.key);
                const std::int32_t tier = SkillTree::Tier(n.key);

                if (i != 0) {
                    j += ",";
                }
                j += "{";
                j += "\"key\":" + std::to_string(n.key) + ",";
                j += "\"name\":\"" + Esc(SkillTree::Name(n)) + "\",";
                j += "\"desc\":\"" + Esc(SkillTree::Desc(n)) + "\",";
                j += "\"icon\":\"" + Esc(n.icon) + "\",";
                j += "\"x\":" + std::to_string(static_cast<int>(n.x)) + ",";
                j += "\"y\":" + std::to_string(static_cast<int>(n.y)) + ",";
                // The NEXT purchase's price, not the flat table value — mastery nodes
                // (capped repeatables) rise per tier, so what buying costs right now
                // depends on the rank already owned.
                j += "\"cost\":" + std::to_string(SkillTree::NextCost(n.key)) + ",";
                j += "\"owned\":" + std::string(Bool(owned)) + ",";
                j += "\"tierMet\":" + std::string(Bool(SkillTree::TierMet(n.key))) + ",";
                j += "\"prereqMet\":" + std::string(Bool(SkillTree::PrereqsMet(n.key))) + ",";
                j += "\"repeatable\":" + std::string(Bool(n.repeatable)) + ",";
                j += "\"rank\":" + std::to_string(rank) + ",";
                j += "\"maxRank\":" + std::to_string(n.maxRank) + ",";
                j += "\"masteryTier\":" + std::to_string(tier) + ",";
                j += "\"masteryTierName\":\"" + Esc(SkillTree::TierName(tier)) + "\",";
                // Display grouping and size — the view frames the graph as labelled
                // zones and draws the landmarks (hub, capstone, gifts) larger.
                j += "\"zone\":\"" + Esc(SkillTree::ZoneName(n.zone)) + "\",";
                j += "\"scale\":" + std::to_string(n.scale) + ",";
                j += "\"reqPower\":\"" +
                     Esc(Isekai::PowerName(SkillTree::RequiredPower(n.key)).c_str()) + "\",";
                // >0 only for a DORMANT blessing: the seal is a level away, not a
                // rebirth away, and the tooltip should say so.
                j += "\"reqLevel\":" +
                     std::to_string(Isekai::AwakeningLevelFor(SkillTree::RequiredPower(n.key))) +
                     ",";
                j += "\"prereq\":[" + std::to_string(n.prereq[0]) + "," +
                     std::to_string(n.prereq[1]) + "]";
                j += "}";
            }
            j += "]}";
            return j;
        }

        [[nodiscard]] std::string BuildPanelJson(const std::string& a_title,
                                                 const std::string& a_body,
                                                 const std::vector<Choice>& a_choices,
                                                 float a_reveal, float a_width) {
            std::string j = "{";
            j += "\"title\":\"" + Esc(a_title.c_str()) + "\",";
            j += "\"body\":\"" + Esc(a_body.c_str()) + "\",";
            j += "\"reveal\":" + std::to_string(static_cast<int>(a_reveal)) + ",";
            j += "\"width\":" + std::to_string(static_cast<int>(a_width)) + ",";
            j += "\"buttons\":[";
            for (std::size_t i = 0; i < a_choices.size(); ++i) {
                const auto& c = a_choices[i];
                if (i != 0) {
                    j += ",";
                }
                j += "{\"label\":\"" + Esc(c.label.c_str()) + "\",";
                j += "\"icon\":\"" + Esc(IconFile(c.icon).c_str()) + "\",";
                j += "\"iconOnly\":" + std::string(Bool(c.iconOnly)) + "}";
            }
            j += "]}";
            return j;
        }

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

        void PushTree() { Invoke("window.isekaiShowTree(" + BuildTreeJson() + ")"); }

        // The shop catalog + the current balance. Reads game state — MAIN THREAD ONLY.
        [[nodiscard]] std::string BuildShopJson() {
            std::string j = "{\"points\":" + std::to_string(GetState().systemPoints) + ",";

            // The shelves come over as their own list, in order, so an empty category
            // (potions before their ESP records exist) still holds its place in the rail
            // instead of vanishing and reordering the ones around it.
            j += "\"shelves\":[";
            const auto shelves = Isekai::Shop::Shelves();
            for (std::size_t i = 0; i < shelves.size(); ++i) {
                j += (i ? "," : "") + ("\"" + Esc(shelves[i].c_str()) + "\"");
            }
            j += "],\"items\":[";

            const auto items = Isekai::Shop::Catalog();
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i != 0) {
                    j += ",";
                }
                j += "{\"name\":\"" + Esc(items[i].name.c_str()) + "\",";
                j += "\"qty\":\"" + Esc(items[i].qty.c_str()) + "\",";
                // One effect per line; Esc turns the newlines into \n and the card renders
                // them with white-space: pre-line.
                j += "\"fx\":\"" + Esc(items[i].effects.c_str()) + "\",";
                j += "\"cost\":" + std::to_string(items[i].cost) + ",";
                j += "\"shelf\":\"" + Esc(items[i].shelf.c_str()) + "\",";
                j += "\"icon\":\"" + Esc(IconFile(items[i].icon).c_str()) + "\"}";
            }
            j += "]}";
            return j;
        }

        void PushShop() { Invoke("window.isekaiShowShop(" + BuildShopJson() + ")"); }

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

        g_view = g_api->CreateView(kViewPath, OnDomReady);
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
        const std::string json = BuildPanelJson(a_title, a_body, a_choices,
                                                std::max(a_revealCharsPerSec, 1.0f),
                                                std::max(a_width, 300.0f));
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
        std::string json = "{\"title\":\"" + Esc(a_title.c_str()) + "\",\"subtitle\":\"" +
                           Esc(a_subtitle.c_str()) + "\"}";
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
