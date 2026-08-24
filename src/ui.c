#include <gb/gb.h>
#include <gb/cgb.h>
#include <gbdk/font.h>
#include <gbdk/console.h>
#include <stdio.h>
#include "ui.h"
#include "chord.h"
#include "wheel_gfx.h"
#include "badge_gfx.h"

/*
 * Layout (20x18 tiles), modeled on the Chromatic mockup:
 *
 *  row 0      KANTAN PLAY   YM2151|APU     (inverted bar)
 *  rows 2-12  8-petal chord wheel (11x11) with D-pad cross center
 *  rows 2-5   CHORD box (cols 12-19)
 *  rows 7-9   PROG: last-4-chords history + cursor
 *  row 17     A:PLAY  START:STOP           (inverted bar)
 */

/* ---- D-pad cross: 5 tiles, 3x3 tile block at wheel center ---- */
#define DPAD_TILE_BASE 128
#define T_UP     (DPAD_TILE_BASE + 0)
#define T_DOWN   (DPAD_TILE_BASE + 1)
#define T_LEFT   (DPAD_TILE_BASE + 2)
#define T_RIGHT  (DPAD_TILE_BASE + 3)
#define T_CENTER (DPAD_TILE_BASE + 4)

static const uint8_t dpad_tiles[] = {
    /* up arm: cap on top, arrow pointing up */
    0x3C,0x3C, 0x7E,0x42, 0x7E,0x5A, 0x7E,0x7E,
    0x7E,0x42, 0x7E,0x42, 0x7E,0x42, 0x7E,0x42,
    /* down arm */
    0x7E,0x42, 0x7E,0x42, 0x7E,0x42, 0x7E,0x42,
    0x7E,0x7E, 0x7E,0x5A, 0x7E,0x42, 0x3C,0x3C,
    /* left arm */
    0x00,0x00, 0x7F,0x7F, 0xFF,0x90, 0xFF,0xB0,
    0xFF,0xB0, 0xFF,0x90, 0x7F,0x7F, 0x00,0x00,
    /* right arm */
    0x00,0x00, 0xFE,0xFE, 0xFF,0x09, 0xFF,0x0D,
    0xFF,0x0D, 0xFF,0x09, 0xFE,0xFE, 0x00,0x00,
    /* center (with small dot) */
    0x7E,0x42, 0xFF,0xC3, 0xFF,0x00, 0xFF,0x18,
    0xFF,0x18, 0xFF,0x00, 0xFF,0xC3, 0x7E,0x42,
};

/* wheel placement on screen */
#define WHEEL_X 0
#define WHEEL_Y 2
#define CROSS_X 5   /* center tile of the wheel */
#define CROSS_Y 7

/* arm tile positions: up, down, left, right */
static const uint8_t arm_pos[4][2] = {
    { CROSS_X, CROSS_Y - 1 }, { CROSS_X, CROSS_Y + 1 },
    { CROSS_X - 1, CROSS_Y }, { CROSS_X + 1, CROSS_Y },
};

/* which arms light up per direction (bit0=up,1=down,2=left,3=right) */
static const uint8_t dir_arms[8] = {
    0x01, 0x09, 0x08, 0x0A, 0x02, 0x06, 0x04, 0x05,
};

/* chord name on the rim of the wheel: x, y, width */
static const uint8_t hl_rect[8][3] = {
    /* UP    */ { 5,  3, 1 },   /* C    */
    /* UR    */ { 8,  4, 2 },   /* Dm   */
    /* RIGHT */ { 9,  7, 2 },   /* F    */
    /* DR    */ { 8, 10, 2 },   /* Em   */
    /* DOWN  */ { 5, 12, 1 },   /* G    */
    /* DL    */ { 1, 10, 2 },   /* C7   */
    /* LEFT  */ { 0,  7, 2 },   /* Am   */
    /* UL    */ { 2,  4, 2 },   /* Bm   */
};

/* KANTAN Play degree badges (circled digits) on the inner ring around
   the D-pad cross; chord names sit out on the rim. C7 is I7 = 1. */
