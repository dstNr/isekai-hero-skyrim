#pragma once

#include <string>

// Optional, one-way integration with SkyrimNet (AI-driven NPCs). We only ever PUSH
// context to it — "this hero was reincarnated by the System", "they earned the System's
// recognition for <deed>" — so AI NPCs can react to the isekai premise. Nothing is
// pulled back, and there is no build- or load-time dependency: if SkyrimNet is absent
// (or the ini switch is off) every call here is a no-op. Same soft-dependency spirit as
// the PrismaUI patch. Not verified in-game — needs a SkyrimNet install (see docs).

namespace Isekai::SkyrimNet {

    // Detect SkyrimNet and read the ini switch. Call once, at kDataLoaded.
    void Install();

    // True when SkyrimNet is present AND the integration is enabled. Every push below
    // checks this itself, so callers never have to.
    [[nodiscard]] bool Available();

    // Persistent world-knowledge the AI can draw on for the rest of the run — the big,
    // lasting facts (the awakening, a milestone). a_eventType is a short tag
    // ("isekai_awakening"), a_content the natural-language fact, third person.
    void PushEvent(std::string a_eventType, std::string a_content);

    // One-off narration surfaced to nearby NPCs right now, for a momentary beat. Used
    // sparingly — the persistent events above carry the durable context.
    void Narrate(std::string a_content);
}
