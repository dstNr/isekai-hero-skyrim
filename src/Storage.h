#pragma once

namespace Isekai::Storage {

    // Resolve the container out of IsekaiHero.esp and hook the crafting menu
    // (materials in storage are lent to the player while it is open).
    // Call at kDataLoaded.
    void Install();

    // True when the player may use the storage: blessed (HERO/ASCENDED) and the
    // container form resolved. Drives whether the System panel shows the button.
    [[nodiscard]] bool Available();

    // Open the dimensional storage. Main thread only, and not while any menu is up —
    // it is reached through the System status panel, which closes itself first.
    void Open();

    // Stock the storage with crafting materials and gold, scaled by the blessing
    // (HERO x2, ASCENDED x4 on a NORMAL-sized base that nobody ever receives,
    // since NORMAL has no storage). Called once, from the reincarnation.
    void GrantStartingMaterials();

    // Remove non-vanilla ingredients from the chest. Earlier builds stocked the
    // Creation Club ones too, whose tracker scripts made every alchemy visit
    // stutter — chests from those saves keep the problem until cleaned. Called on
    // every load.
    void PruneForeignStock();

    // Add any filled soul gem type the chest is currently missing. Older chests were
    // stocked with only the grand gem; this brings existing saves up to the full set
    // (black included) without re-adding types that are still present. Called on load.
    void TopUpSoulGems();

    // --- Zero-transfer crafting support (used by CraftHooks) ---

    // How many of a_obj the storage chest holds (0 if no chest / not found). Read by
    // the crafting count hook so recipes count the chest without anything moving.
    [[nodiscard]] std::int32_t ChestCount(RE::TESBoundObject* a_obj);

    // Remove up to a_count of a_obj from the chest; returns how many were actually
    // removed. Used by the crafting consume hook to spend chest materials directly.
    std::int32_t RemoveFromChest(RE::TESBoundObject* a_obj, std::int32_t a_count);

    // The storage chest reference, or nullptr if none exists yet. The iteration hooks
    // (alchemy) read its inventory entries directly to append them to the menu list.
    [[nodiscard]] RE::TESObjectREFR* ChestRef();
}
