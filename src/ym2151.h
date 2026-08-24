#ifndef YM2151_H
#define YM2151_H

#include <stdint.h>

/*
 * MODRETRO Chromatic YM2151 FPGA expansion (design spec).
 * Unused GB I/O range FF28-FF2F is mapped to the YM2151 bridge:
 *
 *   FF28  YM2151 register address (plain OPM, no escapes)
 *   FF29  YM2151 register data
 *   FF2A  read:  bit7 YM/bridge busy, bit6 ADPCM FIFO ready,
 *                bit5 ADPCM playback busy
 *         write: ADPCM sample data (dedicated FIFO port)
 *   FF2B  bit7-4 ADPCM volume, bit3 ADPCM play (0->1 starts, 1->0
 *         stops and clears the FIFO), bit2 ADPCM output enable,
 *         bit1 GB APU enable, bit0 YM enable
 *   FF2E  Expansion ID      (0x51 when present)
 *   FF2F  Expansion version (>= 0x04: no-escape register map)
 *
 * All 256 OPM registers are reachable; CH7/C2 is used for native YM2151
 * noise. ADPCM is 1ch, 4-bit, 15.625 kHz.
 */
#define YM_REG      (*(volatile uint8_t *)0xFF28)
#define YM_DATA     (*(volatile uint8_t *)0xFF29)
#define YM_STATUS   (*(volatile uint8_t *)0xFF2A)   /* read  */
#define ADPCM_DATA  (*(volatile uint8_t *)0xFF2A)   /* write */
#define ADPCM_CTRL  (*(volatile uint8_t *)0xFF2B)
#define EXT_ID      (*(volatile uint8_t *)0xFF2E)
#define EXT_VERSION (*(volatile uint8_t *)0xFF2F)

#define YM_EXT_ID_VALUE 0x51

/* Returns 1 if the Chromatic YM2151 extension is present */
uint8_t ym_detect(void);

void ym_init(void);
/* Play a 3-note chord on YM channels 0-2. Notes are MIDI note numbers. */
void ym_chord_on(const uint8_t notes[3]);
/* Trigger one bass note on YM channel 4 */
void ym_bass_note(uint8_t note);
void ym_bass_off(void);
void ym_chord_off(void);    /* chord channels 0-2 only */
/* rhythm: kick/snare/crash use ADPCM; hi-hat uses YM2151 CH7 noise */
void ym_drum(uint8_t type);
/* call once per frame: streams ADPCM sample data to the FIFO */
void ym_adpcm_tick(void);

#endif
