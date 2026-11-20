#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Isekai::Enchantments {

    // The eight System enchantments: base ENCH records in IsekaiHero.esp (docs/CREATION_KIT_ESP.md
    // Part M), each taught by one skill-tree node in the ENCHANT band.

    // Look up every form by its local FormID. Call at kDataLoaded; logs each miss.
    void Resolve();

    // Mark the node's enchantment known at the enchanting table. False if the key is not one
    // of the eight or its form did not resolve.
    bool Teach(std::uint32_t a_nodeKey);

    // Known flag set for the eight whose node is in a_unlockedKeys, cleared for the rest.
    // The flag lives on the form, not in the save, so a load must re-derive it both ways.
    void SyncKnown(std::span<const std::uint32_t> a_unlockedKeys);

    [[nodiscard]] bool IsSystemEnchantment(const RE::EnchantmentItem* a_ench);

    // How many of the eight ENCH records resolved (self-test).
    [[nodiscard]] std::size_t Resolved();

    // Later tasks' handles, null until Resolve() found them.
    [[nodiscard]] RE::EffectSetting* StormchainCarrier();
    [[nodiscard]] RE::EffectSetting* BreachCarrier();
    [[nodiscard]] RE::EffectSetting* PhoenixCarrier();
    [[nodiscard]] RE::SpellItem*     StormchainJump();
    [[nodiscard]] RE::BGSArtObject*  BreachArt();
}
