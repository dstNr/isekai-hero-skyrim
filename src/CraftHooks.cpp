#include "CraftHooks.h"

#include "Storage.h"

#include <MinHook.h>

#include <algorithm>
#include <atomic>

namespace Isekai::CraftHooks {

    namespace {

        // ----------------------------------------------------------------------------
        // Zero-transfer crafting, phase 2a: item-crafting stations (the
        // ConstructibleObjectMenu family — forge, smelter, tanning rack, grindstone,
        // armour bench) read AND consume the Dimensional Storage in place. Nothing
        // moves, so there is no VM event flood and no timing race.
        //
        // Those menus never iterate the player inventory; they ask GetInventoryItemCount
        // per recipe (to grey it) and RemoveItem on craft (to consume). Two intercepts
        // cover them. Alchemy and enchanting DO iterate the inventory and still use the
        // old shuttle — a separate hook path (phase 2b) will replace that.
        //
        // Technique, IDs and the RemoveItem vtable slot adapted from SCIE by ohfor
        // (MIT): https://github.com/ohfor/scie
        // ----------------------------------------------------------------------------

        // Hook 3 — GetInventoryItemCount. Standalone engine function, the count the
        // recipe UI consults. Address Library IDs 15869 (SE) / 16109 (AE); third value
        // is the VR offset.
        using GetInventoryItemCount_t =
            std::int32_t (*)(RE::InventoryChanges* a_inv, RE::TESBoundObject* a_item, void* a_filter);
        GetInventoryItemCount_t _originalGetInventoryItemCount = nullptr;

        // Hook 4 — TESObjectREFR::RemoveItem, vtable index 0x56. RemoveItem returns an
        // ObjectRefHandle by value; under the x64 MSVC ABI that becomes a hidden result
        // pointer as the second argument.
        using RemoveItem_t = RE::ObjectRefHandle* (*)(RE::TESObjectREFR* a_this,
                                                      RE::ObjectRefHandle* a_result,
                                                      RE::TESBoundObject* a_item, std::int32_t a_count,
                                                      RE::ITEM_REMOVE_REASON a_reason,
                                                      RE::ExtraDataList* a_extraList,
                                                      RE::TESObjectREFR* a_moveToRef,
                                                      const RE::NiPoint3* a_dropLoc,
                                                      const RE::NiPoint3* a_rotate);
        RemoveItem_t _originalRemoveItem = nullptr;

        // --- Phase 2b, VALIDATION ONLY (pass-through) ---
        // Alchemy and enchanting menus ITERATE the inventory instead of asking for a
        // per-item count, so they use two more engine functions. Before we implement the
        // real index remapping + dedup, these hooks just observe the iteration pattern on
        // the live version. IDs and signatures from SCIE (MIT).

        // Hook 1 — GetContainerItemCount. IDs 19274 (SE) / 19700 (AE).
        using GetContainerItemCount_t = std::int32_t (*)(RE::TESObjectREFR* a_ref,
                                                         bool a_useMerchant, bool a_unk);
        GetContainerItemCount_t _originalGetContainerItemCount = nullptr;

        // Hook 2 — GetInventoryItemEntryAtIdx. IDs 19273 (SE) / 19699 (AE).
        using GetInventoryItemEntryAtIdx_t =
            RE::InventoryEntryData* (*)(RE::TESObjectREFR* a_ref, std::int32_t a_idx, bool a_useMerchant);
        GetInventoryItemEntryAtIdx_t _originalGetInventoryItemEntryAtIdx = nullptr;

        std::atomic<int> s_iterLog{ 0 };

        bool s_active = false;  // both hooks live → Storage disables the shuttle here

