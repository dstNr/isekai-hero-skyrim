#pragma once

// Public API for Isekai Hero — https://github.com/dstNr/isekai-hero-skyrim
//
// Vendor this single file into your SKSE plugin. It pulls in nothing from the mod's
// own source - only <Windows.h>, to resolve the export, and <cstdint>. MIT licensed,
// like the mod's code.
//
// Nothing here throws, and no call blocks. If Isekai Hero is not installed you get
// a nullptr and your plugin carries on.

// Guarded exactly the way PrismaUI's own SKSE header guards it: if you have already
// included <Windows.h> with your own macro choices, yours win and this is a no-op.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <cstdint>

namespace IsekaiHeroAPI
{
    // Bumped only when a NEW interface class is added. V1 is frozen: no method is
    // ever added to, removed from, or reordered within IVIsekaiHero1, so a plugin
    // built against it keeps working against every later version of the mod.
    enum class InterfaceVersion : uint8_t
    {
        V1 = 1,
    };

    // The blessing tier the player currently holds.
    enum class Tier : uint8_t
    {
        Normal   = 0,
        Hero     = 1,
        Ascended = 2,
    };

    // The derived System Rank, read off milestones, level and tree investment.
    enum class Rank : uint8_t
    {
        E = 0,
        D = 1,
        C = 2,
        B = 3,
        A = 4,
        S = 5,
    };

    // Skill-tree node keys. These are stable for the life of the mod: the mod's own
    // co-save stores them, so a key can never be renumbered or reused without
    // breaking every existing save. New nodes take new keys; nothing here moves.
    namespace Keys
    {
        inline constexpr uint32_t kSystemCore        = 1;
        inline constexpr uint32_t kDragonsVoice      = 2;
        inline constexpr uint32_t kVitalSurge        = 3;
        inline constexpr uint32_t kThuumOmniscience  = 4;
        inline constexpr uint32_t kEmberguard        = 5;
        inline constexpr uint32_t kManaWell          = 6;
        inline constexpr uint32_t kArcaneOmniscience = 7;
        inline constexpr uint32_t kFrostguard        = 8;
        inline constexpr uint32_t kSpellOmniscience  = 9;
        inline constexpr uint32_t kSwiftBlood        = 10;
        inline constexpr uint32_t kAlchemicalInsight = 11;
        inline constexpr uint32_t kPlagueward        = 12;
        inline constexpr uint32_t kWorldTree         = 13;
        inline constexpr uint32_t kPerkSynthesis     = 14;
        inline constexpr uint32_t kFleetOfFoot       = 15;  // repeatable
        inline constexpr uint32_t kBeastOfBurden     = 16;  // repeatable
        inline constexpr uint32_t kEnduringVigor     = 17;  // repeatable
        inline constexpr uint32_t kStormWard         = 18;  // repeatable
        inline constexpr uint32_t kWardedMind        = 19;  // repeatable
        inline constexpr uint32_t kArcaneAbsorption  = 20;  // repeatable
        inline constexpr uint32_t kIronSkin          = 21;  // repeatable
        inline constexpr uint32_t kRapidRecovery     = 22;  // repeatable
    }

    // Every read below is a main-thread call: call them from an SKSE task, a message
    // handler, or your own game-thread code. They are cheap and none of them blocks.
    // GrantSystemPoints is the one exception - it is safe from any thread.
    class IVIsekaiHero1
    {
    public:
        // --- state ---

        // False until the player has been through the System's boot sequence. Every
        // other read below is meaningless while this is false.
        virtual bool IsActive() = 0;

        // A Tier value. uint8_t on the wire so adding a tier later cannot change
        // this interface's layout.
        virtual uint8_t GetTier() = 0;

        // The tier's display name in the player's language. For showing, not for
        // comparing - compare GetTier(). Valid until your next call on this
        // interface; copy it if you keep it.
        virtual const char* GetTierName() = 0;

        // Unspent System Points.
        virtual int32_t GetSystemPoints() = 0;

        // A Rank value.
        virtual uint8_t GetRank() = 0;

        // --- skill tree ---

        // Does the player hold this node. Same as GetNodeRank(key) > 0.
        virtual bool HasNode(uint32_t key) = 0;

        // 0 when the player does not hold the node, 1 for a one-shot node they do
        // hold, and the purchase count for a repeatable one.
        virtual int32_t GetNodeRank(uint32_t key) = 0;

        // --- reward ---

        // Award the player System Points. Applied on the game's main thread, so it
        // is safe to call from anywhere; it does not take effect inside this call.
        //
        // `source` names your mod and is written to Isekai Hero's log with every
        // grant. Pass something recognisable: when a player reports an implausible
        // point total, this is what identifies where the points came from.
        virtual void GrantSystemPoints(int32_t amount, const char* source) = 0;
    };

    using RequestPluginAPIFunc = void* (*)(InterfaceVersion);

    // Obtain the interface. Returns nullptr when Isekai Hero is not installed, or
    // when it does not know the version you asked for.
    //
    // Call no earlier than SKSE's kPostLoad message - before that the mod's DLL may
    // not be in the process yet. Requires <Windows.h>, which your SKSE plugin's PCH
    // already includes.
    [[nodiscard]] inline IVIsekaiHero1* RequestInterface(
        InterfaceVersion a_version = InterfaceVersion::V1)
    {
        const auto handle = GetModuleHandleA("IsekaiHeroSKSE.dll");
        if (!handle) {
            return nullptr;
        }
        const auto fn = reinterpret_cast<RequestPluginAPIFunc>(
            GetProcAddress(handle, "RequestPluginAPI"));
        if (!fn) {
            return nullptr;
        }
        return static_cast<IVIsekaiHero1*>(fn(a_version));
    }
}
