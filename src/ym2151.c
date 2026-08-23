#include <gb/gb.h>
#include "ym2151.h"

/*
 * YM2151 driver for the Chromatic FPGA expansion.
 * When detected, ALL parts play on the YM2151:
 *   CH0-2 chord, CH4 bass, CH6 FM kick, CH7 noise drums (NE bit).
 * Untestable on stock emulators; main.c falls back to the GB APU.
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
 * OPM voices in MDX layout minus the ID byte:
 * FL/CON, slot mask, then per-op x4 (M1 M2 C1 C2):
 * DT1/MUL, TL, KS/AR, AME/D1R, DT2/D2R, D1L/RR.
 * chord/bass voices extracted from the Space Harrier X68000 MDX
 * (HARRIER.MDX voice 1 and voice 5); drum voices are hand-made.
 */
static const uint8_t voice_chord[26] = {
    0x3B, 0x0F,
    0x02, 0x02, 0x02, 0x02,
    0x1B, 0x1A, 0x17, 0x04,
    0x5F, 0x5F, 0x1F, 0x1F,
    0x00, 0x00, 0x00, 0x00,
    0x03, 0x03, 0x00, 0x00,
    0x03, 0x03, 0x03, 0x08,
};
static const uint8_t voice_bass[26] = {
    0x3A, 0x0F,
    0x3F, 0x03, 0x3A, 0x61,
    0x05, 0x00, 0x00, 0x02,
    0x1F, 0x19, 0x1A, 0x1F,
    0x0F, 0x10, 0x10, 0x11,
    0x4D, 0xCA, 0x8A, 0x0D,
    0xD5, 0xA5, 0xA5, 0xA7,
};
/* single-op sine thump, M1 only */
static const uint8_t voice_kick[26] = {
    0x07, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x7F, 0x7F, 0x7F,
    0x1F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00,
    0xFA, 0x00, 0x00, 0x00,
};
/* noise drum on CH7: only C2 (the noise slot) sounds */
static const uint8_t voice_noise[26] = {
    0x07, 0x08,
    0x00, 0x00, 0x00, 0x00,
    0x7F, 0x7F, 0x7F, 0x05,
    0x00, 0x00, 0x00, 0x1F,
    0x00, 0x00, 0x00, 0x0B,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xF8,
};

static void load_voice(uint8_t ch, const uint8_t *v)
{
    uint8_t i;

    ym_write(0x08, ch);                     /* key off */
    ym_write(0x20 + ch, 0xC0 | v[0]);       /* RL both, FL, CON */
    for (i = 0; i < 4; i++) {
        uint8_t o = i * 8 + ch;
        ym_write(0x40 + o, v[2 + i]);       /* DT1, MUL */
        ym_write(0x60 + o, v[6 + i]);       /* TL */
        ym_write(0x80 + o, v[10 + i]);      /* KS, AR */
        ym_write(0xA0 + o, v[14 + i]);      /* AME, D1R */
        ym_write(0xC0 + o, v[18 + i]);      /* DT2, D2R */
        ym_write(0xE0 + o, v[22 + i]);      /* D1L, RR */
    }
}

void ym_init(void)
{
    uint8_t ch;

    for (ch = 0; ch < 3; ch++)
        load_voice(ch, voice_chord);
    load_voice(4, voice_bass);
    load_voice(6, voice_kick);
    load_voice(7, voice_noise);
    ym_write(0x0F, 0x00);                   /* noise off for now */
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

void ym_bass_off(void)
{
    ym_write(0x08, 4);
}

void ym_drum(uint8_t type)
{
    switch (type) {
    case 1: /* DRUM_KICK: low FM thump on CH6 */
        ym_write(0x08, 6);
        ym_write(0x28 + 6, 0x0E);               /* low C */
        ym_write(0x08, (0x01 << 3) | 6);        /* key on M1 */
        break;
    case 2: /* DRUM_SNARE */
        ym_write(0x08, 7);
        ym_write(0x0F, 0x8C);                   /* noise on, mid freq */
        ym_write(0xE0 + 24 + 7, 0xF8);          /* C2 D1L/RR: short */
        ym_write(0x28 + 7, 0x4A);
        ym_write(0x08, (0x08 << 3) | 7);        /* key on C2 */
        break;
    case 3: /* DRUM_HAT */
        ym_write(0x08, 7);
        ym_write(0x0F, 0x83);                   /* noise on, bright */
        ym_write(0xE0 + 24 + 7, 0xFA);
        ym_write(0x28 + 7, 0x4A);
        ym_write(0x08, (0x08 << 3) | 7);
        break;
    case 4: /* DRUM_CRASH: long noise wash */
        ym_write(0x08, 7);
        ym_write(0x0F, 0x86);
        ym_write(0xE0 + 24 + 7, 0x04);          /* slow release */
        ym_write(0x28 + 7, 0x4A);
        ym_write(0x08, (0x08 << 3) | 7);
        break;
    default:
        break;
    }
}

void ym_chord_off(void)
{
    uint8_t ch;
    for (ch = 0; ch < 3; ch++)
        ym_write(0x08, ch);
}
