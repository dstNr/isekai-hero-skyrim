#include "CraftHooks.h"

#include "Storage.h"

#include <MinHook.h>

#include <algorithm>

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
