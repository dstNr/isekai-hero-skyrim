# Convert ElevenLabs takes (MP3) into the WAVs the game plays: PCM 16-bit, mono, 44.1 kHz,
# peak-normalised to -1 dBFS, no metadata. -Filter adds the System's light "digital" colour.
#   pwsh -NoProfile -File tools/voice-convert.ps1 -In <dir with .mp3> -Out <dir> [-Filter]
param(
    [Parameter(Mandatory)][string]$In,
    [Parameter(Mandatory)][string]$Out,
    [switch]$Filter
)
$ErrorActionPreference = 'Stop'
if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) { throw "ffmpeg not found on PATH" }
New-Item -ItemType Directory -Force $Out | Out-Null

$colour = 'highpass=f=140,acrusher=bits=12:mix=0.15,aecho=0.6:0.4:9:0.15'
$files = Get-ChildItem $In -Filter *.mp3
if (-not $files) { throw "no .mp3 in $In" }

foreach ($f in $files) {
    $chain = if ($Filter) { "$colour," } else { '' }
    # Pass 1: measure the peak after the optional filter.
    $probe = & ffmpeg -hide_banner -nostats -i $f.FullName -af "${chain}volumedetect" -f null - 2>&1
    $peak = [double](($probe | Select-String 'max_volume: (-?[\d.]+) dB').Matches[0].Groups[1].Value)
    $gain = -1.0 - $peak
    # Pass 2: filter, gain to -1 dBFS peak, mono 44.1 kHz s16, metadata stripped.
    $dst = Join-Path $Out ($f.BaseName + '.wav')
    & ffmpeg -hide_banner -loglevel error -y -i $f.FullName -af "${chain}volume=${gain}dB" `
        -ac 1 -ar 44100 -c:a pcm_s16le -map_metadata -1 -fflags +bitexact -flags:a +bitexact $dst
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed on $($f.Name)" }
    "{0}  peak {1:N1} dB -> -1 dB" -f $dst, $peak
}
