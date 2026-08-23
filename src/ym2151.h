#ifndef YM2151_H
#define YM2151_H

#include <stdint.h>

/*
 * MODRETRO Chromatic YM2151 FPGA expansion (design spec).
 * Unused GB I/O range FF28-FF2F is mapped to the YM2151 bridge:
 *
 *   FF28  YM2151 register address
 *   FF29  YM2151 register data
 *   FF2A  YM2151 status
 *   FF2E  Expansion ID      (0x51 when present)
 *   FF2F  Expansion version
 */
#define YM_REG      (*(volatile uint8_t *)0xFF28)
#define YM_DATA     (*(volatile uint8_t *)0xFF29)
#define YM_STATUS   (*(volatile uint8_t *)0xFF2A)
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

#endif
