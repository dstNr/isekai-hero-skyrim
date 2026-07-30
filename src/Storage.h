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

    // Bring an existing chest up to the current material set — add-on/DLC materials, any
    // ingredient or filled soul gem type it is missing — without refilling stacks the
    // player has spent or touching the gold. Older saves therefore gain new materials on
    // load without a fresh reincarnation. Called on every load.
    void TopUpStock();

    // Delete leftover storage-chest references from earlier rebuilds. RebuildChest used
    // to only disable the old chest, so a long save can hold several orphaned husks; this
    // finds every reference of our chest base except the current one and removes it.
    // Cleans up save bloat (and "unattached" entries a save cleaner would flag). Called
    // on every load.
    void PruneOrphanChests();

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
