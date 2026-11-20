#include "Enchantments.h"

#include "EnchantLogic.h"
#include "FrameTick.h"
#include "Plugin.h"
#include "TickThrottle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

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
        constexpr RE::FormID kPhoenixShader = 0xD97;   // IsekaiShaderPhoenixHeart, the ember look

        RE::EffectSetting*   g_stormchainCarrier = nullptr;
        RE::EffectSetting*   g_breachCarrier = nullptr;
        RE::EffectSetting*   g_phoenixCarrier = nullptr;
        RE::SpellItem*       g_stormchainJump = nullptr;
        RE::BGSArtObject*    g_breachArt = nullptr;
        RE::TESEffectShader* g_phoenixShader = nullptr;

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
        g_phoenixShader = Lookup<RE::TESEffectShader>(kPhoenixShader, "EFSH");
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

    bool IsKnown(std::uint32_t a_nodeKey) {
        return std::ranges::any_of(g_entries, [a_nodeKey](const Entry& e) {
            return e.nodeKey == a_nodeKey && e.form && (e.form->formFlags & static_cast<std::uint32_t>(Flag::kKnown)) != 0;
        });
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
        // The hit sinks, the frame tick and the co-save callbacks all reach this store.
        std::mutex                              g_cooldownLock;
        double                                  g_playSeconds = 0.0;
        std::unordered_map<RE::FormID, double>  g_readyAt;   // actor -> PlaySeconds when ready

        void PhoenixWatch();   // below, with the effects

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
                const std::scoped_lock lock{ g_cooldownLock };
                g_playSeconds += std::clamp(std::chrono::duration<double>(now - last).count(), 0.0, 2.0);
            }
            last = now;
            PhoenixWatch();   // outside the lock: it calls into the engine
        }
    }

    void InstallClock() { FrameTick::Add(&ClockTick); }

    double PlaySeconds() {
        const std::scoped_lock lock{ g_cooldownLock };
        return g_playSeconds;
    }

    double PhoenixReadyAt(RE::FormID a_actor) {
        const std::scoped_lock lock{ g_cooldownLock };
        const auto it = g_readyAt.find(a_actor);
        return it == g_readyAt.end() ? 0.0 : it->second;
    }

    void PhoenixStartCooldown(RE::FormID a_actor) {
        const std::scoped_lock lock{ g_cooldownLock };
        g_readyAt[a_actor] = g_playSeconds + Enchant::Logic::kPhoenixCooldownSeconds;
    }

    void ClearCooldowns() {
        const std::scoped_lock lock{ g_cooldownLock };
        g_playSeconds = 0.0;
        g_readyAt.clear();
    }

    void SaveCooldowns(SKSE::SerializationInterface* a_intf) {
        std::vector<std::pair<RE::FormID, double>> live;
        double play = 0.0;
        {
            const std::scoped_lock lock{ g_cooldownLock };
            play = g_playSeconds;
            for (const auto& [id, ready] : g_readyAt) {
                if (Enchant::Logic::CooldownLive(ready, play)) {
                    live.emplace_back(id, ready);
                }
            }
        }
        a_intf->WriteRecordData(play);
        a_intf->WriteRecordData(static_cast<std::uint32_t>(live.size()));
        for (const auto& [id, ready] : live) {
            a_intf->WriteRecordData(id);
            a_intf->WriteRecordData(ready);
        }
    }

    void LoadCooldowns(SKSE::SerializationInterface* a_intf) {
        double        play = 0.0;
        std::uint32_t count = 0;
        a_intf->ReadRecordData(play);
        a_intf->ReadRecordData(count);
        if (!std::isfinite(play) || play < 0.0) {   // an infinite clock would end every cooldown at once
            play = 0.0;
        }
        std::unordered_map<RE::FormID, double> readyAt;
        if (!Enchant::Logic::CooldownCountAcceptable(count)) {
            logger::error("Co-save: phoenix cooldowns claim {} entries - ignoring them", count);
            count = 0;
        }
        for (std::uint32_t i = 0; i < count; ++i) {
            RE::FormID id = 0;
            double     ready = 0.0;
            a_intf->ReadRecordData(id);
            a_intf->ReadRecordData(ready);
            ready = Enchant::Logic::ClampReadyAt(ready, play);
            RE::FormID resolved = 0;
            if (a_intf->ResolveFormID(id, resolved) && Enchant::Logic::CooldownLive(ready, play)) {
                readyAt[resolved] = ready;
            }
        }
        const std::scoped_lock lock{ g_cooldownLock };
        g_playSeconds = play;
        g_readyAt = std::move(readyAt);
    }

    namespace {
        namespace Logic = Enchant::Logic;

        constexpr float kPhoenixFlashSeconds = 2.0f;

        // Inside a task the actor may be gone; a FormID lookup re-validates it.
        RE::Actor* LiveActor(RE::FormID a_id) {
            auto* actor = a_id ? RE::TESForm::LookupByID<RE::Actor>(a_id) : nullptr;
            return actor && !actor->IsDeleted() && !actor->IsDisabled() ? actor : nullptr;
        }

        // The carrier's magnitude on a_target, already scaled by the enchanting table. With a
        // caster, only that caster's effects count; several items still count once (strongest).
        std::optional<float> CarrierMagnitude(RE::Actor* a_target, const RE::EffectSetting* a_carrier,
                                              const RE::Actor* a_caster) {
            // Through AsMagicTarget(): the MagicTarget base moved in 1.6.629, so an implicit
            // Actor -> MagicTarget conversion reads the wrong vtable there. On VR the list is a
            // per-thread snapshot, so it is walked at once and not kept.
            auto* target = a_carrier ? a_target->AsMagicTarget() : nullptr;
            auto* list = target ? target->GetActiveEffectList() : nullptr;
            if (!list) {
                return std::nullopt;
            }
            std::vector<float> found;
            for (auto* effect : *list) {
                if (!effect || effect->GetBaseObject() != a_carrier ||
                    effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
                    continue;
                }
                if (a_caster && effect->caster.get().get() != a_caster) {
                    continue;
                }
                found.push_back(effect->magnitude);
            }
            return Logic::StrongestMagnitude(found);
        }

        void Stormchain(RE::FormID a_attacker, RE::FormID a_target) {
            auto* attacker = LiveActor(a_attacker);
            auto* target = LiveActor(a_target);
            if (!attacker || !target || !g_stormchainJump) {
                return;
            }
            const auto magnitude = CarrierMagnitude(target, g_stormchainCarrier, attacker);
            auto*      caster = target->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
            if (!magnitude || !caster) {
                return;
            }
            std::vector<RE::Actor*>       actors;
            std::vector<Logic::Candidate> candidates;
            const auto                    origin = target->GetPosition();
            const auto add = [&](RE::Actor* a_actor) {
                if (!a_actor || a_actor->IsDeleted() || a_actor->IsDisabled()) {
                    return;
                }
                actors.push_back(a_actor);
                candidates.push_back({ origin.GetDistance(a_actor->GetPosition()), attacker->IsHostileToActor(a_actor),
                                       !a_actor->IsDead(), a_actor == attacker, a_actor == target });
            };
            // Raw pointers outlive the handle's NiPointer here; safe only because this whole
            // task runs on the main thread, where nothing unloads an actor under us.
            if (const auto* lists = RE::ProcessLists::GetSingleton()) {
                for (const auto& handle : lists->highActorHandles) {
                    add(handle.get().get());
                }
            }
            add(RE::PlayerCharacter::GetSingleton());   // not in the high list; can be hostile to an NPC wielder
            // The jump spell holds no carrier, so a jump never starts another chain.
            for (const auto i : Logic::PickChainTargets(candidates, Logic::kStormchainMaxJumps, Logic::kStormchainRange)) {
                caster->CastSpellImmediate(g_stormchainJump, false, actors[i], 1.0f, false, *magnitude, attacker);
            }
        }

        void Breach(RE::FormID a_attacker, RE::FormID a_target) {
            auto* attacker = LiveActor(a_attacker);
            auto* target = LiveActor(a_target);
            if (!attacker || !target) {
                return;
            }
            const auto magnitude = CarrierMagnitude(target, g_breachCarrier, attacker);
            if (!magnitude) {
                return;
            }
            // Final health damage with the attacker as source (kill credit, aggro, crime). Armor
            // and resistances are applied before this point in the engine, so they are skipped;
            // true also skips the difficulty multiplier.
            target->DoDamage(*magnitude, attacker, true);
            if (g_breachArt) {
                target->ApplyArtObject(g_breachArt, 1.0f);
            }
        }

        // One path for both triggers (a hit, the 1 Hz watch); main thread only, actor already live.
        void PhoenixRescue(RE::Actor* a_actor) {
            auto*      av = a_actor->AsActorValueOwner();
            const auto percent = av ? CarrierMagnitude(a_actor, g_phoenixCarrier, nullptr) : std::nullopt;
            if (!percent) {
                return;
            }
            const auto  id = a_actor->GetFormID();
            const float maxHealth = av->GetPermanentActorValue(RE::ActorValue::kHealth);
            if (!(maxHealth > 0.0f) ||
                !Logic::PhoenixShouldFire(av->GetActorValue(RE::ActorValue::kHealth) / maxHealth, PlaySeconds(),
                                          PhoenixReadyAt(id))) {
                return;
            }
            av->RestoreActorValue(RE::ActorValue::kHealth, maxHealth * *percent / 100.0f);
            PhoenixStartCooldown(id);
            if (g_phoenixShader) {
                a_actor->ApplyEffectShader(g_phoenixShader, kPhoenixFlashSeconds);
            }
            logger::info("Enchantments: Phoenix Heart rescued {:08X} (+{}% health)", id, *percent);
        }

        // Damage over time, falls and Breach lower health without a hit event, so the clock
        // also checks every loaded wearer once a second (the spec's "watch each wearer").
        void PhoenixWatch() {
            if (!g_phoenixCarrier) {
                return;
            }
            const auto check = [](RE::Actor* a_actor) {
                if (a_actor && !a_actor->IsDeleted() && !a_actor->IsDisabled()) {
                    PhoenixRescue(a_actor);
                }
            };
            if (const auto* lists = RE::ProcessLists::GetSingleton()) {
                for (const auto& handle : lists->highActorHandles) {
                    check(handle.get().get());   // main thread: the actor stays loaded for this call
                }
            }
            check(RE::PlayerCharacter::GetSingleton());
        }

        // Sinks may run off the main thread, so they only queue. Nothing thrown may reach the
        // game's event source or its task queue.
        template <class F>
        void OnMainThread(const char* a_what, F a_work) noexcept {
            try {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([a_what, a_work]() {
                        try {
                            a_work();
                        } catch (...) {
                            logger::error("Enchantments: {} failed", a_what);
                        }
                    });
                }
            } catch (...) {
            }
        }

        RE::FormID IdOf(const RE::TESForm* a_form) { return a_form ? a_form->GetFormID() : 0; }

        class EffectWatcher : public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>,
                              public RE::BSTEventSink<RE::TESHitEvent> {
        public:
            static EffectWatcher* GetSingleton() {
                static EffectWatcher singleton;
                return std::addressof(singleton);
            }

            // The engine applies a carrier only while the weapon has charge: no charge check here.
            // Deferred one task, so the carrier's active effect (and its magnitude) exists by then.
            RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event,
                                                  RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override {
                const auto* target = a_event && a_event->target ? a_event->target->As<RE::Actor>() : nullptr;
                const auto* attacker = a_event && a_event->caster ? a_event->caster->As<RE::Actor>() : nullptr;
                if (!target || !attacker || a_event->magicEffect == 0) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const auto attackerID = attacker->GetFormID();
                const auto targetID = target->GetFormID();
                if (a_event->magicEffect == IdOf(g_stormchainCarrier)) {
                    OnMainThread("Stormchain", [attackerID, targetID]() { Stormchain(attackerID, targetID); });
                } else if (a_event->magicEffect == IdOf(g_breachCarrier)) {
                    OnMainThread("System Breach", [attackerID, targetID]() { Breach(attackerID, targetID); });
                }
                return RE::BSEventNotifyControl::kContinue;
            }

            RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event,
                                                  RE::BSTEventSource<RE::TESHitEvent>*) override {
                const auto* hit = a_event && a_event->target ? a_event->target->As<RE::Actor>() : nullptr;
                if (hit && g_phoenixCarrier) {
                    const auto id = hit->GetFormID();
                    OnMainThread("Phoenix Heart", [id]() {
                        if (auto* actor = LiveActor(id)) {
                            PhoenixRescue(actor);
                        }
                    });
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    void Install() {
        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (!holder) {
            logger::error("Enchantments: no event source holder - Stormchain, System Breach and Phoenix Heart stay inert");
            return;
        }
        holder->AddEventSink<RE::TESMagicEffectApplyEvent>(EffectWatcher::GetSingleton());
        holder->AddEventSink<RE::TESHitEvent>(EffectWatcher::GetSingleton());
        logger::info("Enchantments: watching hits for Stormchain, System Breach and Phoenix Heart");
    }
}
