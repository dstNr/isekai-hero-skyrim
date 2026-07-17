#pragma once

// Zero-transfer crafting for the Dimensional Storage.
//
// Goal: crafting stations read (and, later, consume) the storage chest WITHOUT
// physically moving items — no shuttle, no VM event flood, no timing race. The
// technique (which engine functions to intercept, their Address Library IDs, and
// the RemoveItem vtable slot) is adapted from SCIE — Skyrim Crafting Inventory
// Extender by ohfor, MIT-licensed: https://github.com/ohfor/scie
//
// Rollout is phased. This first build installs ONLY a pass-through validation hook
// on the item-count function: it observes and logs, and changes no behavior. It
// exists to confirm, on the live game version, that we intercept the right call
// before anything goes live.

namespace Isekai::CraftHooks {

    // Install the crafting inventory hooks. Call once, after kDataLoaded. Guards
    // itself against a missing/mismatched Address Library — on any failure it logs
    // and disables itself rather than risk a crash.
    void Install();

    // True once the item-crafting hooks (count + consume) are live. Storage uses this
    // to decide whether to disable the old shuttle for forge-family stations: if the
    // hooks did NOT install (e.g. Address Library mismatch), the shuttle stays on as a
    // fallback so item crafting never loses access to the chest.
    [[nodiscard]] bool ItemCraftingHooksActive();

    // True once the iteration hooks are live — alchemy and enchanting read the chest's
    // ingredients / soul gems in place. Same fallback contract as above: if they did not
    // install, the shuttle keeps covering those stations.
    [[nodiscard]] bool IterationHooksActive();
}
