#include <gb/gb.h>
#include "chord.h"

/*
 * Key = C major. Diatonic chords assigned to the 8 D-pad directions:
 *
 *              C
 *        Bm       Dm
 *     Am     [+]     Em
 *        C7       F
 *              G
 *
 * Note values are the REAL KANTAN Music API output (Close voicing,
 * key=0, pitches 4-6), dumped from the bundled m1mac library.
 * Per KANTAN, degree 7 defaults to Bm (dim is a modifier).
 * C7 = degree 1 with Modifier_7.
 *
 * Ordering: the APU path can only play note[1] and note[2] (two pulse
 * channels), so each entry is arranged as { extra, root-ish, 3rd-ish }:
 * the character notes always sit in [1]/[2], the doubled/5th in [0].
 * The YM path plays all three, order doesn't matter there.
 */
const Chord chords[8] = {
    /* UP    */ { "C",    { 55, 60, 64 } },   /* G  | C  E  */
    /* UR    */ { "Dm",   { 57, 62, 65 } },   /* A  | D  F  */
    /* RIGHT */ { "F",    { 60, 65, 57 } },   /* C  | F  A  */
    /* DR    */ { "Em",   { 59, 64, 55 } },   /* B  | E  G  */
    /* DOWN  */ { "G",    { 62, 55, 59 } },   /* D  | G  B  */
    /* DL    */ { "C7",   { 55, 58, 64 } },   /* G  | Bb E  */
    /* LEFT  */ { "Am",   { 64, 57, 60 } },   /* E  | A  C  */
    /* UL    */ { "Bm",   { 66, 59, 62 } },   /* F# | B  D  */
};

/* minor_swap = true, same source */
const Chord chords_swap[8] = {
    /* UP    */ { "Cm",   { 55, 60, 63 } },   /* G  | C  Eb */
    /* UR    */ { "D",    { 57, 62, 66 } },   /* A  | D  F# */
    /* RIGHT */ { "Fm",   { 60, 65, 56 } },   /* C  | F  Ab */
    /* DR    */ { "E",    { 59, 64, 56 } },   /* B  | E  G# */
    /* DOWN  */ { "Gm",   { 62, 55, 58 } },   /* D  | G  Bb */
    /* DL    */ { "Cm7",  { 55, 58, 63 } },   /* G  | Bb Eb */
    /* LEFT  */ { "A",    { 64, 57, 61 } },   /* E  | A  C# */
    /* UL    */ { "B",    { 66, 59, 63 } },   /* F# | B  D# */
};

uint8_t dir_from_joypad(uint8_t j)
{
    uint8_t d = j & (J_UP | J_DOWN | J_LEFT | J_RIGHT);

    switch (d) {
    case J_UP:              return DIR_UP;
    case J_UP | J_RIGHT:    return DIR_UR;
    case J_RIGHT:           return DIR_RIGHT;
    case J_DOWN | J_RIGHT:  return DIR_DR;
    case J_DOWN:            return DIR_DOWN;
    case J_DOWN | J_LEFT:   return DIR_DL;
    case J_LEFT:            return DIR_LEFT;
    case J_UP | J_LEFT:     return DIR_UL;
    default:                return DIR_NONE;
    }
}
