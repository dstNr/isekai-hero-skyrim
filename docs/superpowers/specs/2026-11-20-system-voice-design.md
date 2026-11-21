# System Voice — design

Status: approved by the owner (2026-11-20). Built after the System enchantments are merged
(both add records to `IsekaiHero.esp`).

## Intent

Isekai stories give "the System" a voice: a cool, precise announcer that speaks when
something important happens. Isekai Hero gets that voice for its key moments, generated
with ElevenLabs in one voice the owner picks, so the System feels present without talking
all the time.

## Goals

- Spoken lines at about twelve moments (below), never on every window or purchase.
- One voice (owner's choice, a cool AI character), delivered in three to four styles for the
  frequent moments, so repeats do not sound the same.
- Plays through the game's audio system, with its own **System Voice** slider in Settings →
  Audio. (First built under the game's Voice category; that category pauses under every
  menu, so lines waited for the System window to close. Changed 2026-11-21, owner's
  decision: own category `0xDCB`, keeps playing in menus, see CREATION_KIT_ESP.md Part N.)
- An MCM and ini switch to turn the voice off.

## Non-goals

- No subtitles; no translated voice (the voice is English; text stays translatable).
- No lines on window open, shop purchase or skill-tree purchase.
- No several different voices.
- No own playback path outside the game's sound system.

## Anti-goals

- A voice that talks so often it gets muted counts as a failure.
- Audio without a valid commercial licence must never ship.

## Approaches considered

| | Approach | Trade-off |
|---|---|---|
| **A (chosen)** | One SNDR per moment in `IsekaiHero.esp`, each holding all its variant files; the engine picks one at random. Category: own (System Voice). | Volume slider works, random choice for free, same path as the existing System sounds (`src/Sounds.cpp`). |
| B | Play WAVs directly from the plugin | No ESP work, but the volume sliders do not apply, and random choice is our code. |

## The moments

| Moment | Example line | Styles × lines |
|---|---|---|
| Reincarnation (System boots) | "System initializing… Soul signature confirmed. Welcome, Reincarnator." | 1 × 2 |
| Blessing chosen | "Blessing accepted. Your new life begins now." | one per blessing |
| Level up | "Level up. Your limits have been raised." | 3–4 × 4 |
| Milestone (Quest Complete) | "Quest complete. Reward granted." | 3–4 × 3 |
| System rank up (E→S) | "System rank increased. Rank D." | one per rank |
| New System quest | "New quest issued." | 3–4 × 3 |
| System quest fulfilled | "Objective fulfilled. System Points awarded." | 3–4 × 3 |
| DORMANT awakens (level 25/80) | "Dormant blessing… awakening." | 1 × 2 |
| REBOOT | "System reboot. Rewriting fate." | 1 × 2 |
| Lethal threat (an enemy reads LETHAL) | "Warning. Lethal threat detected." | 3–4 × 3, at most every few minutes |
| Guild promotion | "Guild rank recognized." | 1 × 2 |
| Lost co-save | "Error. Soul record not found." | 1 × 1 |

Styles for the frequent moments: matter-of-fact, solemn, cold and clipped, faintly
impressed. About 60–70 files in total. Final wording is settled when generating.

## How it is made

- **Voice:** ElevenLabs voice ID `kdmDKE6EkgrWrrykO9Qt` (owner's choice, 2026-11-20).
- **Generation:** through the ElevenLabs MCP the owner sets up with their own API key, on a
  paid plan (Creator) that includes commercial use. All lines are generated while the
  subscription is active; the owner keeps the invoice as proof. Styles through ElevenLabs'
  voice settings or delivery tags, same voice ID throughout.
- **Files:** MP3 from ElevenLabs converted to WAV (the format of the existing System
  sounds), mono, peak-normalised so the moments sit at one loudness. An optional light
  "digital" filter for the AI character is tried and played to the owner before it is used.
- **Records:** one SNDR per moment, category System Voice (own SNCT), all variants listed; local FormIDs
  ≤ 0xFFF; resolved like the existing ones in `Sounds.cpp`.
- **Triggers:** each moment's existing code path (reincarnation, blessing, level-up,
  milestone payout, rank, quests, dormant awakening, REBOOT, threat verdict, guild, co-save
  check) calls the voice; the lethal-threat line has its own cooldown.
- **Switch:** `SystemVoice` (ini and MCM, default on).
- **Self-test:** the voice descriptors resolve.

## Constraints

- Same runtime range and single DLL as the rest of the mod.
- Archive size stays modest (WAV, short lines; a few MB).
- Licence: ElevenLabs paid plan at generation time; the README credits ElevenLabs.

## Left to the builder

- Exact wording of each line and how many variants per moment, within the table above.
- The cooldown length for the lethal-threat line.
- Whether the digital filter is used (owner listens first).
