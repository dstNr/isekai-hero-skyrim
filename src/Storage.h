#pragma once

namespace Isekai::Storage {

    // Resolve the container and token out of IsekaiHero.esp, hook the equip event
    // (the token is used from the inventory like a potion) and the crafting menu
    // (materials in storage are lent to the player while it is open).
    // Call at kDataLoaded.
    void Install();

    // Open the dimensional storage. Only HERO and ASCENDED get it — the System
    // denies NORMAL with a notification. Main thread only.
    void Open();

    // Hand the player the storage token if they are entitled to one and do not
    // carry it yet. Called after reincarnation and on every load, so characters
    // from older saves receive theirs too.
    void EnsureToken();
}
