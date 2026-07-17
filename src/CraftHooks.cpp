#include "CraftHooks.h"

#include <MinHook.h>

#include <atomic>

namespace Isekai::CraftHooks {

    namespace {

        // Hook target — GetInventoryItemCount. This is the standalone engine function
        // the crafting menu consults to decide whether a recipe is craftable and to
        // show "you have N". Address Library IDs 15869 (SE) / 16109 (AE); the third
        // value is the VR offset. Signature and IDs come from SCIE (MIT). This build
        // hooks it only to VERIFY the interception on the live version — it returns the
        // original result unchanged.
        using GetInventoryItemCount_t =
            std::int32_t (*)(RE::InventoryChanges* a_inv, RE::TESBoundObject* a_item, void* a_filter);

        GetInventoryItemCount_t _originalGetInventoryItemCount = nullptr;

        bool             s_installed = false;
        std::atomic<int> s_logged{ 0 };

        // Is the player currently occupying a crafting station? The count function is
        // called all over the game; we only care about calls made while at a bench.
        [[nodiscard]] bool AtCraftingStation() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            return player && player->GetOccupiedFurniture().native_handle() != 0;
        }

        std::int32_t Hook_GetInventoryItemCount(RE::InventoryChanges* a_inv,
                                                RE::TESBoundObject* a_item, void* a_filter) {
            const std::int32_t original = _originalGetInventoryItemCount(a_inv, a_item, a_filter);

            // VALIDATION ONLY — never change the result. Confirm this is the function the
            // crafting UI asks, that it fires while at a station, and for which items.
            if (a_item && AtCraftingStation()) {
                if (const int n = ++s_logged; n <= 60) {
                    logger::info("CraftHooks[validate]: GetInventoryItemCount('{}') = {} at a station",
                                 a_item->GetName(), original);
                }
            }
            return original;
        }

    }  // namespace

    void Install() {
        if (s_installed) {
            return;
        }

        // Resolve the target through Address Library first. A missing or mismatched
        // Address Library throws or returns 0 — we bail loudly instead of patching a
        // wrong address (which would be a crash, not a misfeature).
        std::uintptr_t addr = 0;
        try {
            const REL::Relocation<std::uintptr_t> target{ REL::VariantID(15869, 16109, 0x1f7ed0) };
            addr = target.address();
        } catch (const std::exception& e) {
            logger::error("CraftHooks: Address Library lookup failed ({}) — hooks disabled", e.what());
            return;
        } catch (...) {
            logger::error("CraftHooks: Address Library lookup failed — hooks disabled");
            return;
        }
        if (addr == 0) {
            logger::error("CraftHooks: target resolved to address 0 — hooks disabled");
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

        s_installed = true;
        logger::info("CraftHooks: validation hook live at {:X} (GetInventoryItemCount, AE 16109)", addr);
    }
}
