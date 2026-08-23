#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

void apu_init(void);
/* Chord voices on CH1/CH2 (3rd + 5th/7th). Notes are MIDI note numbers. */
void apu_chord_on(const uint8_t notes[3]);
/* Trigger one bass note on CH3 (triangle). Pass the actual MIDI note. */
void apu_bass_note(uint8_t note);
void apu_bass_off(void);
void apu_chord_off(void);   /* chord voices (CH1/CH2) only */

/* rhythm on the noise channel (CH4) */
#define DRUM_NONE  0
#define DRUM_KICK  1
#define DRUM_SNARE 2
#define DRUM_HAT   3
#define DRUM_CRASH 4
void apu_drum(uint8_t type);

#endif
