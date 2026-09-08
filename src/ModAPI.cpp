#include "PCH.h"

#include "ModAPI.h"

#include "../include/IsekaiHeroAPI.h"
#include "Logger.h"
#include "Plugin.h"
#include "Progression.h"
#include "SkillTree.h"
#include "System.h"

namespace Isekai::ModAPI {

    namespace {

        // Storage for GetTierName's return value. The interface promises a const char*
        // valid until the caller's next call, which is exactly this buffer's lifetime.
        // A std::string return would be an ABI trap: a consumer built against another
        // STL would read a different layout.
        std::string g_tierName;

        [[nodiscard]] std::uint8_t TierValue(PowerLevel a_power) {
            switch (a_power) {
                case PowerLevel::Hero:
                    return static_cast<std::uint8_t>(IsekaiHeroAPI::Tier::Hero);
                case PowerLevel::Ascended:
                    return static_cast<std::uint8_t>(IsekaiHeroAPI::Tier::Ascended);
                default:
                    return static_cast<std::uint8_t>(IsekaiHeroAPI::Tier::Normal);
            }
        }

        [[nodiscard]] std::uint8_t RankValue(const std::string& a_rank) {
            if (a_rank == "S") return static_cast<std::uint8_t>(IsekaiHeroAPI::Rank::S);
            if (a_rank == "A") return static_cast<std::uint8_t>(IsekaiHeroAPI::Rank::A);
            if (a_rank == "B") return static_cast<std::uint8_t>(IsekaiHeroAPI::Rank::B);
            if (a_rank == "C") return static_cast<std::uint8_t>(IsekaiHeroAPI::Rank::C);
            if (a_rank == "D") return static_cast<std::uint8_t>(IsekaiHeroAPI::Rank::D);
            return static_cast<std::uint8_t>(IsekaiHeroAPI::Rank::E);
        }

        class Interface1 final : public IsekaiHeroAPI::IVIsekaiHero1
        {
        public:
            bool IsActive() override {
                return Plugin::IsLoaded() && GetState().reincarnated;
            }

            std::uint8_t GetTier() override {
                return TierValue(GetState().power);
            }

            const char* GetTierName() override {
                g_tierName = PowerName(GetState().power);
                return g_tierName.c_str();
            }

            std::int32_t GetSystemPoints() override {
                return GetState().systemPoints;
            }

            std::uint8_t GetRank() override {
                return RankValue(Progression::SystemRank());
            }

            bool HasNode(std::uint32_t a_key) override {
                return GetNodeRank(a_key) > 0;
            }

            std::int32_t GetNodeRank(std::uint32_t a_key) override {
                // A one-shot node lives in unlockedNodes and has no entry in nodeRanks,
                // so IsUnlocked has to be asked separately — SkillTree::Rank alone would
                // report 0 for a node the player definitely holds.
                if (SkillTree::IsUnlocked(a_key)) {
                    return 1;
                }
                return SkillTree::Rank(a_key);
            }

            void GrantSystemPoints(std::int32_t a_amount, const char* a_source) override {
                const std::string source = (a_source && *a_source) ? a_source : "<unnamed>";

                // Dropped rather than queued: with no save loaded there is no state for
                // the points to land in, and a queue that outlived a return to the main
                // menu would pay them to whichever character is loaded next.
                if (!Plugin::IsLoaded()) {
                    logger::warn(
                        "ModAPI: '{}' tried to grant {} System Point(s) with no save loaded "
                        "— dropped",
                        source, a_amount);
                    return;
                }

                if (a_amount == 0) {
                    return;
                }

                // The caller is on whatever thread it likes. Progression state belongs to
                // the main thread, and mutating it from a worker is how a save gets
                // corrupted.
                if (auto* task = SKSE::GetTaskInterface()) {
                    task->AddTask([a_amount, source]() {
                        if (!Plugin::IsLoaded()) {
                            return;  // the save went away between the call and this frame
                        }
                        GetState().systemPoints += a_amount;
                        logger::info("ModAPI: '{}' granted {} System Point(s), now {}", source,
                                     a_amount, GetState().systemPoints);
                    });
                }
            }
        };

        Interface1 g_interface1;

        // The last state we told the world about. Compared each tick; a message goes out
        // only when something actually moved, so a listener is not woken once a second
        // for nothing.
        IsekaiHeroAPI::StateChanged g_published{};
        bool                        g_everPublished = false;
    }

    void Install() {
        logger::info("ModAPI: RequestPluginAPI is exported, interface V1 available");
    }

    void PublishIfChanged() {
        if (!Plugin::IsLoaded()) {
            return;
        }

        IsekaiHeroAPI::StateChanged now{};
        now.active = GetState().reincarnated;
        now.tier = TierValue(GetState().power);
        now.rank = RankValue(Progression::SystemRank());
        now.systemPoints = GetState().systemPoints;

        if (g_everPublished && now.active == g_published.active && now.tier == g_published.tier &&
            now.rank == g_published.rank && now.systemPoints == g_published.systemPoints) {
            return;
        }

        g_published = now;
        g_everPublished = true;

        // Dispatched by the address of g_published rather than a stack copy, so the
        // pointer a listener receives stays valid for the whole of its handler. Only
        // ever written from the main thread, which is the only thread that gets here.
        if (auto* msg = SKSE::GetMessagingInterface()) {
            msg->Dispatch(static_cast<std::uint32_t>(IsekaiHeroAPI::MessageType::kStateChanged),
                          &g_published, sizeof(g_published), nullptr);
        }
    }

    void* Resolve(std::uint8_t a_version) {
        if (a_version == static_cast<std::uint8_t>(IsekaiHeroAPI::InterfaceVersion::V1)) {
            return static_cast<IsekaiHeroAPI::IVIsekaiHero1*>(&g_interface1);
        }
        // A consumer built against a newer header than this build knows. Refuse cleanly;
        // never hand back an interface whose layout we cannot promise.
        logger::warn("ModAPI: a plugin asked for interface version {}, which this build does "
                     "not have",
                     static_cast<int>(a_version));
        return nullptr;
    }
}

// Exported by name and resolved with GetProcAddress, so the name is a contract: do not
// rename it, and do not let the linker decorate it. It stays a one-line forward so that
// everything it touches can keep internal linkage.
extern "C" __declspec(dllexport) void* RequestPluginAPI(
    IsekaiHeroAPI::InterfaceVersion a_version)
{
    return Isekai::ModAPI::Resolve(static_cast<std::uint8_t>(a_version));
}
