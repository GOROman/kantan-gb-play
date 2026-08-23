#include <gb/gb.h>
#include <gb/hardware.h>
#include "sound.h"

/*
 * GB period values for pulse channels: freq = 131072 / (2048 - P)
 * Index = MIDI note - 48, covering C3 (48) .. B6 (95).
 * CH3 (wave, 32-sample) runs one octave lower for the same P,
 * so it indexes this table at note+12.
 */
#define NOTE_MIN 36
#define NOTE_MAX 95

static const uint16_t period_table[NOTE_MAX - NOTE_MIN + 1] = {
      44,  157,  263,  363,  458,  547,  631,  711,  786,  856,  923,  986, /* C2..B2 */
    1046, 1102, 1155, 1205, 1253, 1297, 1339, 1379, 1417, 1452, 1486, 1517, /* C3..B3 */
    1547, 1575, 1602, 1627, 1650, 1673, 1694, 1714, 1732, 1750, 1767, 1783, /* C4..B4 */
    1798, 1812, 1825, 1837, 1849, 1860, 1871, 1881, 1890, 1899, 1907, 1915, /* C5..B5 */
    1923, 1930, 1936, 1943, 1949, 1954, 1959, 1964, 1969, 1974, 1978, 1982, /* C6..B6 */
};

/* Triangle wave for CH3 */
static const uint8_t wave_tri[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
};

static uint16_t period_of(uint8_t note)
{
    if (note < NOTE_MIN) note = NOTE_MIN;
    if (note > NOTE_MAX) note = NOTE_MAX;
    return period_table[note - NOTE_MIN];
}

void apu_init(void)
{
    uint8_t i;

    NR52_REG = 0x80;    /* APU on */
    NR50_REG = 0x77;    /* master volume max */
    NR51_REG = 0xFF;    /* CH1-4 to both L/R */

    /* Load triangle into wave RAM (CH3 must be off) */
    NR30_REG = 0x00;
    for (i = 0; i < 16; i++)
        _AUD3WAVERAM[i] = wave_tri[i];
}

void apu_chord_on(const uint8_t notes[3])
{
    uint16_t p;

    /* CH1 = 3rd (the root is carried by the bass) */
    p = period_of(notes[1]);
    NR10_REG = 0x00;                    /* sweep off */
    NR11_REG = 0x80;                    /* 50% duty */
    NR12_REG = 0xF3;                    /* vol 15, decay */
    NR13_REG = (uint8_t)p;
    NR14_REG = 0x80 | (uint8_t)(p >> 8);

    /* CH2 = 5th (or 7th) */
    p = period_of(notes[2]);
    NR21_REG = 0x80;
    NR22_REG = 0xF3;
    NR23_REG = (uint8_t)p;
    NR24_REG = 0x80 | (uint8_t)(p >> 8);

}

void apu_bass_note(uint8_t note)
{
    /* +12 compensates the wave channel running an octave low */
    uint16_t p = period_of(note + 12);

    NR30_REG = 0x80;                    /* DAC on */
    NR31_REG = 0x00;
    NR32_REG = 0x20;                    /* 100% volume */
    NR33_REG = (uint8_t)p;
    NR34_REG = 0x80 | (uint8_t)(p >> 8);
}

void apu_drum(uint8_t type)
{
    switch (type) {
    case DRUM_KICK:
        NR41_REG = 0x00;    /* length off: envelope shapes it */
        NR42_REG = 0xF1;    /* full volume, fastest decay: punchy thud */
        NR43_REG = 0x74;    /* deep 15-bit rumble */
        break;
    case DRUM_SNARE:
        NR41_REG = 0x00;
        NR42_REG = 0xD1;    /* loud, fast decay: tight snap */
        NR43_REG = 0x34;    /* mid 15-bit noise */
        break;
    case DRUM_HAT:
        NR41_REG = 0x3A;    /* short length cut for a closed tick */
        NR42_REG = 0x81;    /* medium volume, fast decay */
        NR43_REG = 0x09;    /* 7-bit LFSR: bright metallic hiss */
        break;
    default:
        return;
    }
    NR44_REG = (type == DRUM_HAT) ? 0xC0 : 0x80;    /* trigger (+length for hat) */
}

void apu_chord_off(void)
{
    NR12_REG = 0x00;    /* DAC off silences the channel */
    NR22_REG = 0x00;
}

void apu_bass_off(void)
{
    NR30_REG = 0x00;
}