static const uint8_t badge_pos[8][2] = {
    /* UP    */ { 5, 5 },
    /* UR    */ { 7, 5 },
    /* RIGHT */ { 8, 7 },
    /* DR    */ { 7, 9 },
    /* DOWN  */ { 5, 10 },
    /* DL    */ { 3, 9 },
    /* LEFT  */ { 2, 7 },
    /* UL    */ { 3, 5 },
};
static const uint8_t badge_digit[8] = { 1, 2, 4, 3, 5, 1, 6, 7 };

/* Chromatic-style palettes */
static const palette_color_t bkg_palettes[] = {
    /* pal 0: normal — dark navy lines on lavender white */
    RGB(30, 30, 31), RGB(22, 21, 27), RGB(12, 11, 18), RGB(3, 2, 9),
    /* pal 1: selected — purple petal */
    RGB(22, 17, 31), RGB(17, 12, 27), RGB(10, 6, 18), RGB(3, 2, 9),
    /* pal 2: playing — green */
    RGB(16, 30, 16), RGB(11, 23, 11), RGB(5, 13, 5), RGB(0, 7, 0),
    /* pal 3: status bars — white text on dark purple */
    RGB(7, 4, 14), RGB(11, 8, 18), RGB(20, 18, 26), RGB(31, 31, 31),
};

static uint8_t is_cgb;
static uint8_t cur_dir = DIR_NONE;
static uint8_t cur_pal = 1;

/* PROG: last 8 chords in a 4x2 grid at (12,8)-(19,9) */
static uint8_t hist_n = 0;

static void set_attr_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t pal)
{
    uint8_t buf[20];
    uint8_t i;

    if (!is_cgb)
        return;
    for (i = 0; i < w; i++)
        buf[i] = pal;
    VBK_REG = 1;
    set_bkg_tiles(x, y, w, 1, buf);
    VBK_REG = 0;
}

static void set_arm_attrs(uint8_t dir, uint8_t pal)
{
    uint8_t mask, i;

    if (dir == DIR_NONE)
        return;
    mask = dir_arms[dir];
    for (i = 0; i < 4; i++)
        if (mask & (1 << i))
            set_attr_rect(arm_pos[i][0], arm_pos[i][1], 1, pal);
}

static void paint_dir(uint8_t dir, uint8_t pal)
{
    if (dir == DIR_NONE)
        return;
    set_attr_rect(hl_rect[dir][0], hl_rect[dir][1], hl_rect[dir][2], pal);
    set_attr_rect(badge_pos[dir][0], badge_pos[dir][1], 1, pal);
    set_arm_attrs(dir, pal);
}

void ui_init(uint8_t ym_present)
{
    uint8_t ty;

    is_cgb = (_cpu == CGB_TYPE);

    font_init();
    font_set(font_load(font_ibm));

    if (is_cgb)
        set_bkg_palette(0, 4, bkg_palettes);

    set_bkg_data(DPAD_TILE_BASE, 5, dpad_tiles);
    set_bkg_data(WHEEL_TILE_BASE, WHEEL_TILE_COUNT, wheel_tiles);
    set_bkg_data(BADGE_TILE_BASE, BADGE_TILE_COUNT, badge_tiles);

    /* status bars */
    gotoxy(0, 0);
    printf("KANTAN");
    ui_show_bpm(155);
    gotoxy(14, 0);
    printf(ym_present ? "YM2151" : "   APU");
    /* keep the last cell of the bottom row unwritten: filling column 19
       of row 17 makes the console scroll and lose row 0 */
    gotoxy(0, 17);
    printf("A:PLAY B:m ST:STOP");
    set_attr_rect(0, 0, 20, 3);
    set_attr_rect(0, 17, 20, 3);

    /* chord wheel + D-pad cross */
    for (ty = 0; ty < 11; ty++)
        set_bkg_tiles(WHEEL_X, WHEEL_Y + ty, 11, 1, &wheel_map[ty * 11]);
    {
        const uint8_t cross[3][3] = {
            { 0,       T_UP,     0        },
            { T_LEFT,  T_CENTER, T_RIGHT  },
            { 0,       T_DOWN,   0        },
        };
        set_bkg_tiles(CROSS_X, CROSS_Y - 1, 1, 1, &cross[0][1]);
        set_bkg_tiles(CROSS_X - 1, CROSS_Y, 3, 1, cross[1]);
        set_bkg_tiles(CROSS_X, CROSS_Y + 1, 1, 1, &cross[2][1]);
    }

    /* chord names on the rim */
    gotoxy(5, 3);   printf("C");
    gotoxy(8, 4);   printf("Dm");
    gotoxy(9, 7);   printf("F ");
    gotoxy(8, 10);  printf("Em");
    gotoxy(5, 12);  printf("G");
    gotoxy(1, 10);  printf("C7");
    gotoxy(0, 7);   printf("Am");
    gotoxy(2, 4);   printf("Bm");

    /* degree badges */
    for (ty = 0; ty < 8; ty++) {
        uint8_t t = BADGE_TILE_BASE + badge_digit[ty] - 1;
        set_bkg_tiles(badge_pos[ty][0], badge_pos[ty][1], 1, 1, &t);
    }

    /* CHORD box */
    gotoxy(12, 2);  printf("+------+");
    gotoxy(12, 3);  printf("|CHORD |");
    gotoxy(12, 4);  printf("|      |");
    gotoxy(12, 5);  printf("+------+");

    ui_show_oct(-2);

    /* progression history */
    gotoxy(12, 7);  printf("PROG:");

    SHOW_BKG;
    DISPLAY_ON;
}

