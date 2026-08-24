#ifndef UI_H
#define UI_H

#include <stdint.h>

void ui_init(uint8_t ym_present);
void ui_highlight(uint8_t dir);             /* DIR_NONE clears */
void ui_show_chord(const char *name, uint8_t playing);
void ui_push_history(const char *name);     /* PROG row, last 4 chords */
void ui_show_bpm(uint16_t bpm);
void ui_show_oct(int8_t oct);
void ui_show_adpcm(uint8_t status, uint8_t control, uint8_t version);

#endif
