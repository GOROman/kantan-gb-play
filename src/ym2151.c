#include <gb/gb.h>
#include "ym2151.h"

/*
 * Minimal YM2151 driver for the Chromatic FPGA expansion.
 * Untestable on stock emulators; main.c falls back to the GB APU
 * when ym_detect() fails.
 */

static void ym_write(uint8_t reg, uint8_t data)
{
    uint8_t guard = 0xFF;
    /* wait while busy (status bit7), with a timeout guard */
    while ((YM_STATUS & 0x80) && --guard)
        ;
    YM_REG = reg;
    YM_DATA = data;
}

uint8_t ym_detect(void)
{
    return EXT_ID == YM_EXT_ID_VALUE;
}

/*
 * Simple organ-ish patch on CH0-2:
 * CON=7 (all operators are carriers), only M1 audible (others TL=127).
 */
void ym_init(void)
{
    uint8_t ch, op;

    /* chord voices CH0-2, bass on CH4 (per the Chromatic design doc) */
    for (ch = 0; ch < 5; ch++) {
        if (ch == 3)
            continue;
        ym_write(0x08, ch);                     /* key off */
        ym_write(0x20 + ch, 0xC7);              /* RL=both, FB=0, CON=7 */
        for (op = 0; op < 4; op++) {
            uint8_t o = op * 8 + ch;
            ym_write(0x40 + o, 0x01);           /* DT1=0, MUL=1 */
            ym_write(0x60 + o, op == 0 ? 0x18 : 0x7F); /* TL: M1 on, rest off */
            ym_write(0x80 + o, 0x1F);           /* KS=0, AR=31 */
            ym_write(0xA0 + o, 0x05);           /* D1R */
            ym_write(0xC0 + o, 0x00);           /* D2R */
            ym_write(0xE0 + o, 0x07);           /* D1L=0, RR=7 */
        }
    }
}

/* YM2151 key code: note within octave uses a 0-15 code with gaps */
static const uint8_t kc_note[12] = {
    /* C   C#  D   D#  E   F   F#  G   G#  A   A#  B  */
      14,  0,  1,  2,  4,  5,  6,  8,  9, 10, 12, 13
};

static uint8_t midi_to_kc(uint8_t note)
{
    uint8_t sem = note % 12;
    uint8_t oct = note / 12;    /* MIDI octave (C4=60 -> 5) */
    uint8_t kc_o;

    /* A4 (69) must land on KC 0x4A: octave = midi_oct - 1, C shifts down one */
    kc_o = oct - 1;
    if (sem == 0)
        kc_o--;
    if (kc_o > 7)
        kc_o = 7;
    return (kc_o << 4) | kc_note[sem];
}

void ym_chord_on(const uint8_t notes[3])
{
    uint8_t ch;

    for (ch = 0; ch < 3; ch++) {
        ym_write(0x08, ch);                     /* key off first */
        ym_write(0x28 + ch, midi_to_kc(notes[ch]));
        ym_write(0x30 + ch, 0x00);              /* KF = 0 */
        ym_write(0x08, 0x78 | ch);              /* key on, all slots */
    }
}

void ym_bass_note(uint8_t note)
{
    ym_write(0x08, 4);                          /* key off */
    ym_write(0x28 + 4, midi_to_kc(note));
    ym_write(0x30 + 4, 0x00);
    ym_write(0x08, 0x78 | 4);                   /* key on */
}

void ym_chord_off(void)
{
    uint8_t ch;
    for (ch = 0; ch < 3; ch++)
        ym_write(0x08, ch);
}

void ym_bass_off(void)
{
    ym_write(0x08, 4);
}
