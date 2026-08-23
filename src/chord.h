#ifndef CHORD_H
#define CHORD_H

#include <stdint.h>

/* 8 directions, clockwise from UP */
#define DIR_UP    0
#define DIR_UR    1
#define DIR_RIGHT 2
#define DIR_DR    3
#define DIR_DOWN  4
#define DIR_DL    5
#define DIR_LEFT  6
#define DIR_UL    7
#define DIR_NONE  0xFF

typedef struct {
    const char *name;
    uint8_t note[3];    /* KANTAN Close voicing, pitches 4-6 (MIDI) */
} Chord;

extern const Chord chords[8];
extern const Chord chords_swap[8];  /* B held: major/minor swapped */

/* joypad D-pad bits -> direction (DIR_NONE if no D-pad input) */
uint8_t dir_from_joypad(uint8_t j);

#endif