        // Is the player at an ITEM-crafting station? Only there do we augment counts and
        // redirect consumption. GetInventoryItemCount fires all over the game; this scopes
        // us to a workbench and keeps quests, barter and normal inventory untouched.
        [[nodiscard]] bool AtHookedStation() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return false;
            }
            auto* ref = player->GetOccupiedFurniture().get().get();
            auto* base = ref ? ref->GetBaseObject() : nullptr;
            auto* furn = base ? base->As<RE::TESFurniture>() : nullptr;
            if (!furn) {
                return false;
            }
            using BT = RE::TESFurniture::WorkBenchData::BenchType;
            switch (furn->workBenchData.benchType.get()) {
            case BT::kCreateObject:     // forge, smelter, tanning rack, cooking, staff enchanter
            case BT::kSmithingWeapon:   // grindstone
            case BT::kSmithingArmor:    // armour workbench
                return true;
            default:                    // kAlchemy / kEnchanting / none → shuttle handles it
                return false;
            }
        }

        // The iteration stations (alchemy, enchanting) — where Phase 2b will augment.
        [[nodiscard]] bool AtIterationStation() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return false;
            }
            auto* ref = player->GetOccupiedFurniture().get().get();
            auto* base = ref ? ref->GetBaseObject() : nullptr;
            auto* furn = base ? base->As<RE::TESFurniture>() : nullptr;
            if (!furn) {
                return false;
            }
            using BT = RE::TESFurniture::WorkBenchData::BenchType;
            switch (furn->workBenchData.benchType.get()) {
            case BT::kAlchemy:
            case BT::kAlchemyExperiment:
            case BT::kEnchanting:
            case BT::kEnchantingExperiment:
                return true;
            default:
                return false;
            }
        }

        std::int32_t Hook_GetInventoryItemCount(RE::InventoryChanges* a_inv,
                                                RE::TESBoundObject* a_item, void* a_filter) {
            const std::int32_t original = _originalGetInventoryItemCount(a_inv, a_item, a_filter);
            if (!a_item || !AtHookedStation()) {
                return original;
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || a_inv != player->GetInventoryChanges()) {
                return original;  // not the player's inventory — leave it alone
            }
            // Count the chest in place. The recipe now sees player + storage; nothing moved.
            return original + Storage::ChestCount(a_item);
        }

        RE::ObjectRefHandle* Hook_RemoveItem(RE::TESObjectREFR* a_this, RE::ObjectRefHandle* a_result,
                                             RE::TESBoundObject* a_item, std::int32_t a_count,
                                             RE::ITEM_REMOVE_REASON a_reason,
                                             RE::ExtraDataList* a_extraList,
                                             RE::TESObjectREFR* a_moveToRef,
                                             const RE::NiPoint3* a_dropLoc,
                                             const RE::NiPoint3* a_rotate) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && a_this == player && a_item && a_count > 0 &&
                a_reason == RE::ITEM_REMOVE_REASON::kRemove && AtHookedStation()) {
                // Spend the storage first, then let the player's own carried stock cover
                // any remainder. The count hook already told the menu we have enough.
                const std::int32_t fromChest = Storage::RemoveFromChest(a_item, a_count);
                const std::int32_t remainder = a_count - fromChest;
                if (fromChest > 0) {
                    // info-level on purpose: one line per craft, and it is the proof that
                    // consumption really pulls from the chest (not a silent dupe).
                    logger::info("CraftHooks: consumed {}x '{}' from storage, {} from player",
                                 fromChest, a_item->GetName(), remainder);
                }
                if (remainder <= 0) {
                    // Fully covered by the chest — do not touch the player's inventory,
                    // and do not re-enter the original with a possibly-consumed extra list.
                    return a_result;
                }
                return _originalRemoveItem(a_this, a_result, a_item, remainder, a_reason,
                                           a_extraList, a_moveToRef, a_dropLoc, a_rotate);
            }
            return _originalRemoveItem(a_this, a_result, a_item, a_count, a_reason, a_extraList,
                                       a_moveToRef, a_dropLoc, a_rotate);
        }

        // Phase 2b validation — pass-through. Observe how alchemy/enchanting iterate the
        // player inventory so the real remapping (next step) uses the true call pattern.
        std::int32_t Hook_GetContainerItemCount(RE::TESObjectREFR* a_ref, bool a_useMerchant,
                                                bool a_unk) {
            const std::int32_t original = _originalGetContainerItemCount(a_ref, a_useMerchant, a_unk);
            if (a_ref && a_ref->IsPlayerRef() && AtIterationStation()) {
                if (const int n = ++s_iterLog; n <= 120) {
                    logger::info("CraftHooks[2b]: GetContainerItemCount(player) = {} stacks", original);
                }
            }
            return original;
        }

        RE::InventoryEntryData* Hook_GetInventoryItemEntryAtIdx(RE::TESObjectREFR* a_ref,
                                                               std::int32_t a_idx, bool a_useMerchant) {
            auto* entry = _originalGetInventoryItemEntryAtIdx(a_ref, a_idx, a_useMerchant);
            if (a_ref && a_ref->IsPlayerRef() && AtIterationStation()) {
                if (const int n = ++s_iterLog; n <= 120) {
                    const char* nm = (entry && reinterpret_cast<std::uintptr_t>(entry) >= 0x10000 &&
                                      entry->object)
                                         ? entry->object->GetName()
                                         : "<none>";
                    logger::info("CraftHooks[2b]: GetInventoryItemEntryAtIdx(player, {}) -> '{}'",
                                 a_idx, nm);
                }
            }
            return entry;
        }

    }  // namespace

    bool ItemCraftingHooksActive() {
        return s_active;
    }

    void Install() {
        if (s_active) {
            return;
        }

        // Resolve the count function through Address Library first. A missing/mismatched
        // library throws or returns 0 — bail loudly rather than patch a wrong address.
        std::uintptr_t addr = 0;
        try {
            const REL::Relocation<std::uintptr_t> target{ REL::VariantID(15869, 16109, 0x1f7ed0) };
            addr = target.address();
        } catch (const std::exception& e) {
            logger::error("CraftHooks: Address Library lookup failed ({}) — item-crafting hooks disabled",
                          e.what());
            return;
        } catch (...) {
            logger::error("CraftHooks: Address Library lookup failed — item-crafting hooks disabled");
            return;
        }
        if (addr == 0) {
            logger::error("CraftHooks: count function resolved to address 0 — hooks disabled");
            return;
        }

        if (MH_Initialize() != MH_OK) {
            logger::error("CraftHooks: MH_Initialize failed — hooks disabled");
            return;
        }
        if (MH_CreateHook(reinterpret_cast<void*>(addr),
                          reinterpret_cast<void*>(&Hook_GetInventoryItemCount),
                          reinterpret_cast<void**>(&_originalGetInventoryItemCount)) != MH_OK) {
            logger::error("CraftHooks: MH_CreateHook(GetInventoryItemCount) failed — hooks disabled");
            MH_Uninitialize();
            return;
        }

        // Phase 2b validation hooks (pass-through). Best-effort: if either fails we log and
        // carry on — they observe only, so a miss costs nothing but the diagnostic.
        try {
            REL::Relocation<std::uintptr_t> h1{ REL::VariantID(19274, 19700, 0x29f980) };
            if (MH_CreateHook(reinterpret_cast<void*>(h1.address()),
                              reinterpret_cast<void*>(&Hook_GetContainerItemCount),
                              reinterpret_cast<void**>(&_originalGetContainerItemCount)) != MH_OK) {
                logger::warn("CraftHooks[2b]: MH_CreateHook(GetContainerItemCount) failed");
            }
            REL::Relocation<std::uintptr_t> h2{ REL::VariantID(19273, 19699, 0x29f910) };
            if (MH_CreateHook(reinterpret_cast<void*>(h2.address()),
                              reinterpret_cast<void*>(&Hook_GetInventoryItemEntryAtIdx),
                              reinterpret_cast<void**>(&_originalGetInventoryItemEntryAtIdx)) != MH_OK) {
                logger::warn("CraftHooks[2b]: MH_CreateHook(GetInventoryItemEntryAtIdx) failed");
            }
        } catch (...) {
            logger::warn("CraftHooks[2b]: validation hook lookup failed — skipping the observers");
        }

        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
            logger::error("CraftHooks: MH_EnableHook failed — hooks disabled");
            return;
        }

        // Consume hook — a vtable swap on PlayerCharacter's RemoveItem. Version-stable
        // (the vtable address itself comes from Address Library, no byte patching).
        try {
            REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
            _originalRemoveItem =
                reinterpret_cast<RemoveItem_t>(vtbl.write_vfunc(0x56, &Hook_RemoveItem));
        } catch (const std::exception& e) {
            logger::error("CraftHooks: RemoveItem vtable hook failed ({}) — count hook stays, "
                          "shuttle keeps consuming", e.what());
            return;  // s_active stays false → shuttle remains the fallback
        }

        s_active = true;
        logger::info("CraftHooks: item-crafting hooks live (count @ {:X}, consume @ vtable 0x56)", addr);
    }
}
