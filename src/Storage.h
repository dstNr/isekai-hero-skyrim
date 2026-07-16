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
}
