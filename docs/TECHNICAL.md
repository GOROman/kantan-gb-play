**English** | [日本語](TECHNICAL.ja.md)

# KANTAN GB PLAY — Technical Report

A Game Boy Color chord instrument that also targets a custom FPGA build of the
MODRETRO Chromatic with a YM2151 (OPM) core and an MSM6258-style ADPCM channel.
This document records how the ROM works and the design decisions behind it.

## 1. Architecture

```
main.c        input debounce, free-running 8th-note grid, demo sequencer
chord.c       chord tables (KANTAN Music API output), D-pad decoding
sound.c       GB APU driver  (fallback path: any GBC / emulator)
ym2151.c      Chromatic extension driver (OPM + ADPCM via FF28-FF2F)
ui.c          tile UI, GBC attribute highlighting, status displays
```

At boot the ROM reads `FF2E`; `0x51` selects the YM2151/ADPCM path, anything
else falls back to the GB APU. Everything else (chord logic, timing, UI) is
shared between the two paths.

## 2. Chord engine

Chord voicings are the *actual output* of the KANTAN Music API. The
`KANTAN_Play_core` repository ships prebuilt libraries per platform; the
`m1mac` build runs natively on Apple Silicon, so a small C harness called
`KANTANMusic_GetMidiNoteNumber()` for every degree/pitch and the results were
dumped into `chord.c` (Close voicing, key = C, pitches 4-6 of 6).

Findings worth recording:

- KANTAN degree 7 defaults to **Bm**, not Bdim (dim is a modifier).
- `minor_swap` inverts chord quality; the swapped tables were dumped the same way.
- C7 is degree 1 with `Modifier_7`.
- With `bass_degree = 1` (on-chord), pitch 1 becomes a constant C and pitch 2
  is muted — which is exactly the "X/C pedal bass" the ROM implements.

The APU path can only afford two pulse voices for the chord, so each table
entry is ordered `{extra, root-ish, 3rd-ish}`: the two played notes always
include the 3rd, otherwise F and Fm would be indistinguishable (their
difference sits in the note the 2-voice subset used to drop).

## 3. Timing

One global 8th-note grid runs freely from boot: `frames_per_8th =
round(1800 / BPM)` at ~60 fps. Everything quantizes to it — bass steps, drum
hits, and chord key-off (key-on is immediate for playability; a lone B release
must not retrigger). Because the grid never resets, re-pressing A cannot break
the groove. Joypad reads are debounced by accepting a value only after two
identical consecutive frames.

## 4. GB APU path

| Channel | Role |
|---|---|
| CH1/CH2 (pulse) | chord (root + 3rd subset) |
| CH3 (wave, triangle) | octave bass C2↔C3, C pedal |
| CH4 (noise) | kick / snare / hat / crash |

The period table covers C2–B7 with `P = 2048 - 131072/f`. CH3 plays one octave
lower than the pulses for the same period value, so the bass looks the note up
at `note + 12`. Drum voices are envelope programs: kick = max volume, fastest
decay into deep 15-bit noise; hat = 7-bit LFSR with a length-counter cut;
crash = slow decay.

## 5. Chromatic extension (FF28-FF2F)

| Reg | Function |
|---|---|
| FF28 | OPM register address; `0xFF`/`0xFE`/`0xFD` are ADPCM escapes (data mode / play / stop, committed by the next FF29 write) |
| FF29 | OPM data, or ADPCM sample data in data mode |
| FF2A | bit7 YM/bridge busy, bit6 ADPCM FIFO ready, bit5 ADPCM playing |
| FF2B | bit7-4 ADPCM volume, bit2 ADPCM enable, bit1 GB APU enable, bit0 YM enable |
| FF2E | expansion ID (`0x51`) |
| FF2F | expansion version (`0x03` = separated status map) |

Because `0xFD-0xFF` on FF28 are escapes, OPM registers 0xFD-0xFF (the D1L/RR
of channels 5-7's C2 slot) are unreachable; those channels are simply not used.
Channel plan: CH0-2 chord, CH3 FM hi-hat, CH4 bass.

OPM voices use the MDX 27-byte layout (FL/CON, slot mask, then per-operator
DT1/MUL, TL, KS/AR, AME/D1R, DT2/D2R, D1L/RR × M1,M2,C1,C2), written to
`0x40+i*8+ch` … `0xE0+i*8+ch` — the same register mapping mdxtools uses. Key
code conversion is the OPM 4-bits-per-octave scheme with gaps
(C#=0 … C=14, octave in bits 6-4), clamped at both ends.

The chord and bass patches are the OPM voice parameters used by X68000 Space
Harrier MDX covers (synth register settings, not audio). The FM hi-hat is a
single-operator patch with maximum feedback — self-modulation turns the sine
into a metallic hiss.

## 6. ADPCM drums

The FPGA exposes a 1-channel MSM6258-style decoder (4-bit OKI ADPCM,
15.625 kHz) fed through a 256-byte FIFO. There is no DMA on the Game Boy for
I/O, so the CPU streams the sample: ~131 bytes per frame are needed and the
driver feeds up to 160 per frame while FF2A bit6 (ready) allows, after waiting
for bit7 (bridge busy) so the bridge cannot drop the first bytes while a JT51
write is still in flight. A hit primes the FIFO (stop → data mode → 160 bytes
→ play), then `ym_adpcm_tick()` keeps it fed each frame. CPU cost is roughly
10-15% of a frame.

The committed samples are original synthesized drums
(`tools/gen_drums.py` → `tools/adpcm.py` OKI encoder, low nibble first —
matching the decoder). `tools/pdx2c.py` can extract slots from an MDX PDX file
for **local** testing; extracted game audio must not be committed.

## 7. MDX analysis

The demo progression, tempo, rhythm pattern and voices come from analyzing
X68000 MDX covers of the Space Harrier main theme with a small Python parser
(format per the mdxtools docs): note byte `0x80+n` → MIDI `n+3`, 48 ticks per
quarter, tempo `BPM = 4883 / (256 - @t)`. Aggregating sounding pitch classes
per half bar across the FM channels yields the progression
**C → F → Fm → C** (×3) with an **F → Dm → Fm → C** turnaround at
`@t225 ≈ 157.5 BPM`, consistent across three independent covers. The rhythm
channel is four-on-the-floor kicks with a fill every 8th bar; the bass track
alternates octaves in 8ths. Chord progressions, tempos and rhythm patterns are
facts about the music and carry no melody data.

## 8. UI pipeline

The wheel (11×11 tiles) and the circled degree badges are generated by
`tools/gen_wheel.py` / `tools/gen_badges.py`: render to a 1-bit bitmap,
slice into 2bpp GB tiles, deduplicate (the wheel needs 73 unique tiles), emit
C arrays plus a tilemap. VRAM: font 0-95 (GBDK stdio), D-pad cross 128-132,
wheel 133-205, badges 206-212. Highlighting is pure GBC attribute-map work
(palette 1 = selected, 2 = playing) so no tiles are rewritten at runtime.

## 9. GBDK pitfalls encountered

- `printf("%u", (uint8_t)x)` garbles: varargs are not promoted, `%u` reads
  16 bits. Cast to `uint16_t` (symptom: `OCT:-2` displayed as `OCT:-512`).
- `printf("%c%c", a, b)` drops the second character; print one `%c` per call
  (symptom: "Em" logged as "E", hex dumps losing digits).
- Writing column 19 of row 17 scrolls the console and silently destroys row 0.
- The wave channel sounds one octave below the pulses for the same period
  value; compensate with a +12 table lookup.
- Field widths (`%02X`) are ignored; format hex manually.
