#include <gb/gb.h>
#include "ym2151.h"
#include "adpcm_smp.h"

static uint8_t has_adpcm;

/*
 * YM2151 driver for the Chromatic FPGA expansion.
 * When detected, ALL parts play on the YM2151/ADPCM:
 *   CH0-2 chord, CH4 bass, CH7 native-noise hi-hat;
 *   kick/snare/crash are MSM6258 ADPCM one-shots.
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
    0x22, 0x28, 0x21, 0x00,     /* carrier TL 4 -> 0: full level */
    0x5F, 0x5F, 0x5F, 0x9F,
    0x0B, 0x0B, 0x0B, 0x06,
    0x05, 0x05, 0x05, 0x04,
    0x37, 0x37, 0x37, 0x37,
};
/* Native YM2151 noise on CH7: only C2 (the noise slot) sounds. */
static const uint8_t voice_noise[26] = {
    0x07, 0x08,
    0x00, 0x00, 0x00, 0x00,
    0x7F, 0x7F, 0x7F, 0x05,
    0x00, 0x00, 0x00, 0x1F,
    0x00, 0x00, 0x00, 0x0B,
    0x00, 0x00, 0x00, 0x0A,     /* C2 D2R: keep decaying past D1L,
                                   otherwise the hat sustains forever */
    0x00, 0x00, 0x00, 0xFA,
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
static uint8_t noise_frames = 0;

/* FF2B: bit7-4 volume, bit3 play (edge), bit2 ADPCM enable,
   bit1 GB APU enable, bit0 YM enable */
#define ACTL_STOP 0xC1      /* vol C, YM on, APU off, ADPCM muted, play=0 */
#define ACTL_PLAY 0xCD      /* vol C, YM on, APU off, ADPCM on, play=1 */

/* feed while the FIFO signals ready, bounded per call.
   FF2A is a dedicated ADPCM data port (writes), independent of the
   YM address/data pair - no escapes, no busy interlock needed. */
static void adpcm_feed(uint8_t budget)
{
    while (ad_left && budget-- && (YM_STATUS & 0x40)) {
        ADPCM_DATA = *ad_ptr++;             /* MSM6258: low nibble first */
        ad_left--;
    }
}

static void adpcm_play(const uint8_t *data, uint16_t len)
{
    ADPCM_CTRL = ACTL_STOP;                 /* play 1->0 edge: stop, mute */
    ad_ptr = data;
    ad_left = len;
    adpcm_feed(160);                        /* prime beyond one video frame */
    ADPCM_CTRL = ACTL_PLAY;                 /* play 0->1 edge: start */
}

void ym_adpcm_tick(void)
{
    if (noise_frames && --noise_frames == 0) {
        ym_write(0x08, 7);                  /* key off CH7 */
        ym_write(0x0F, 0x00);               /* disable noise */
        ym_write(0x7F, 0x7F);               /* mute CH7/C2 immediately */
    }
    if (has_adpcm)
        adpcm_feed(160);    /* > 131 bytes/frame needed at 15.6 kHz */
}

void ym_init(void)
{
    uint8_t ch;

    for (ch = 0; ch < 3; ch++)
        load_voice(ch, voice_chord);
    load_voice(4, voice_bass);
    load_voice(7, voice_noise);
    ym_write(0x0F, 0x00);                   /* noise off until first hat */

    /* This ROM targets the Chromatic bitstream with MSM6258 support.  Keep
       the version read as informational; older bridge revisions returned
       zero here even though the ADPCM path was present. */
    has_adpcm = 1;
    if (has_adpcm)
        ADPCM_CTRL = ACTL_STOP;             /* YM on, APU off, ADPCM muted at boot */
}

/* YM2151 key code: note within octave uses a 0-15 code with gaps */
static const uint8_t kc_note[12] = {
    /* C   C#  D   D#  E   F   F#  G   G#  A   A#  B  */
      14,  0,  1,  2,  4,  5,  6,  8,  9, 10, 12, 13
};

static uint8_t midi_to_kc(uint8_t note)
{
    uint8_t sem = note % 12;
    int8_t kc_o;

    /* A4 (69) must land on KC 0x4A: octave = midi_oct - 1, C shifts down one */
    kc_o = (int8_t)(note / 12) - 1;
    if (sem == 0)
        kc_o--;
    if (kc_o < 0)
        kc_o = 0;
    if (kc_o > 7)
        kc_o = 7;
    return ((uint8_t)kc_o << 4) | kc_note[sem];
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
    ym_write(0x08, 7);      /* also release the hat (CH7 noise) */
}

/* Kick/snare/crash are ADPCM one-shots (1ch, a new hit cuts the
   previous one); without the ADPCM extension they stay silent
   (FM click-drums sounded like a metronome). The hi-hat uses the
   native YM2151 noise generator on CH7/C2. */
void ym_drum(uint8_t type)
{
    if (type == 3) {                            /* DRUM_HAT: YM noise */
        ym_write(0x08, 7);                      /* key off CH7 */
        ym_write(0x0F, 0x83);                   /* noise on, bright */
        ym_write(0x7F, 0x05);                   /* restore CH7/C2 level */
        ym_write(0xFF, 0xFA);                   /* C2 D1L/RR: short */
        ym_write(0x28 + 7, 0x4A);
        ym_write(0x08, 0x40 | 7);               /* key on C2 */
        noise_frames = 2;                       /* cut after ~2 frames */
        return;
    }
    if (!has_adpcm)
        return;
    switch (type) {
    case 1: adpcm_play(adpcm_kick, ADPCM_KICK_LEN); break;
    case 2: adpcm_play(adpcm_snare, ADPCM_SNARE_LEN); break;
    case 4: adpcm_play(adpcm_crash, ADPCM_CRASH_LEN); break;
    default: break;
    }
}

void ym_chord_off(void)
{
    uint8_t ch;
    for (ch = 0; ch < 3; ch++)
        ym_write(0x08, ch);
}
