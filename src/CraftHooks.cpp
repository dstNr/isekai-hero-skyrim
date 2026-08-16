#include "CraftHooks.h"

#include "Plugin.h"
#include "Storage.h"

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Isekai::CraftHooks {

    namespace {

        // ----------------------------------------------------------------------------
        // Crafting stations read AND consume the Dimensional Storage in place — the
        // display count and the consume both come from the chest, nothing is shuttled.
        //
        //   * Item crafting (forge/smelter/tanning/grindstone/armour bench) asks a
        //     per-recipe COUNT and consumes on craft → Hook 3 + Hook 4.
        //   * Alchemy ITERATES the inventory to build its ingredient list → Hook 1 +
        //     Hook 2, plus Hook 3/4 for the count/consume paths.
        //
        // ONE thing a count hook cannot do: satisfy a recipe-VISIBILITY condition. CCOR
        // (and kin) hide recipes behind `GetItemCount material >= 1`, which the engine's
        // condition system reads straight off the real inventory — never through any
        // hookable count function (we tried the standalone, the member and the
        // PlayerCharacter one; none is on that path). The only thing that satisfies such
        // a condition is a real item in the inventory. So at a forge-family bench we lend
        // exactly ONE of each stored material the player lacks (LendTokens), the recipe
        // then shows, the count/consume hooks present and spend the FULL chest, and the
        // leftover tokens go back on menu close. One item per type, not stacks — no flood.
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

        // Re-entrancy depth for the standalone count hook: it may internally call the
        // member count, so only the OUTERMOST call adds the chest and it can never be
        // counted twice. Per-thread because a count may run off the main thread.
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

        // --- Recipe-visibility tokens (main thread only, in the event sinks) ---
        using BenchType = RE::TESFurniture::WorkBenchData::BenchType;

        // The one item of each material we lent the player so a foreign `>= 1` visibility
        // condition passes. Raw base-form pointers — never freed, and only ever touched on
        // the main thread, so no lock.
        std::vector<RE::TESBoundObject*> g_lentTokens;

        // Only the forge family gates recipes on carried materials (CCOR). Alchemy and
        // enchanting don't hide recipes that way, and their soul-gem/ingredient stock is
        // served by the iteration path — no material tokens there.
        //
        // Enchanting is included for the OTHER reason tokens exist: a mod's pre-craft
        // Papyrus prompt (Thaumaturgy's "empower this enchantment with a flawless gem?")
        // reads the real inventory via GetItemCount before the menu even opens. Our C++
        // activate sink runs synchronously, ahead of that queued Papyrus fragment, so a
        // gem lent here is already carried when the prompt asks. (Soul gems — the enchant
        // power source — still come from the iteration path, not tokens.)
        [[nodiscard]] bool IsEnchantBench(BenchType a_bench) {
            return a_bench == BenchType::kEnchanting ||
                   a_bench == BenchType::kEnchantingExperiment;
        }
        [[nodiscard]] bool BenchWantsTokens(BenchType a_bench) {
            return a_bench == BenchType::kCreateObject ||
                   a_bench == BenchType::kSmithingWeapon ||
                   a_bench == BenchType::kSmithingArmor || IsEnchantBench(a_bench);
        }

        // Only official-master items are ever tokened. Moving a scripted mod item in and
        // out of the inventory each station visit fires its OnContainerChanged handler —
        // and some (deployable "bear traps", campfire kit, ...) react by spawning more of
        // themselves, flooding the VM into an endless-item stutter. Vanilla + DLC materials
        // carry no such scripts, so those are safe to shuffle. (DLC IS official, so the
        // add-on materials the player asked for — chitin plate, netch leather, corkbulb
        // root — still get tokens.) Check itself lives in Plugin::IsOfficialMaster,
        // shared with Storage/Shop — used to be duplicated identically.
        using Plugin::IsOfficialMaster;

        // Exactly the materials some crafting recipe requires — the components of every
        // BGSConstructibleObject, Misc or Ingredient, restricted to official masters (see
        // IsOfficialMaster). Built once at Install. Tokening only these (rather than every
        // stored ingredient) keeps the lent set to the few dozen things the forge family
        // actually gates on, so the per-open item shuffle stays tiny. Soul gems carry
        // fill-state extra data and are never tokened.
        std::unordered_set<RE::TESBoundObject*> g_recipeMaterials;

        void BuildRecipeMaterialSet() {
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data) {
                return;
            }
            for (auto* cobj : data->GetFormArray<RE::BGSConstructibleObject>()) {
                if (!cobj) {
                    continue;
                }
                cobj->requiredItems.ForEachContainerObject([](RE::ContainerObject& a_c) {
                    if (a_c.obj && IsOfficialMaster(a_c.obj)) {
                        const auto t = a_c.obj->GetFormType();
                        if (t == RE::FormType::Misc || t == RE::FormType::Ingredient) {
                            g_recipeMaterials.insert(a_c.obj);
                        }
                    }
                    return RE::BSContainer::ForEachResult::kContinue;
                });
            }
            logger::info("CraftHooks: tracking {} recipe material type(s) for visibility tokens",
                         g_recipeMaterials.size());
        }

        // Hand every still-held token back to the chest. kStoreInContainer (not kRemove),
        // so Hook 4 lets it pass straight through instead of treating it as a consume.
        //
        // Anything that does NOT make it back stays on the list. This used to clear
        // unconditionally — including when there was no chest to return to at all — and a
        // token dropped from the list is stranded for good, because nothing else in the
        // game knows the item was ever on loan. That is the leak behind "crafting
        // materials turned up in my inventory and never left again": each station visit
        // forgot a few more, and the next visit then skipped them as "already carried",
        // so the lent count fell visit by visit (275 -> 207 -> 127 -> 34 in one session)
        // while the difference sat in the player's pockets.
        //
        // RemoveItem reports nothing at all, so the return is VERIFIED by reading the
        // inventory back rather than assumed.
        void ReturnTokens() {
            if (g_lentTokens.empty()) {
                return;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* chest = Storage::ChestRef();
            if (!player || !chest) {
                logger::warn("CraftHooks: holding {} token(s) — no {} to return them to; "
                             "they stay on the books for the next attempt",
                             g_lentTokens.size(), player ? "storage chest" : "player");
                return;  // keep the list: a lost entry can never be reclaimed
            }

            const auto before = player->GetInventoryCounts();
            int        spent = 0;
            for (auto* obj : g_lentTokens) {
                const auto it = before.find(obj);
                if (it == before.end() || it->second <= 0) {
                    ++spent;  // crafted away while the menu was open — nothing to hand back
                    continue;
                }
                player->RemoveItem(obj, 1, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr,
                                   chest);
            }

            const auto                       after = player->GetInventoryCounts();
            std::vector<RE::TESBoundObject*> stuck;
            for (auto* obj : g_lentTokens) {
                const auto had = before.find(obj);
                if (had == before.end() || had->second <= 0) {
                    continue;  // counted as spent above
                }
                const auto now = after.find(obj);
                if (now != after.end() && now->second >= had->second) {
                    stuck.push_back(obj);  // the move did not take — try again next time
                }
            }

            const auto lent = g_lentTokens.size();
            g_lentTokens = std::move(stuck);
            logger::info("CraftHooks: returned {} of {} token(s) ({} spent crafting{})",
                         lent - g_lentTokens.size() - static_cast<std::size_t>(spent), lent,
                         spent,
                         g_lentTokens.empty()
                             ? ""
                             : ", " + std::to_string(g_lentTokens.size()) + " would not move");
            if (!g_lentTokens.empty()) {
                // Name them: a return that silently fails for a whole class of item (an
                // extra-data stack, a quest-flagged material) is not something a count can
                // point at.
                std::string names;
                for (auto* obj : g_lentTokens) {
                    if (!names.empty()) {
                        names += ", ";
                    }
                    names += obj->GetName();
                }
                logger::warn("CraftHooks: still on loan: {}", names);
            }
        }

        // Lend one of each stored material the player is not already carrying, so a foreign
        // `GetItemCount >= 1` check passes: at a forge, that is every smithing-recipe
        // component (so CCOR shows the recipe); at an enchanter, it is every stored Misc
        // item (so a gem-spending "empower" prompt sees the gems). MUST run before
        // OpenCraftSession snapshots the chest, so the cached counts match the (now
        // one-lower) chest. Main thread only.
        void LendTokens(BenchType a_bench) {
            ReturnTokens();  // sweep up anything stranded by an aborted session first
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* chest = Storage::ChestRef();
            if (!player || !chest) {
                return;
            }
            const bool enchant = IsEnchantBench(a_bench);
            const auto held = player->GetInventoryCounts();
            // GetInventoryCounts hands back a copy, so removing from the chest inside the
            // loop is safe.
            for (const auto& [obj, cnt] : chest->GetInventoryCounts()) {
                if (!obj || cnt <= 0) {
                    continue;
                }
                // Forge: recipe components (already official-only). Enchanter: any official
                // Misc (gems for "empower") — the official filter keeps scripted mod items
                // out of the shuffle here too.
                const bool wanted = enchant ? (obj->GetFormType() == RE::FormType::Misc &&
                                               IsOfficialMaster(obj))
                                            : g_recipeMaterials.contains(obj);
                if (!wanted) {
                    continue;
                }
                if (const auto it = held.find(obj); it != held.end() && it->second > 0) {
                    continue;  // already carrying one — the condition already passes
                }
                // a_this is the chest here, so the player-scoped Hook 4 ignores this move.
                chest->RemoveItem(obj, 1, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr,
                                  player);
                g_lentTokens.push_back(obj);
            }
            if (!g_lentTokens.empty()) {
                logger::info("CraftHooks: lent {} material token(s) for recipe visibility",
                             g_lentTokens.size());
            }
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
                    const auto bench = furn ? furn->workBenchData.benchType.get() : BenchType::kNone;
                    if (bench != BenchType::kNone) {
                        // Main thread (game event) — safe to touch game objects here. This
                        // fires as the bench is activated, BEFORE the menu builds its recipe
                        // list, which is exactly when the visibility tokens have to be in
                        // place.
                        g_craftContext.store(true, std::memory_order_relaxed);
                        g_contextExpiryMs.store(NowMs() + 20000, std::memory_order_relaxed);
                        if (BenchWantsTokens(bench)) {
                            LendTokens(bench);  // must precede the snapshot below
                        }
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
                        ReturnTokens();  // leftover tokens go home; consumed ones stay spent
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

        // Event sinks that open/close the crafting context (for pre-menu prompts) and lend
        // the visibility tokens. Only needed when the hooks are live.
        if (s_itemActive) {
            BuildRecipeMaterialSet();
            if (auto* src = RE::ScriptEventSourceHolder::GetSingleton()) {
                src->AddEventSink<RE::TESActivateEvent>(ActivateWatcher::GetSingleton());
            }
            if (auto* ui = RE::UI::GetSingleton()) {
                ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatcher::GetSingleton());
            }
        }

        logger::info("CraftHooks: item-crafting {} (count @ {:X}, consume {}), iteration {}",
                     s_itemActive ? "LIVE" : "OFF", countAddr, removeOk ? "ok" : "FAILED",
                     s_iterActive ? "LIVE" : "OFF");
    }
}
