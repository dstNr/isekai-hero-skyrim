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
}
