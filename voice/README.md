# The System's voice

The spoken lines in `sounds/voice/` are text-to-speech takes, one take per line.

| | |
|---|---|
| Generated | 2026-11-21 |
| Service | ElevenLabs, model `eleven_v4` |
| Voice ID | `kdmDKE6EkgrWrrykO9Qt` |
| Plan tier | Creator |

`lines.tsv` holds every file with the exact text that was sent, including the delivery tags
(`[solemnly]`, `[coldly] [clipped]`; the plain variant has none). One-off moments are solemn only;
the frequent moments (level up, milestone, new quest, quest done, lethal threat) have each line in
solemn, cold and plain, and the plugin picks one at random.

Every take was transcribed and compared with its text before conversion.

## Rebuilding the WAVs

The MP3 takes are not kept in the repository. From a folder of takes named like the WAVs:

    pwsh -NoProfile -File tools/voice-convert.ps1 -In <mp3 folder> -Out sounds/voice -Filter

`-Filter` adds the System's colour (light digital crush and flange, small-room reverb); output is
PCM 16-bit mono 44.1 kHz, peak-normalised to -1 dBFS, without metadata.
