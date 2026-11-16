#pragma once

// Adventurers Guild (Nexus 193777) feeding the System.
//
// That mod sends SKSE mod events for the player's guild work: AG_MissiveCompleted (a
// missive, notice or Jarl's bounty; numArg = its rank 0..5, E..S), AG_DungeonCleared
// (numArg = the dungeon's threat rank) and AG_RankChanged (a promotion; numArg = the new
// rank). This listens for them and pays System Points. Nothing links against the guild:
// without it the events never come, so there is no dependency and nothing to detect.

namespace Isekai::GuildBridge {

    // Register the mod event sink. Call once, after kDataLoaded. A no-op when
    // AdventurersGuildPoints is 0.
    void Install();
}
