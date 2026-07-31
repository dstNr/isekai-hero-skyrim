#pragma once

// "System Analysis" — the isekai signature move this mod was missing: point at
// something, get a System readout (Solo Leveling's "Observation", Overlord's
// "Appraisal"). Gated behind a skill-tree node (SkillTree::kAnalyzeNodeKey) so it is
// something earned, not handed out for free.

namespace Isekai::Analyze {

    // Register the hotkey (Config::AnalyzeKey). Call once, at kDataLoaded.
    void Install();
}
