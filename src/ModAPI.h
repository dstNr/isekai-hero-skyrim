#pragma once

// The public API's plumbing. The interface itself lives in include/IsekaiHeroAPI.h,
// which foreign plugins vendor; this is only what our own code needs to know.

#include <cstdint>

namespace Isekai::ModAPI {

    // Log that the API is available and what it answers. Call at kDataLoaded.
    // The export itself needs no registration — it is resolved by name.
    void Install();

    // Hand back the interface for a requested version, or nullptr for one this build
    // does not implement. Exists so the exported entry point never has to name an
    // object that lives in ModAPI.cpp's anonymous namespace.
    [[nodiscard]] void* Resolve(std::uint8_t a_version);

    // Compare the System's state against the last one published and dispatch an
    // SKSE message if it moved. Main thread only; called once a second from the
    // overlay's game-clock tick.
    //
    // Polled rather than pushed from each site that changes the tier: those sites
    // are three today and nobody will remember the fourth. A comparison cannot
    // drift out of date the way a forgotten call site can.
    void PublishIfChanged();
}