void ui_highlight(uint8_t dir)
{
    if (dir == cur_dir)
        return;
    paint_dir(cur_dir, 0);
    cur_dir = dir;
    cur_pal = 1;
    paint_dir(dir, 1);
}

void ui_show_chord(const char *name, uint8_t playing)
{
    uint8_t n = 0;

    gotoxy(13, 4);
    printf("%s", name);
    while (name[n])
        n++;
    /* bass pedals on C: show slash-chord names like F/C */
    if (name[0] != 'C') {
        printf("/C");
        n += 2;
    }
    for (; n < 6; n++)
        printf(" ");
    if (cur_dir != DIR_NONE) {
        uint8_t pal = playing ? 2 : 1;
        if (pal != cur_pal) {
            cur_pal = pal;
            paint_dir(cur_dir, pal);
        }
    }
}

void ui_show_bpm(uint16_t bpm)
{
    gotoxy(7, 0);
    printf("BPM");
    if (bpm < 100)
        printf(" ");
    printf("%u", bpm);
}

/* GBDK's printf ignores field widths like %02X, so print hex by hand */
/* GBDK printf drops the second argument of "%c%c": print one at a time */
static void print_hex2(uint8_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    printf("%c", hex[v >> 4]);
    printf("%c", hex[v & 0x0F]);
}

void ui_show_adpcm(uint8_t status, uint8_t control, uint8_t version)
{
    /* keep each line <= 8 chars: column 19 is the screen edge and the
       console wraps overflow onto the next row */
    gotoxy(12, 14);
    printf("A");
    print_hex2(status);
    printf(" C");
    print_hex2(control);
    gotoxy(12, 15);
    printf("V");
    print_hex2(version);
    printf(" B%c ", (status & 0x80) ? '1' : '0');
}

void ui_show_oct(int8_t oct)
{
    gotoxy(12, 6);
    /* GBDK printf: %u must get a 16-bit argument (8-bit garbles) */
    printf("OCT:%c", (char)(oct < 0 ? '-' : '+'));
    printf("%u", (uint16_t)(oct < 0 ? -oct : oct));
}

void ui_reset_history(void)
{
    hist_n = 0;
    gotoxy(12, 8);
    printf("        ");
    gotoxy(12, 9);
    printf("        ");
}

void ui_push_history(const char *name)
{
    uint8_t slot;

    if (hist_n >= 8)
        ui_reset_history();
    slot = hist_n++;
    gotoxy(12 + (slot & 3) * 2, 8 + (slot >> 2));
    printf("%c", name[0]);
    printf("%c", name[1] ? name[1] : ' ');
}
