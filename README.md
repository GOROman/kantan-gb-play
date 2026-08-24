**English** | [日本語](README.ja.md)

# KANTAN GB PLAY

A chord-playing instrument ROM for the Game Boy Color.
Inspired by [KANTAN Play](https://github.com/InstaChord/KANTAN_Play_core)'s "degree + key → chord" concept: pick one of eight chords with the D-pad and strum it with the A button. The key is fixed to C major.

![screenshot](docs/screenshot.png)

## ▶ Play in your browser

Boots on [claude-gb-emu](https://github.com/GOROman/claude-gb-emu) (a WASM GBC emulator) with the latest ROM loaded automatically:

**https://goroman.github.io/claude-gb-emu/?rom=https%3A%2F%2Fraw.githubusercontent.com%2FGOROman%2Fkantan-gb-play%2Fmain%2Fkantan-gb-play.gbc**

The latest prebuilt ROM lives at the repository root as [`kantan-gb-play.gbc`](kantan-gb-play.gbc); older builds are kept in [`roms/`](roms/).

## Controls

| Input | Function |
|---|---|
| D-pad (8 directions) | Select a chord (diagonals = two buttons). Bass & rhythm keep running while held |
| A | Play the chord (sounds while held; releases on the next 8th-note grid step) |
| B | Play the minor-swapped chord (C→Cm, F→Fm, ...; can also toggle while A is held) |
| START | Stop everything (panic, immediate) + reset the PROG log |

### SELECT combos

| Input | Function |
|---|---|
| SELECT + Up / Down | BPM +5 / −5 (40–240, default 155; shown in the top bar) |
| SELECT + Right / Left | Octave +1 / −1 (8 steps: −3 to +4, shown as `OCT:`) |
| SELECT + A | Demo mode (auto-plays a Space Harrier-style progression at BPM 155). Press again or START to stop |

- Note-on/off, bass and rhythm are all quantized to a free-running 8th-note grid
- `PROG:` logs the last 8 chords you played (including swapped ones like Fm) in a 4x2 grid

## Screen

- Center: an 8-petal chord wheel (chord names on the rim, KANTAN degree badges ①–⑦ inside, a D-pad cross in the middle). Purple = selected, green = playing
- Right: `CHORD` (current chord; shown as a slash chord like `Dm/C` since the bass pedals on C), `OCT:`, `PROG:`
- Top bar: BPM and the active sound source (`APU` / `YM2151`)

## Sound

- **GB APU** (any GBC or emulator): two pulse channels for the chord, the wave channel (triangle) for an 8th-note octave bass (C2↔C3, C pedal), and the noise channel for a four-on-the-floor kick with a fill every 8th bar
- **YM2151** (MODRETRO Chromatic FPGA extension): when `FF2E == 0x51` is detected at boot, every part moves to FM/ADPCM
  - CH0-2: 3-voice chord / CH4: bass / CH3: FM hi-hat
  - Kick, snare and crash are MSM6258-compatible ADPCM one-shots (`FF28` escapes 0xFF/0xFE/0xFD, streamed into the FIFO every frame)
  - Chord and bass patches come from the OPM voice parameters of an X68000 Space Harrier MDX cover
  - The drum samples are original synthesized sounds (regenerate with `tools/gen_drums.py`). To swap in samples from your own PDX file locally, run `python3 tools/pdx2c.py <FILE.PDX> <kick> <snare> <crash>`

## Building

Requires [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) (expected at `~/gbdk-install/gbdk`, override with `make GBDK=...`).

```sh
make            # builds build/kantan-gb-play.gbc
make run        # launch in mGBA
make release    # copy to roms/ with a timestamp and refresh the root ROM
```

See [docs/TECHNICAL.md](docs/TECHNICAL.md) for a full technical report (architecture, register maps, timing, GBDK pitfalls).

## Layout

- `src/main.c` — main loop (input debounce, 8th-note grid, demo sequencer)
- `src/chord.c` — chord tables (real KANTAN Music API Close-voicing output, 8 normal + 8 swapped)
- `src/sound.c` — GB APU driver (C2–B7 period table, chord/bass/drums)
- `src/ym2151.c` — Chromatic extension driver (OPM voices, KC conversion, ADPCM streaming)
- `src/ui.c` — UI (wheel, highlights, CHORD/PROG/BPM/OCT displays)
- `tools/` — tile and sample generators (Python)
