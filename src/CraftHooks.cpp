#include "CraftHooks.h"

#include "Storage.h"

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace Isekai::CraftHooks {

    namespace {

        // ----------------------------------------------------------------------------
        // Zero-transfer crafting: crafting stations read AND consume the Dimensional
        // Storage in place — nothing moves, so no VM event flood and no timing race.
        //
        //   * Item crafting (forge/smelter/tanning/grindstone/armour bench) asks a
        //     per-recipe COUNT and consumes on craft → Hook 3 + Hook 4.
        //   * Alchemy ITERATES the inventory to build its ingredient list → Hook 1 +
        //     Hook 2, plus Hook 3/4 for the count/consume paths.
        //   * Enchanting also iterates but consumes soul gems (with fill state) — kept
        //     on the old shuttle for now; its careful hook comes next.
        //
        // Technique, IDs and the RemoveItem vtable slot adapted from SCIE by ohfor
        // (MIT): https://github.com/ohfor/scie
        // ----------------------------------------------------------------------------

        // Hook 3 — GetInventoryItemCount (standalone). IDs 15869 (SE) / 16109 (AE).
        using GetInventoryItemCount_t =
            std::int32_t (*)(RE::InventoryChanges* a_inv, RE::TESBoundObject* a_item, void* a_filter);
        GetInventoryItemCount_t _originalGetInventoryItemCount = nullptr;

        // Hook 1 — GetContainerItemCount. IDs 19274 (SE) / 19700 (AE).
        using GetContainerItemCount_t = std::int32_t (*)(RE::TESObjectREFR* a_ref,
                                                         bool a_useMerchant, bool a_unk);
        GetContainerItemCount_t _originalGetContainerItemCount = nullptr;

        // Hook 2 — GetInventoryItemEntryAtIdx. IDs 19273 (SE) / 19699 (AE).
        using GetInventoryItemEntryAtIdx_t =
            RE::InventoryEntryData* (*)(RE::TESObjectREFR* a_ref, std::int32_t a_idx, bool a_useMerchant);
        GetInventoryItemEntryAtIdx_t _originalGetInventoryItemEntryAtIdx = nullptr;

        // Hook 4 — TESObjectREFR::RemoveItem, vtable index 0x56. Returns ObjectRefHandle
        // by value → hidden result pointer as the second arg (x64 MSVC ABI).
        using RemoveItem_t = RE::ObjectRefHandle* (*)(RE::TESObjectREFR* a_this,
                                                      RE::ObjectRefHandle* a_result,
                                                      RE::TESBoundObject* a_item, std::int32_t a_count,
                                                      RE::ITEM_REMOVE_REASON a_reason,
                                                      RE::ExtraDataList* a_extraList,
                                                      RE::TESObjectREFR* a_moveToRef,
                                                      const RE::NiPoint3* a_dropLoc,
                                                      const RE::NiPoint3* a_rotate);
        RemoveItem_t _originalRemoveItem = nullptr;

        // Hook 5 — PlayerCharacter::GetItemCount. IDs 19275 (SE) / 19701 (AE). This is
        // the count Papyrus scripts hit (Actor.GetItemCount), so other mods' pre-craft
        // prompts — e.g. "empower this enchantment with a flawless gem?" — read it. It's
        // a DIFFERENT function from the menu's count (Hook 3), so without its own hook
        // those scripts see only the player's carried stock, never the chest.
        using PlayerGetItemCount_t = std::int32_t (*)(RE::PlayerCharacter* a_this,
                                                      RE::TESBoundObject* a_obj);
        PlayerGetItemCount_t _originalPlayerGetItemCount = nullptr;

        // Hook 6 — InventoryChanges::GetItemCount (the MEMBER function). IDs 15868 (SE) /
        // 16047 (AE). This is the one that matters most: recipe CONDITIONS (CCOR's
        // "hide recipes you lack the material for") and Papyrus GetItemCount read THIS,
        // not the standalone Hook 3 uses for the menu display. Without it, the chest
        // shows in the material readout but the recipe stays hidden until you carry one.
        // Returns int16.
        using InvChangesGetItemCount_t = std::int16_t (*)(RE::InventoryChanges* a_this,
                                                          RE::TESBoundObject* a_obj);
        InvChangesGetItemCount_t _originalInvChangesGetItemCount = nullptr;

        // All the count hooks share this re-entrancy depth. Any of them may internally
        // call another (e.g. the standalone calls the member); only the OUTERMOST call
        // adds the chest, so it can never be counted twice. Per-thread because Papyrus
        // may run a count off the main thread.
        thread_local int s_countDepth = 0;

        bool s_itemActive = false;     // Hook 3 + Hook 4 live (item crafting)
        bool s_iterActive = false;  // Hook 1 + Hook 2 live (alchemy + enchanting iteration)

        // --- Crafting session state, thread-safe by construction ---
        // The count functions (Hook 3/5/6) fire on several threads, and the member one is
        // extremely hot. So the hooks touch NO game objects — only the atomics and the
        // locked cache below. Every game-object read (menu state, the player's inventory
        // changes, the chest contents) happens once, on the main thread, in the event sinks
        // that open/close a crafting session. That is what stops the crashes AND keeps the
        // augmentation working regardless of which thread a count runs on.
        std::atomic<bool>                  g_menuOpen{ false };       // a CraftingMenu is open
        std::atomic<bool>                  g_craftContext{ false };   // a station was just activated
        std::atomic<long long>             g_contextExpiryMs{ 0 };    // backstop for an aborted activate
        std::atomic<RE::InventoryChanges*> g_playerInvChanges{ nullptr };  // captured on the main thread

        std::mutex                                            g_cacheMutex;
        std::unordered_map<RE::TESBoundObject*, std::int32_t> g_chestCache;  // guarded by g_cacheMutex

        [[nodiscard]] long long NowMs() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                .count();
        }

        // In a crafting interaction? Atomics only, so it is safe from any thread. The menu
        // flag covers the whole menu (incl. the recipe-list build where CCOR's item-count
        // conditions decide visibility); the activate context covers the window BEFORE the
        // menu opens, where some mods pop a prompt ("empower with a flawless gem?").
        [[nodiscard]] bool InCraftContext() {
            if (g_menuOpen.load(std::memory_order_relaxed)) {
                return true;
            }
            return g_craftContext.load(std::memory_order_relaxed) &&
                   NowMs() < g_contextExpiryMs.load(std::memory_order_relaxed);
        }

        // Snapshot the player's inventory-changes pointer and the chest's counts. MAIN
        // THREAD ONLY (event sinks) — the one place we read game objects for crafting.
        void OpenCraftSession() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            g_playerInvChanges.store(player ? player->GetInventoryChanges() : nullptr,
                                     std::memory_order_relaxed);
            std::scoped_lock lock(g_cacheMutex);
            g_chestCache.clear();
            if (auto* chest = Storage::ChestRef()) {
                for (const auto& [obj, cnt] : chest->GetInventoryCounts()) {
                    if (obj && cnt > 0) {
                        g_chestCache[obj] = cnt;
                    }
                }
            }
        }

        void CloseCraftSession() {
            std::scoped_lock lock(g_cacheMutex);
            g_chestCache.clear();
        }

        // Chest count of an item — a locked map read, no game objects, safe from any thread.
        [[nodiscard]] std::int32_t ChestCountCached(RE::TESBoundObject* a_obj) {
            std::scoped_lock lock(g_cacheMutex);
            const auto it = g_chestCache.find(a_obj);
            return it != g_chestCache.end() ? it->second : 0;
        }

        // Keep the cache in step with a chest consume, again without touching game objects.
        void DecrementCache(RE::TESBoundObject* a_obj, std::int32_t a_n) {
            std::scoped_lock lock(g_cacheMutex);
            const auto it = g_chestCache.find(a_obj);
            if (it != g_chestCache.end()) {
                it->second -= a_n;
                if (it->second <= 0) {
                    g_chestCache.erase(it);
                }
            }
        }

        // Boundary between the player's own stacks and the chest's, cached at the start
        // of each list iteration. Main-thread only (the crafting menu runs there).
        std::int32_t g_playerBoundary = 0;
        std::int32_t g_chestStacks = 0;

        // Shared tail for the per-item count hooks: add the chest, but only at the outermost
        // count call (so a nested standalone→member chain can't double it), only while
        // crafting, and only for the player's own inventory changes — compared against the
        // pointer we cached on the main thread, so this stays a plain pointer test with no
        // game-object access.
        [[nodiscard]] bool ShouldAddChest(RE::InventoryChanges* a_inv) {
            return s_countDepth == 0 && a_inv &&
                   a_inv == g_playerInvChanges.load(std::memory_order_relaxed) && InCraftContext();
        }

        // --- Hook 3: standalone GetInventoryItemCount — the menu's material readout ---
        std::int32_t Hook_GetInventoryItemCount(RE::InventoryChanges* a_inv,
                                                RE::TESBoundObject* a_item, void* a_filter) {
            ++s_countDepth;
            const std::int32_t original = _originalGetInventoryItemCount(a_inv, a_item, a_filter);
            --s_countDepth;
            if (!a_item || !ShouldAddChest(a_inv)) {
                return original;
            }
            return original + ChestCountCached(a_item);
        }

        // --- Hook 6: InventoryChanges::GetItemCount (member) — recipe conditions + Papyrus ---
        std::int16_t Hook_InvChangesGetItemCount(RE::InventoryChanges* a_this,
                                                 RE::TESBoundObject* a_obj) {
            ++s_countDepth;
            const std::int16_t original = _originalInvChangesGetItemCount(a_this, a_obj);
            --s_countDepth;
            if (!a_obj || !ShouldAddChest(a_this)) {
                return original;
            }
            const std::int32_t total = static_cast<std::int32_t>(original) + ChestCountCached(a_obj);
            return static_cast<std::int16_t>(std::min(total, 32767));
        }

        // --- Hook 5: PlayerCharacter::GetItemCount (belt-and-braces; rarely the path) ---
        std::int32_t Hook_PlayerGetItemCount(RE::PlayerCharacter* a_this, RE::TESBoundObject* a_obj) {
            ++s_countDepth;
            const std::int32_t original = _originalPlayerGetItemCount(a_this, a_obj);
            --s_countDepth;
            if (!a_obj || s_countDepth != 0 || !InCraftContext() ||
                a_this != RE::PlayerCharacter::GetSingleton()) {
                return original;  // GetSingleton() is a plain global read, safe off-thread
            }
            return original + ChestCountCached(a_obj);
        }

        // --- Hook 1: total stack count of a container ---
        std::int32_t Hook_GetContainerItemCount(RE::TESObjectREFR* a_ref, bool a_useMerchant,
                                                bool a_unk) {
            const std::int32_t original = _originalGetContainerItemCount(a_ref, a_useMerchant, a_unk);
            if (!a_ref || !a_ref->IsPlayerRef() || !InCraftContext()) {
                return original;
            }
            auto* chest = Storage::ChestRef();
            if (!chest) {
                return original;
            }
            // The menu will iterate player + chest stacks; report the combined total.
            return original + _originalGetContainerItemCount(chest, a_useMerchant, a_unk);
        }

        // --- Hook 2: the entry at a given index (list iteration) ---
        RE::InventoryEntryData* Hook_GetInventoryItemEntryAtIdx(RE::TESObjectREFR* a_ref,
                                                               std::int32_t a_idx,
                                                               bool a_useMerchant) {
            if (!a_ref || !a_ref->IsPlayerRef() || !InCraftContext()) {
                return _originalGetInventoryItemEntryAtIdx(a_ref, a_idx, a_useMerchant);
            }
            auto* chest = Storage::ChestRef();
            if (!chest) {
                return _originalGetInventoryItemEntryAtIdx(a_ref, a_idx, a_useMerchant);
            }

            // Re-read the boundary at the start of each iteration. The menu always walks
            // from index 0, and it re-walks after every craft — so this stays correct as
            // the chest's stack count shrinks, without a separate refresh flag.
            if (a_idx <= 0) {
                g_playerBoundary = _originalGetContainerItemCount(a_ref, false, true);
                g_chestStacks = _originalGetContainerItemCount(chest, false, true);
            }

            if (a_idx < g_playerBoundary) {
                return _originalGetInventoryItemEntryAtIdx(a_ref, a_idx, a_useMerchant);  // player's own
            }

            const std::int32_t localIdx = a_idx - g_playerBoundary;
            if (localIdx < 0 || localIdx >= g_chestStacks) {
                return nullptr;  // past the end — stale/oversized query, don't touch the engine
            }

            auto* entry = _originalGetInventoryItemEntryAtIdx(chest, localIdx, false);
            // Guard against null / sentinel pointers before dereferencing (SCIE crash sig).
            if (!entry || reinterpret_cast<std::uintptr_t>(entry) < 0x10000 || !entry->object) {
                return nullptr;
            }
            return entry;  // the menu keeps only the form types it wants (ingredients here)
        }

        // --- Hook 4: consume, pulling the chest first ---
        RE::ObjectRefHandle* Hook_RemoveItem(RE::TESObjectREFR* a_this, RE::ObjectRefHandle* a_result,
                                             RE::TESBoundObject* a_item, std::int32_t a_count,
                                             RE::ITEM_REMOVE_REASON a_reason,
                                             RE::ExtraDataList* a_extraList,
                                             RE::TESObjectREFR* a_moveToRef,
                                             const RE::NiPoint3* a_dropLoc,
                                             const RE::NiPoint3* a_rotate) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && a_this == player && a_item && a_count > 0 &&
                a_reason == RE::ITEM_REMOVE_REASON::kRemove && InCraftContext()) {
                const std::int32_t fromChest = Storage::RemoveFromChest(a_item, a_count);
                const std::int32_t remainder = a_count - fromChest;
                if (fromChest > 0) {
                    DecrementCache(a_item, fromChest);  // keep the display in step
                    logger::info("CraftHooks: consumed {}x '{}' from storage, {} from player",
                                 fromChest, a_item->GetName(), remainder);
                }
                if (remainder <= 0) {
                    return a_result;  // fully covered by the chest — leave the player alone
                }
                return _originalRemoveItem(a_this, a_result, a_item, remainder, a_reason,
                                           a_extraList, a_moveToRef, a_dropLoc, a_rotate);
            }
            return _originalRemoveItem(a_this, a_result, a_item, a_count, a_reason, a_extraList,
                                       a_moveToRef, a_dropLoc, a_rotate);
        }

        // Opens the crafting context the instant a crafting station is activated — before
        // any mod's pre-menu prompt runs. Closed again on crafting-menu close (below).
        class ActivateWatcher : public RE::BSTEventSink<RE::TESActivateEvent> {
        public:
            static ActivateWatcher* GetSingleton() {
                static ActivateWatcher singleton;
                return std::addressof(singleton);
            }
            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESActivateEvent* a_event,
                RE::BSTEventSource<RE::TESActivateEvent>*) override {
                if (a_event && a_event->actionRef && a_event->actionRef->IsPlayerRef() &&
                    a_event->objectActivated) {
                    auto* base = a_event->objectActivated->GetBaseObject();
                    auto* furn = base ? base->As<RE::TESFurniture>() : nullptr;
                    if (furn && furn->workBenchData.benchType.get() !=
                                    RE::TESFurniture::WorkBenchData::BenchType::kNone) {
                        // Main thread (game event) — safe to snapshot the session here, so
                        // even a pre-menu prompt sees the chest.
                        g_craftContext.store(true, std::memory_order_relaxed);
                        g_contextExpiryMs.store(NowMs() + 20000, std::memory_order_relaxed);
                        OpenCraftSession();
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            ActivateWatcher() = default;
        };

        // Opens the session when the crafting menu opens (belt-and-braces to the activate
        // sink, and the reliable close). Both fire on the main thread.
        class MenuWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            static MenuWatcher* GetSingleton() {
                static MenuWatcher singleton;
                return std::addressof(singleton);
            }
            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (a_event && a_event->menuName == RE::CraftingMenu::MENU_NAME) {
                    if (a_event->opening) {
                        g_menuOpen.store(true, std::memory_order_relaxed);
                        OpenCraftSession();
                    } else {
                        g_menuOpen.store(false, std::memory_order_relaxed);
                        g_craftContext.store(false, std::memory_order_relaxed);
                        CloseCraftSession();
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            MenuWatcher() = default;
        };

    }  // namespace

    bool ItemCraftingHooksActive() {
        return s_itemActive;
    }

    bool IterationHooksActive() {
        return s_iterActive;
    }

    void Install() {
        if (s_itemActive || s_iterActive) {
            return;
        }

        // Resolve the count function through Address Library first. A missing/mismatched
        // library throws or returns 0 — bail loudly rather than patch a wrong address.
        std::uintptr_t countAddr = 0;
        try {
            const REL::Relocation<std::uintptr_t> target{ REL::VariantID(15869, 16109, 0x1f7ed0) };
            countAddr = target.address();
        } catch (const std::exception& e) {
            logger::error("CraftHooks: Address Library lookup failed ({}) — hooks disabled", e.what());
            return;
        } catch (...) {
            logger::error("CraftHooks: Address Library lookup failed — hooks disabled");
            return;
        }
        if (countAddr == 0) {
            logger::error("CraftHooks: count function resolved to address 0 — hooks disabled");
            return;
        }

        if (MH_Initialize() != MH_OK) {
            logger::error("CraftHooks: MH_Initialize failed — hooks disabled");
            return;
        }

        bool countOk = MH_CreateHook(reinterpret_cast<void*>(countAddr),
                                     reinterpret_cast<void*>(&Hook_GetInventoryItemCount),
                                     reinterpret_cast<void**>(&_originalGetInventoryItemCount)) == MH_OK;
        if (!countOk) {
            logger::error("CraftHooks: MH_CreateHook(GetInventoryItemCount) failed — hooks disabled");
            MH_Uninitialize();
            return;
        }

        // Iteration hooks for alchemy (best-effort; a miss just leaves the shuttle on).
        bool h1ok = false;
        bool h2ok = false;
        try {
            REL::Relocation<std::uintptr_t> h1{ REL::VariantID(19274, 19700, 0x29f980) };
            h1ok = MH_CreateHook(reinterpret_cast<void*>(h1.address()),
                                 reinterpret_cast<void*>(&Hook_GetContainerItemCount),
                                 reinterpret_cast<void**>(&_originalGetContainerItemCount)) == MH_OK;
            REL::Relocation<std::uintptr_t> h2{ REL::VariantID(19273, 19699, 0x29f910) };
            h2ok = MH_CreateHook(reinterpret_cast<void*>(h2.address()),
                                 reinterpret_cast<void*>(&Hook_GetInventoryItemEntryAtIdx),
                                 reinterpret_cast<void**>(&_originalGetInventoryItemEntryAtIdx)) == MH_OK;
        } catch (...) {
            logger::warn("CraftHooks: iteration hook lookup failed — alchemy stays on the shuttle");
        }

        // Script-side count hook (best-effort): so other mods' Papyrus prompts see the
        // chest at a station too. A miss only means such prompts miss the chest.
        try {
            REL::Relocation<std::uintptr_t> pg{ REL::RelocationID(19275, 19701) };
            if (MH_CreateHook(reinterpret_cast<void*>(pg.address()),
                              reinterpret_cast<void*>(&Hook_PlayerGetItemCount),
                              reinterpret_cast<void**>(&_originalPlayerGetItemCount)) != MH_OK) {
                logger::warn("CraftHooks: MH_CreateHook(PlayerCharacter::GetItemCount) failed");
            }
        } catch (...) {
            logger::warn("CraftHooks: PlayerCharacter::GetItemCount lookup failed — script counts unhooked");
        }

        // Member count hook — THE one recipe conditions and Papyrus actually read. Without
        // it, condition-gated recipes (CCOR) stay hidden and pre-craft prompts miss the
        // chest even though the menu display shows it.
        bool memberOk = false;
        try {
            REL::Relocation<std::uintptr_t> m{ REL::RelocationID(15868, 16047) };
            memberOk = MH_CreateHook(reinterpret_cast<void*>(m.address()),
                                     reinterpret_cast<void*>(&Hook_InvChangesGetItemCount),
                                     reinterpret_cast<void**>(&_originalInvChangesGetItemCount)) == MH_OK;
            if (!memberOk) {
                logger::warn("CraftHooks: MH_CreateHook(InventoryChanges::GetItemCount) failed");
            }
        } catch (...) {
            logger::warn("CraftHooks: InventoryChanges::GetItemCount lookup failed");
        }

        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
            logger::error("CraftHooks: MH_EnableHook failed — hooks disabled");
            return;
        }

        // Consume hook — vtable swap on PlayerCharacter's RemoveItem (version-stable).
        bool removeOk = false;
        try {
            REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
            _originalRemoveItem =
                reinterpret_cast<RemoveItem_t>(vtbl.write_vfunc(0x56, &Hook_RemoveItem));
            removeOk = _originalRemoveItem != nullptr;
        } catch (const std::exception& e) {
            logger::error("CraftHooks: RemoveItem vtable hook failed ({})", e.what());
        }

        // Item crafting needs count + consume; alchemy additionally needs both iteration
        // hooks. Whatever did not come up stays on the shuttle via Storage's fallback.
        s_itemActive = countOk && removeOk;
        s_iterActive = s_itemActive && h1ok && h2ok;

        // Event sinks that open/close the crafting context (for pre-menu prompts). Only
        // needed when the hooks are live.
        if (s_itemActive) {
            if (auto* src = RE::ScriptEventSourceHolder::GetSingleton()) {
                src->AddEventSink<RE::TESActivateEvent>(ActivateWatcher::GetSingleton());
            }
            if (auto* ui = RE::UI::GetSingleton()) {
                ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatcher::GetSingleton());
            }
        }

        logger::info("CraftHooks: item-crafting {} (count @ {:X}, consume {}), iteration {}, member-count {}",
                     s_itemActive ? "LIVE" : "OFF", countAddr, removeOk ? "ok" : "FAILED",
                     s_iterActive ? "LIVE" : "OFF", memberOk ? "ok" : "FAILED");
    }
}
