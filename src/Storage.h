#pragma once

namespace Isekai::Storage {

    // Resolve the container base object out of IsekaiHero.esp and arm the hotkey.
    // Call at kDataLoaded.
    void Install();

    // Open the dimensional storage: one chest inventory, reachable from anywhere,
    // shared across the whole playthrough. Main thread only.
    void Open();
}
