#pragma once

#include <cstddef>
#include <cstdint>

namespace Isekai::Storage {

    // What a shop material pack fills the chest with. The three categories are exactly
    // the three sweeps the chest was stocked from before it became purchasable, so
    // there is still only one definition of "what counts as a smithing material".
    enum class MaterialCategory {
        kSmithing,    // Misc materials any official-master recipe consumes (+ the gem set)
        kAlchemy,     // every named ingredient from the official masters
        kEnchanting,  // every filled soul gem base form from the official masters
    };

    // Resolve the container out of IsekaiHero.esp and hook the crafting menu
    // (materials in storage are lent to the player while it is open).
    // Call at kDataLoaded.
    void Install();

    // True when the player may use the storage: reincarnated (any blessing — NORMAL
    // gets the same dimension, just empty, as a stash) and the container form resolved.
    // Drives whether the System panel shows the button.
    [[nodiscard]] bool Available();

    // Open the dimensional storage. Main thread only, and not while any menu is up —
    // it is reached through the System status panel, which closes itself first.
    void Open();

    // Grant the storage codex token (the physical fallback access item) if the player
    // does not already carry one. Called once from the reincarnation, and again on every
    // load as a backfill for saves from before this existed — harmless no-op otherwise.
    void GrantCodexIfMissing();

    // Add a_perItem of every material in a_category to the chest, creating the chest if
    // it does not exist yet. Returns how many distinct stacks were added (0 if the chest
    // could not be created). This is what the System Shop's material packs buy.
    //
    // The chest is no longer stocked at reincarnation: it starts empty for every
    // blessing and is filled with System Points instead, so what you carry is something
    // you chose to spend on rather than something the tier handed you. HERO/ASCENDED
    // still get there faster — their reward scale multiplies System Point income.
    std::size_t StockCategory(MaterialCategory a_category, std::int32_t a_perItem);

    // How many distinct materials a_category would deliver, WITHOUT delivering them or
    // creating a chest. The self-test uses this: the categories are semantic sweeps over
    // the load order (recipe components, official ingredients, filled soul gems), and if
    // one of those filters ever stops matching, the pack still sells and still charges —
    // it just quietly hands over nothing.
    [[nodiscard]] std::size_t CountCategory(MaterialCategory a_category);

    // Deliver a_count of a plain form (the shop's gold packs) into the chest, creating
    // it if needed. Returns false when the chest could not be created.
    bool Deliver(RE::TESBoundObject* a_obj, std::int32_t a_count);

    // Remove non-vanilla ingredients from the chest. Earlier builds stocked the
    // Creation Club ones too, whose tracker scripts made every alchemy visit
    // stutter — chests from those saves keep the problem until cleaned. Called on
    // every load.
    void PruneForeignStock();

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
