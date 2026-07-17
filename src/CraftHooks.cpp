#include "CraftHooks.h"

#include "Storage.h"

#include <MinHook.h>

#include <algorithm>

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

        bool s_itemActive = false;     // Hook 3 + Hook 4 live (item crafting)
        bool s_alchemyActive = false;  // Hook 1 + Hook 2 live (alchemy iteration)

        // Boundary between the player's own stacks and the chest's, cached at the start
        // of each list iteration. Main-thread only (the crafting menu runs there).
        std::int32_t g_playerBoundary = 0;
        std::int32_t g_chestStacks = 0;

        [[nodiscard]] RE::TESFurniture::WorkBenchData::BenchType CurrentBench() {
            using BT = RE::TESFurniture::WorkBenchData::BenchType;
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return BT::kNone;
            }
            auto* ref = player->GetOccupiedFurniture().get().get();
            auto* base = ref ? ref->GetBaseObject() : nullptr;
            auto* furn = base ? base->As<RE::TESFurniture>() : nullptr;
            return furn ? furn->workBenchData.benchType.get() : BT::kNone;
        }

        // Item-crafting station (ConstructibleObjectMenu family) — count + consume.
        [[nodiscard]] bool AtItemStation() {
            using BT = RE::TESFurniture::WorkBenchData::BenchType;
            switch (CurrentBench()) {
            case BT::kCreateObject:
            case BT::kSmithingWeapon:
            case BT::kSmithingArmor:
                return true;
            default:
                return false;
            }
        }

        // Alchemy station — the iteration menu we now feed from the chest.
        [[nodiscard]] bool AtAlchemyStation() {
            using BT = RE::TESFurniture::WorkBenchData::BenchType;
            const auto b = CurrentBench();
            return b == BT::kAlchemy || b == BT::kAlchemyExperiment;
        }

        // Where the chest backs the recipe: count-augment and consume redirect apply.
        // Alchemy only counts once its iteration hooks are live (else the shuttle owns it).
        [[nodiscard]] bool AtChestBackedStation() {
            return AtItemStation() || (AtAlchemyStation() && s_alchemyActive);
        }

        // --- Hook 3: per-item count (recipe availability) ---
        std::int32_t Hook_GetInventoryItemCount(RE::InventoryChanges* a_inv,
                                                RE::TESBoundObject* a_item, void* a_filter) {
            const std::int32_t original = _originalGetInventoryItemCount(a_inv, a_item, a_filter);
            if (!a_item || !AtChestBackedStation()) {
                return original;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || a_inv != player->GetInventoryChanges()) {
                return original;
            }
            return original + Storage::ChestCount(a_item);
        }

        // --- Hook 1: total stack count of a container ---
        std::int32_t Hook_GetContainerItemCount(RE::TESObjectREFR* a_ref, bool a_useMerchant,
                                                bool a_unk) {
            const std::int32_t original = _originalGetContainerItemCount(a_ref, a_useMerchant, a_unk);
            if (!a_ref || !a_ref->IsPlayerRef() || !AtAlchemyStation()) {
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
            if (!a_ref || !a_ref->IsPlayerRef() || !AtAlchemyStation()) {
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
                a_reason == RE::ITEM_REMOVE_REASON::kRemove && AtChestBackedStation()) {
                const std::int32_t fromChest = Storage::RemoveFromChest(a_item, a_count);
                const std::int32_t remainder = a_count - fromChest;
                if (fromChest > 0) {
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

    }  // namespace

    bool ItemCraftingHooksActive() {
        return s_itemActive;
    }

    bool AlchemyHooksActive() {
        return s_alchemyActive;
    }

    void Install() {
        if (s_itemActive || s_alchemyActive) {
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
        s_alchemyActive = s_itemActive && h1ok && h2ok;

        logger::info("CraftHooks: item-crafting {} (count @ {:X}, consume {}), alchemy {}",
                     s_itemActive ? "LIVE" : "OFF", countAddr, removeOk ? "ok" : "FAILED",
                     s_alchemyActive ? "LIVE" : "OFF");
    }
}
