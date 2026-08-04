#pragma once

// The System Shop — a second sink for System Points, once the skill tree stops needing
// them. Closes the loop the roadmap noted: milestones are the only *source*, the tree
// the only *sink*; a bought-out tree leaves points with nothing left to do. Delivers
// straight into the Dimensional Storage, the same AddObjectToContainer path
// GrantStartingMaterials already uses.
//
// The catalog is exposed as data (Catalog/Buy) rather than baked into one renderer's
// button list: the web patch draws it as item cards and the built-in UI as its own
// window, and both must offer exactly the same goods at the same prices.

#include <cstdint>
#include <string>
#include <vector>

namespace Isekai::Shop {

    // One purchasable entry. Resolved fresh by Catalog(), so an item whose form is
    // missing from the load order simply never appears.
    struct Item {
        std::string  name;   // "Grand Soul Gem"
        std::string  qty;    // "x1" — shown next to the name, never parsed
        std::int32_t cost;   // System Points
        std::string  icon;   // file name under the icons folder
        std::string  shelf;  // display group, e.g. "ELIXIRS" — see Shelves()
    };

    // The display groups, in the order they should be offered. Both renderers build
    // their category list from this rather than collecting the distinct shelf names
    // themselves, so an empty group (every potion still awaiting its ESP record) keeps
    // its place instead of silently reordering the ones around it.
    [[nodiscard]] std::vector<std::string> Shelves();

    // Resolve the catalog's soul-gem forms out of Skyrim.esm. Call at kDataLoaded, after
    // Storage::Install() (independent of it, but keeps load-order intent obvious).
    void Install();

    // True under the same conditions as Storage::Available() — the shop delivers into
    // the same chest, so it makes no sense without it. Drives whether the System panel
    // shows the button.
    [[nodiscard]] bool Available();

    // The goods on offer, in display order. Indices into this vector are what Buy()
    // takes, so both renderers must build their list from one Catalog() call and not
    // renumber it.
    [[nodiscard]] std::vector<Item> Catalog();

    // Spend and deliver entry a_index of Catalog(). False when the index is stale, the
    // player cannot afford it, or the storage chest is not ready — each already reported
    // to the player as a notification. Main thread only.
    bool Buy(int a_index);

    // Open the catalog. Main thread only; guarded exactly like Storage::Open() (reached
    // as a status-panel action, so the panel that led here has already closed).
    void Open();
}
