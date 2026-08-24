#include <gb/gb.h>
#include "chord.h"
#include "sound.h"
#include "ym2151.h"
#include "ui.h"

/*
 * KANTAN GB PLAY — chord instrument for Game Boy Color.
 *
 * D-pad (8 directions) selects a diatonic chord in C major,
 * A button plays it. Uses the Chromatic YM2151 FPGA extension
 * when detected, otherwise a 3-voice chord on the GB APU.
 */

void main(void)
{
    uint8_t ym = ym_detect();
    uint8_t sel = DIR_UP;       /* latched selection */
    uint8_t playing = 0;
    uint8_t prev_j = 0;
    uint16_t bpm = 140;
    uint8_t step_period = 13;   /* frames per 8th note = round(1800 / BPM) */
    uint8_t step_timer = 0;
    uint8_t step = 0;           /* 8th-note step in the bar (0-7) */

    /* Rhythm per the X68000 MDX: four-on-the-floor kicks, and every
       8th bar a fill (kick, snare, kick 16ths, snare, crash) */
    static const uint8_t drum_main[8] = {
        DRUM_KICK, DRUM_HAT, DRUM_KICK, DRUM_HAT,
        DRUM_KICK, DRUM_HAT, DRUM_KICK, DRUM_HAT,
    };
    static const uint8_t drum_fill[8] = {
        DRUM_KICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT,
        DRUM_KICK, DRUM_KICK, DRUM_SNARE, DRUM_CRASH,
    };
    uint8_t bar = 0;

    /* Demo (SELECT+A): Space Harrier main theme progression, one entry
       per half bar. Derived from the X68000 MDX analysis:
       (C F Fm C) x3, then F Dm Fm C as the turnaround. {dir, swap} */
    static const uint8_t demo_seq[32][2] = {
        { DIR_UP, 0 }, { DIR_UP, 0 }, { DIR_RIGHT, 0 }, { DIR_RIGHT, 0 },
        { DIR_RIGHT, 0 }, { DIR_RIGHT, 1 }, { DIR_UP, 0 }, { DIR_UP, 0 },
        { DIR_UP, 0 }, { DIR_UP, 0 }, { DIR_RIGHT, 0 }, { DIR_RIGHT, 0 },
        { DIR_RIGHT, 0 }, { DIR_RIGHT, 1 }, { DIR_UP, 0 }, { DIR_UP, 0 },
        { DIR_UP, 0 }, { DIR_UP, 0 }, { DIR_RIGHT, 0 }, { DIR_RIGHT, 0 },
        { DIR_RIGHT, 0 }, { DIR_RIGHT, 1 }, { DIR_UP, 0 }, { DIR_UP, 0 },
        { DIR_UP, 0 }, { DIR_UP, 0 }, { DIR_RIGHT, 0 }, { DIR_RIGHT, 0 },
        { DIR_UR, 0 }, { DIR_RIGHT, 1 }, { DIR_UP, 0 }, { DIR_UP, 0 },
    };
    uint8_t demo = 0;
    uint8_t demo_idx = 0;

#define STEP_PAR (step & 1)
    uint8_t pending_off = 0;    /* key-off waits for the next grid step */
    int8_t octv = 0;            /* octave shift, -3..+4 (SELECT + left/right) */
    int8_t oct_off = 0;         /* octv * 12 */
    uint8_t j = 0;
    uint8_t last_raw = 0;
    uint8_t accomp = 0;         /* bass + rhythm running */
    const Chord *cur;

    apu_init();
    if (ym)
        ym_init();

    ui_init(ym);
    ui_highlight(sel);
    ui_show_chord(chords[sel].name, 0);

    while (1) {
        /* debounce: accept a reading only when stable for 2 frames */
        uint8_t raw = joypad();
        uint8_t dir;
        if (raw == last_raw)
            j = raw;
        last_raw = raw;
        dir = dir_from_joypad(j);
        uint8_t retrig = 0;

        /* B held: major/minor swap */
        cur = (j & J_B) ? &chords_swap[sel] : &chords[sel];

        /* SELECT held: up/down = BPM, left/right = octave, A = demo */
        if (j & J_SELECT) {
            uint16_t nb;
            if ((j & J_A) && !(prev_j & J_A)) {
                demo ^= 1;
                if (demo) {
                    /* Space Harrier tempo (@t225 = 157.5) */
                    bpm = 155;
                    step_period = (uint8_t)((1800 + bpm / 2) / bpm);
                    ui_show_bpm(bpm);
                    demo_idx = 0;
                    pending_off = 0;
                    step = 7;
                    step_timer = step_period - 1;
                } else {
                    if (ym) {
                        ym_chord_off();
                        ym_bass_off();
                    } else {
                        apu_chord_off();
                        apu_bass_off();
                    }
                    playing = 0;
                    accomp = 0;
                    ui_show_chord(chords[sel].name, 0);
                }
            }
            nb = bpm;   /* after the demo toggle, which sets its own BPM */
            if ((j & J_UP) && !(prev_j & J_UP) && nb < 240)
                nb += 5;
            if ((j & J_DOWN) && !(prev_j & J_DOWN) && nb > 40)
                nb -= 5;
            if (nb != bpm) {
                bpm = nb;
                step_period = (uint8_t)((1800 + bpm / 2) / bpm);
                ui_show_bpm(bpm);
            }
            if ((j & J_RIGHT) && !(prev_j & J_RIGHT) && octv < 4) {
                octv++;
                oct_off = octv * 12;
                ui_show_oct(octv);
            }
            if ((j & J_LEFT) && !(prev_j & J_LEFT) && octv > -3) {
                octv--;
                oct_off = octv * 12;
                ui_show_oct(octv);
            }
            dir = DIR_NONE;
        }

        /* new direction latches a chord (A/B plays it) */
        if (!demo && dir != DIR_NONE && dir != sel) {
            sel = dir;
            cur = (j & J_B) ? &chords_swap[sel] : &chords[sel];
            ui_highlight(sel);
            ui_show_chord(cur->name, playing);
            retrig = playing;
        }

        /* B toggled while still holding a play button: swap retrigger.
           A lone B release must NOT retrigger — just let key-off happen. */
        if (!demo && (j & J_B) != (prev_j & J_B) &&
            (j & (J_A | J_B)) && (prev_j & (J_A | J_B))) {
            ui_show_chord(cur->name, playing);
            retrig |= playing;
        }

        /* A or B pressed: note on (B plays the swapped chord) */
        if (!demo && !(j & J_SELECT) &&
            (j & (J_A | J_B)) && !(prev_j & (J_A | J_B))) {
            retrig = 1;
            playing = 1;
            pending_off = 0;
            ui_show_chord(cur->name, 1);
            ui_push_history(cur->name);
        }

        if (retrig) {
            uint8_t notes[3];
            uint8_t i;
            for (i = 0; i < 3; i++)
                notes[i] = cur->note[i] + oct_off;
            if (ym)
                ym_chord_on(notes);
            else
                apu_chord_on(notes);
        }

        /* accompaniment (bass + rhythm) runs while the D-pad is held or a
           chord is playing; SELECT gestures don't start it */
        {
            uint8_t acc = playing || demo;
            if (!(j & J_SELECT) &&
                (j & (J_UP | J_DOWN | J_LEFT | J_RIGHT)))
                acc = 1;
            if (acc && !accomp) {
                /* on-chord: the bass pedals on C (X/C) */
                uint8_t bass = 60 + oct_off - 24;
                if (ym)
                    ym_bass_note(bass);
                else
                    apu_bass_note(bass);
            }
            accomp = acc;
        }

        /* free-running 8th-note grid: bass, rhythm and key-off are all
           quantized to it, so nothing ever breaks the groove */
        if (++step_timer >= step_period) {
            step_timer = 0;
            step = (step + 1) & 7;
            if (step == 0)
                bar++;
            if (pending_off) {
                /* quantized chord key-off */
                pending_off = 0;
                if (playing) {
                    if (ym)
                        ym_chord_off();
                    else
                        apu_chord_off();
                    playing = 0;
                    ui_show_chord(cur->name, 0);
                }
            }
            /* demo: next chord on every half-bar boundary */
            if (demo && (step & 3) == 0) {
                uint8_t notes[3];
                uint8_t i;
                uint8_t d = demo_seq[demo_idx][0];
                uint8_t sw = demo_seq[demo_idx][1];
                demo_idx = (demo_idx + 1) & 31;
                cur = sw ? &chords_swap[d] : &chords[d];
                if (d != sel || !playing) {
                    sel = d;
                    ui_highlight(sel);
                    ui_push_history(cur->name);
                }
                ui_show_chord(cur->name, 1);
                playing = 1;
                for (i = 0; i < 3; i++)
                    notes[i] = cur->note[i] + oct_off;
                if (ym)
                    ym_chord_on(notes);
                else
                    apu_chord_on(notes);
            }
            /* keep going while the D-pad is held */
            accomp = demo || playing ||
                     (!(j & J_SELECT) &&
                      (j & (J_UP | J_DOWN | J_LEFT | J_RIGHT)));
            if (accomp) {
                /* 8th-note octave bass C2<->C3 (the MDX uses C1<->C2,
                   too low for a small speaker) */
                uint8_t note = 60 + oct_off - (STEP_PAR ? 12 : 24);
                uint8_t hit = ((bar & 7) == 7 ? drum_fill : drum_main)[step];
                if (ym)
                    ym_drum(hit);
                else
                    apu_drum(hit);
                if (ym)
                    ym_bass_note(note);
                else
                    apu_bass_note(note);
            } else {
                if (ym)
                    ym_bass_off();
                else
                    apu_bass_off();
            }
        }

        /* START: stop / panic (immediate, not quantized) */
        if ((j & J_START) && !(prev_j & J_START)) {
            if (ym) {
                ym_chord_off();
                ym_bass_off();
            } else {
                apu_chord_off();
                apu_bass_off();
            }
            playing = 0;
            pending_off = 0;
            accomp = 0;
            demo = 0;
            ui_show_chord(chords[sel].name, 0);
        }

        /* both A and B released: key off on the next grid step */
        if (!demo && !(j & (J_A | J_B)) && (prev_j & (J_A | J_B)))
            pending_off = 1;

        prev_j = j;
        if (ym)
            ym_adpcm_tick();    /* keep the ADPCM FIFO fed */
        vsync();
    }
}
