#include "Enchantments.h"

#include "EnchantLogic.h"
#include "FrameTick.h"
#include "Plugin.h"
#include "TickThrottle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <unordered_map>

namespace Isekai::Enchantments {

    namespace {
        using Flag = RE::TESForm::RecordFlags;

        // Node key -> base ENCH in IsekaiHero.esp (docs/CREATION_KIT_ESP.md, Part M).
        struct Entry {
            std::uint32_t        nodeKey;
            RE::FormID           localID;
            RE::EnchantmentItem* form = nullptr;
        };
        std::array<Entry, 8> g_entries{ {
            { 24, 0xDAC },  // Divine Judgment
            { 25, 0xDAD },  // Stormchain
            { 26, 0xDAE },  // Soul Rend
            { 27, 0xDAF },  // System Breach
            { 28, 0xDB0 },  // Bulwark
            { 29, 0xDB1 },  // Thorncrown
            { 30, 0xDB2 },  // Phoenix Heart
            { 31, 0xDB3 },  // Voidstep
        } };

        constexpr RE::FormID kStormchainCarrier = 0xDA1;
        constexpr RE::FormID kBreachCarrier = 0xDA5;
        constexpr RE::FormID kPhoenixCarrier = 0xDA9;
        constexpr RE::FormID kStormchainJump = 0xDB4;
        constexpr RE::FormID kBreachArt = 0xD99;

        RE::EffectSetting* g_stormchainCarrier = nullptr;
        RE::EffectSetting* g_breachCarrier = nullptr;
        RE::EffectSetting* g_phoenixCarrier = nullptr;
        RE::SpellItem*     g_stormchainJump = nullptr;
        RE::BGSArtObject*  g_breachArt = nullptr;

        template <class T>
        T* Lookup(RE::FormID a_localID, const char* a_what) {
            auto* form = Plugin::LookupOurForm<T>(a_localID);
            if (!form) {
                logger::error("Enchantments: {} {:#05x} not found in {}", a_what, a_localID,
                              Plugin::kFileName);
            }
            return form;
        }
    }

    void Resolve() {
        for (auto& e : g_entries) {
            e.form = Lookup<RE::EnchantmentItem>(e.localID, "ENCH");
        }
        g_stormchainCarrier = Lookup<RE::EffectSetting>(kStormchainCarrier, "MGEF");
        g_breachCarrier = Lookup<RE::EffectSetting>(kBreachCarrier, "MGEF");
        g_phoenixCarrier = Lookup<RE::EffectSetting>(kPhoenixCarrier, "MGEF");
        g_stormchainJump = Lookup<RE::SpellItem>(kStormchainJump, "SPEL");
        g_breachArt = Lookup<RE::BGSArtObject>(kBreachArt, "ARTO");
        logger::info("Enchantments: {} of {} System enchantments resolved", Resolved(),
                     g_entries.size());
    }

    bool Teach(std::uint32_t a_nodeKey) {
        for (const auto& e : g_entries) {
            if (e.nodeKey == a_nodeKey && e.form) {
                e.form->formFlags |= Flag::kKnown;
                return true;
            }
        }
        return false;
    }

    void SyncKnown(std::span<const std::uint32_t> a_unlockedKeys) {
        for (const auto& e : g_entries) {
            if (!e.form) {
                continue;
            }
            if (std::ranges::find(a_unlockedKeys, e.nodeKey) != a_unlockedKeys.end()) {
                e.form->formFlags |= Flag::kKnown;
            } else {
                e.form->formFlags &= ~static_cast<std::uint32_t>(Flag::kKnown);
            }
        }
    }

    bool IsSystemEnchantment(const RE::EnchantmentItem* a_ench) {
        return a_ench && std::ranges::any_of(g_entries, [a_ench](const Entry& e) { return e.form == a_ench; });
    }

    std::size_t Resolved() {
        return static_cast<std::size_t>(
            std::ranges::count_if(g_entries, [](const Entry& e) { return e.form != nullptr; }));
    }

    RE::EffectSetting* StormchainCarrier() { return g_stormchainCarrier; }
    RE::EffectSetting* BreachCarrier() { return g_breachCarrier; }
    RE::EffectSetting* PhoenixCarrier() { return g_phoenixCarrier; }
    RE::SpellItem*     StormchainJump() { return g_stormchainJump; }
    RE::BGSArtObject*  BreachArt() { return g_breachArt; }

    namespace {
        double                                  g_playSeconds = 0.0;
        std::unordered_map<RE::FormID, double>  g_readyAt;   // actor -> PlaySeconds when ready

        void ClockTick() {
            using Clock = std::chrono::steady_clock;
            static TickThrottle  throttle{ std::chrono::seconds(1) };
            static Clock::time_point last{};
            const auto now = Clock::now();
            if (!throttle.Due(now)) {
                return;
            }
            // FrameTick is silent while paused, so a long gap since the last tick is a pause
            // (or a load) and must not count as play; clamp it.
            if (const auto ui = RE::UI::GetSingleton(); last != Clock::time_point{} && !(ui && ui->GameIsPaused())) {
                g_playSeconds += std::clamp(std::chrono::duration<double>(now - last).count(), 0.0, 2.0);
            }
            last = now;
        }
    }

    void InstallClock() { FrameTick::Add(&ClockTick); }

    double PlaySeconds() { return g_playSeconds; }

    double PhoenixReadyAt(RE::FormID a_actor) {
        const auto it = g_readyAt.find(a_actor);
        return it == g_readyAt.end() ? 0.0 : it->second;
    }

    void PhoenixStartCooldown(RE::FormID a_actor) {
        g_readyAt[a_actor] = g_playSeconds + Enchant::Logic::kPhoenixCooldownSeconds;
    }

    void ClearCooldowns() {
        g_playSeconds = 0.0;
        g_readyAt.clear();
    }

    void SaveCooldowns(SKSE::SerializationInterface* a_intf) {
        std::vector<std::pair<RE::FormID, double>> live;
        for (const auto& [id, ready] : g_readyAt) {
            if (Enchant::Logic::CooldownLive(ready, g_playSeconds)) {
                live.emplace_back(id, ready);
            }
        }
        a_intf->WriteRecordData(g_playSeconds);
        a_intf->WriteRecordData(static_cast<std::uint32_t>(live.size()));
        for (const auto& [id, ready] : live) {
            a_intf->WriteRecordData(id);
            a_intf->WriteRecordData(ready);
        }
    }

    void LoadCooldowns(SKSE::SerializationInterface* a_intf) {
        ClearCooldowns();
        double        play = 0.0;
        std::uint32_t count = 0;
        a_intf->ReadRecordData(play);
        a_intf->ReadRecordData(count);
        if (!(play >= 0.0)) {   // also rejects NaN
            play = 0.0;
        }
        g_playSeconds = play;
        if (!Enchant::Logic::CooldownCountAcceptable(count)) {
            logger::error("Co-save: phoenix cooldowns claim {} entries - ignoring them", count);
            return;
        }
        for (std::uint32_t i = 0; i < count; ++i) {
            RE::FormID id = 0;
            double     ready = 0.0;
            a_intf->ReadRecordData(id);
            a_intf->ReadRecordData(ready);
            RE::FormID resolved = 0;
            if (a_intf->ResolveFormID(id, resolved) && Enchant::Logic::CooldownLive(ready, g_playSeconds)) {
                g_readyAt[resolved] = ready;
            }
        }
    }
}
