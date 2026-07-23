#include "UI/PrismaTree.h"

#include "PrismaUI_API.h"
#include "SkillTree.h"
#include "Sounds.h"
#include "System.h"  // PowerName, kMaxPerkPoints

#include <atomic>
#include <filesystem>
#include <string>

namespace Isekai::UI::PrismaTree {

    namespace {
        // The view's html path, relative to Data\PrismaUI\views\ (PrismaUI's own root).
        constexpr const char* kViewPath = "IsekaiHero/index.html";
        // Where that file actually lives on disk — the gate for "is the patch installed".
        constexpr const char* kViewFile = "Data\\PrismaUI\\views\\IsekaiHero\\index.html";

        PRISMA_UI_API::IVPrismaUI1* g_api = nullptr;
        PrismaView                  g_view = 0;
        std::atomic<bool>           g_ready{ false };  // DOM ready, data may be pushed
        std::atomic<bool>           g_open{ false };

        // --- JSON helpers (no dependency pulled in for a payload this small) ---

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

        // Serialize the whole tree + header. Reads game state (perk pool → player), so
        // MAIN THREAD ONLY.
        [[nodiscard]] std::string BuildTreeJson() {
            std::size_t count = 0;
            const auto* nodes = SkillTree::Nodes(count);

            std::string j = "{";
            j += "\"points\":" + std::to_string(SkillTree::Points()) + ",";
            j += "\"perks\":" + std::to_string(SkillTree::PerkPool()) + ",";
            j += "\"perkMax\":" + std::to_string(Isekai::kMaxPerkPoints) + ",";
            j += "\"nodes\":[";
            for (std::size_t i = 0; i < count; ++i) {
                const auto&        n = nodes[i];
                const std::int32_t rank = SkillTree::Rank(n.key);
                const bool         maxed = n.repeatable && n.maxRank > 0 && rank >= n.maxRank;
                const bool         owned =
                    n.repeatable ? maxed : SkillTree::IsUnlocked(n.key);

                if (i != 0) {
                    j += ",";
                }
                j += "{";
                j += "\"key\":" + std::to_string(n.key) + ",";
                j += "\"name\":\"" + Esc(n.name) + "\",";
                j += "\"desc\":\"" + Esc(n.desc) + "\",";
                j += "\"icon\":\"" + Esc(n.icon) + "\",";
                j += "\"x\":" + std::to_string(static_cast<int>(n.x)) + ",";
                j += "\"y\":" + std::to_string(static_cast<int>(n.y)) + ",";
                j += "\"cost\":" + std::to_string(n.cost) + ",";
                j += "\"owned\":" + std::string(Bool(owned)) + ",";
                j += "\"tierMet\":" + std::string(Bool(SkillTree::TierMet(n.key))) + ",";
                j += "\"prereqMet\":" + std::string(Bool(SkillTree::PrereqsMet(n.key))) + ",";
                j += "\"repeatable\":" + std::string(Bool(n.repeatable)) + ",";
                j += "\"rank\":" + std::to_string(rank) + ",";
                j += "\"maxRank\":" + std::to_string(n.maxRank) + ",";
                j += "\"reqPower\":\"" +
                     Esc(Isekai::PowerName(SkillTree::RequiredPower(n.key)).c_str()) + "\",";
                j += "\"prereq\":[" + std::to_string(n.prereq[0]) + "," +
                     std::to_string(n.prereq[1]) + "]";
                j += "}";
            }
            j += "]}";
            return j;
        }

        // Hand the fresh tree to JS. MAIN THREAD ONLY (BuildTreeJson reads game state).
        void PushData() {
            if (!g_api || !g_ready.load(std::memory_order_acquire)) {
                return;
            }
            const std::string call = "window.isekaiSetTree(" + BuildTreeJson() + ")";
            g_api->Invoke(g_view, call.c_str());
        }

        void PushDataOnMainThread() {
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([]() { PushData(); });
            }
        }

        // --- PrismaUI callbacks (may arrive off the main thread) ---

        void OnDomReady(PrismaView) {
            g_ready.store(true, std::memory_order_release);
            logger::info("PrismaTree: view DOM ready");
            PushDataOnMainThread();  // seed the tree the first time
        }

        // JS -> C++: a node was clicked. Argument is the node key as a decimal string.
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
                    SkillTree::TryUnlock(key);  // ignores an illegal buy on its own
                    PushData();                 // reflect the new state back to JS
                });
            }
        }

        // JS -> C++: the view asked to close (its own ESC / close button).
        void OnClose(const char*) {
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([]() { Close(); });
            }
        }
    }

    void Install() {
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
        if (!g_api) {
            logger::info("PrismaTree: PrismaUI not loaded — using the ImGui skill tree");
            return;
        }

        std::error_code ec;
        if (!std::filesystem::exists(kViewFile, ec)) {
            logger::info("PrismaTree: PrismaUI present but the view patch is not installed "
                         "({}) — using the ImGui skill tree",
                         kViewFile);
            g_api = nullptr;
            return;
        }

        g_view = g_api->CreateView(kViewPath, OnDomReady);
        if (!g_api->IsValid(g_view)) {
            logger::error("PrismaTree: CreateView failed — falling back to the ImGui tree");
            g_api = nullptr;
            return;
        }

        g_api->RegisterJSListener(g_view, "isekaiBuy", OnBuy);
        g_api->RegisterJSListener(g_view, "isekaiClose", OnClose);
        g_api->Hide(g_view);  // dormant until the player opens the tree

        logger::info("PrismaTree: web skill tree active (view {})", g_view);
    }

    bool Active() {
        return g_api != nullptr && g_api->IsValid(g_view);
    }

    bool IsOpen() {
        return g_open.load(std::memory_order_acquire);
    }

    void Open() {
        if (!Active()) {
            return;
        }
        PushData();  // refresh before it appears (points spent since last time, etc.)
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/true);
        g_open.store(true, std::memory_order_release);
        Sounds::Play(Sounds::Sfx::WindowOpen);
    }

    void Close() {
        if (!Active()) {
            return;
        }
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
        g_open.store(false, std::memory_order_release);
        Sounds::Play(Sounds::Sfx::WindowClose);
    }
}
