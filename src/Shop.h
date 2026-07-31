#pragma once

// The System Shop — a second sink for System Points, once the skill tree stops needing
// them. Closes the loop the roadmap noted: milestones are the only *source*, the tree
// the only *sink*; a bought-out tree leaves points with nothing left to do. Delivers
// straight into the Dimensional Storage, the same AddObjectToContainer path
// GrantStartingMaterials already uses.

namespace Isekai::Shop {

    // Resolve the catalog's soul-gem forms out of Skyrim.esm. Call at kDataLoaded, after
    // Storage::Install() (independent of it, but keeps load-order intent obvious).
    void Install();

    // True under the same conditions as Storage::Available() — the shop delivers into
    // the same chest, so it makes no sense without it. Drives whether the System panel
    // shows the button.
    [[nodiscard]] bool Available();

    // Open the catalog. Main thread only; guarded exactly like Storage::Open() (reached
    // as a status-panel action, so the panel that led here has already closed).
    void Open();
}
