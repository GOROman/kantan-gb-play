#include <gb/gb.h>
#include "ym2151.h"
#include "adpcm_smp.h"

static uint8_t has_adpcm;

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
/* HARRIER.MDX voice 4: the actual bass patch (used by its bass tracks) */
static const uint8_t voice_bass[26] = {
    0x3A, 0x0F,
    0x00, 0x00, 0x04, 0x01,
    0x22, 0x28, 0x21, 0x04,
    0x5F, 0x5F, 0x5F, 0x9F,
    0x0B, 0x0B, 0x0B, 0x06,
    0x05, 0x05, 0x05, 0x04,
    0x37, 0x37, 0x37, 0x37,
};
/* single-op sine thump, M1 only (FM drum fallback on CH3) */
static const uint8_t voice_kick[26] = {
    0x07, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x7F, 0x7F, 0x7F,
    0x1F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00,
    0xFA, 0x00, 0x00, 0x00,
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

/* streaming state: sample currently being fed to the ADPCM FIFO */
static const uint8_t *ad_ptr;
static uint16_t ad_left = 0;

/* feed while the FIFO signals ready, bounded per call */
static void adpcm_feed(uint8_t budget)
{
    if (!ad_left)
        return;
    YM_REG = 0xFF;                          /* data mode */
    while (ad_left && budget-- && (YM_STATUS & 0x40)) {
        YM_DATA = *ad_ptr++;
        ad_left--;
    }
}

static void adpcm_play(const uint8_t *data, uint16_t len)
{
    YM_REG = 0xFD;                          /* stop */
    ad_ptr = data;
    ad_left = len;
    adpcm_feed(64);                         /* prime the FIFO */
    YM_REG = 0xFE;                          /* play */
}

void ym_adpcm_tick(void)
{
    if (has_adpcm)
        adpcm_feed(160);    /* > 131 bytes/frame needed at 15.6 kHz */
}

void ym_init(void)
{
    uint8_t ch;

    for (ch = 0; ch < 3; ch++)
        load_voice(ch, voice_chord);
    load_voice(4, voice_bass);
    load_voice(3, voice_kick);              /* FM drum fallback */

    has_adpcm = (EXT_VERSION >= 0x02);
    if (has_adpcm) {
        YM_REG = 0xFD;                      /* ADPCM stop */
        ADPCM_CTRL = 0xF4;                  /* volume max, ADPCM enable */
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

void ym_bass_off(void)
{
    ym_write(0x08, 4);
}

void ym_drum(uint8_t type)
{
    uint8_t kc;

    if (type == 0)
        return;

    /* ADPCM one-shots (1ch, so a new hit cuts the previous one) */
    if (has_adpcm) {
        switch (type) {
        case 1: adpcm_play(adpcm_kick, ADPCM_KICK_LEN); return;
        case 2: adpcm_play(adpcm_snare, ADPCM_SNARE_LEN); return;
        case 4: adpcm_play(adpcm_crash, ADPCM_CRASH_LEN); return;
        default: break;     /* hat falls through to FM */
        }
    }

    /* FM fallback on CH3: same thump patch, pitched per drum */
    switch (type) {
    case 1:  kc = 0x0E; break;      /* kick: low */
    case 2:  kc = 0x3A; break;      /* snare-ish: mid */
    case 3:  kc = 0x6A; break;      /* hat-ish: high tick */
    default: kc = 0x5A; break;      /* crash-ish */
    }
    ym_write(0x08, 3);
    ym_write(0x28 + 3, kc);
    ym_write(0x08, (0x01 << 3) | 3);            /* key on M1 */
}

void ym_chord_off(void)
{
    uint8_t ch;
    for (ch = 0; ch < 3; ch++)
        ym_write(0x08, ch);
}
