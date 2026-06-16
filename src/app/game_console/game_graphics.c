#include "game_graphics.h"

#include <stddef.h>

#define SCREEN_WIDTH 240

static uint16_t g_line_buffer[SCREEN_WIDTH];

static const uint8_t g_font[][5] = {
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, /* A */
    {0x7f, 0x49, 0x49, 0x49, 0x36}, /* B */
    {0x3e, 0x41, 0x41, 0x41, 0x22}, /* C */
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, /* D */
    {0x7f, 0x49, 0x49, 0x49, 0x41}, /* E */
    {0x7f, 0x09, 0x09, 0x09, 0x01}, /* F */
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, /* G */
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, /* H */
    {0x00, 0x41, 0x7f, 0x41, 0x00}, /* I */
    {0x20, 0x40, 0x41, 0x3f, 0x01}, /* J */
    {0x7f, 0x08, 0x14, 0x22, 0x41}, /* K */
    {0x7f, 0x40, 0x40, 0x40, 0x40}, /* L */
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, /* M */
    {0x7f, 0x04, 0x08, 0x10, 0x7f}, /* N */
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, /* O */
    {0x7f, 0x09, 0x09, 0x09, 0x06}, /* P */
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, /* Q */
    {0x7f, 0x09, 0x19, 0x29, 0x46}, /* R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* S */
    {0x01, 0x01, 0x7f, 0x01, 0x01}, /* T */
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, /* U */
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, /* V */
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, /* W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* X */
    {0x07, 0x08, 0x70, 0x08, 0x07}, /* Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* Z */
};

static const uint8_t g_font_lower[][5] = {
    {0x20, 0x54, 0x54, 0x54, 0x78}, /* a */
    {0x7f, 0x48, 0x48, 0x48, 0x30}, /* b */
    {0x38, 0x44, 0x44, 0x44, 0x28}, /* c */
    {0x30, 0x48, 0x48, 0x49, 0x7f}, /* d */
    {0x38, 0x54, 0x54, 0x54, 0x58}, /* e */
    {0x08, 0x7e, 0x09, 0x01, 0x02}, /* f */
    {0x18, 0xa4, 0xa4, 0xa8, 0x70}, /* g */
    {0x7f, 0x10, 0x08, 0x10, 0x60}, /* h */
    {0x00, 0x44, 0x7d, 0x40, 0x00}, /* i */
    {0x40, 0x80, 0x84, 0x7d, 0x00}, /* j */
    {0x7f, 0x10, 0x28, 0x44, 0x00}, /* k */
    {0x00, 0x41, 0x7f, 0x40, 0x00}, /* l */
    {0x7c, 0x04, 0x78, 0x04, 0x78}, /* m */
    {0x7c, 0x08, 0x04, 0x04, 0x78}, /* n */
    {0x38, 0x44, 0x44, 0x44, 0x38}, /* o */
    {0xfc, 0x24, 0x24, 0x24, 0x18}, /* p */
    {0x18, 0x24, 0x24, 0x24, 0xfc}, /* q */
    {0x7c, 0x08, 0x04, 0x04, 0x08}, /* r */
    {0x48, 0x54, 0x54, 0x54, 0x24}, /* s */
    {0x04, 0x3e, 0x44, 0x44, 0x24}, /* t */
    {0x3c, 0x40, 0x40, 0x40, 0x7c}, /* u */
    {0x1c, 0x20, 0x40, 0x20, 0x1c}, /* v */
    {0x3c, 0x40, 0x38, 0x40, 0x3c}, /* w */
    {0x44, 0x28, 0x10, 0x28, 0x44}, /* x */
    {0x1c, 0xa0, 0xa0, 0xa0, 0x7c}, /* y */
    {0x44, 0x64, 0x54, 0x4c, 0x44}, /* z */
};

static const uint8_t g_glyph_lt[5] = {0x00, 0x08, 0x14, 0x22, 0x41};         /* < */
static const uint8_t g_glyph_gt[5] = {0x41, 0x22, 0x14, 0x08, 0x00};         /* > */
static const uint8_t g_glyph_slash[5] = {0x10, 0x08, 0x04, 0x02, 0x01};      /* / */
static const uint8_t g_glyph_dot[5] = {0x00, 0x00, 0x40, 0x00, 0x00};        /* . */
static const uint8_t g_glyph_colon[5] = {0x00, 0x00, 0x44, 0x00, 0x00};      /* : */
static const uint8_t g_glyph_comma[5] = {0x00, 0x40, 0x30, 0x00, 0x00};      /* , */
static const uint8_t g_glyph_underscore[5] = {0x40, 0x40, 0x40, 0x40, 0x40}; /* _ */
static const uint8_t g_glyph_caret[5] = {0x04, 0x02, 0x01, 0x02, 0x04};      /* ^ */

static const uint8_t g_digits[][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e},
    {0x00, 0x42, 0x7f, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31},
    {0x18, 0x14, 0x12, 0x7f, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1e},
};

static const uint8_t* glyph_for(char character) {
    if (character >= 'A' && character <= 'Z') { return g_font[character - 'A']; }
    if (character >= 'a' && character <= 'z') { return g_font_lower[character - 'a']; }
    if (character >= '0' && character <= '9') { return g_digits[character - '0']; }
    switch (character) {
        case '<':
            return g_glyph_lt;
        case '>':
            return g_glyph_gt;
        case '/':
            return g_glyph_slash;
        case '.':
            return g_glyph_dot;
        case ':':
            return g_glyph_colon;
        case ',':
            return g_glyph_comma;
        case '_':
            return g_glyph_underscore;
        case '^':
            return g_glyph_caret;
        default:
            return NULL;
    }
}

uint16_t* Game_Graphics_Get_Line_Buffer(void) { return g_line_buffer; }

void Game_Graphics_Fill_Rect(
    St7789* lcd, int32_t x, int32_t y, int32_t width, int32_t height, uint16_t color) {
    if (lcd == NULL || width <= 0 || height <= 0 || width > SCREEN_WIDTH) { return; }

    for (int32_t col = 0; col < width; col++) { g_line_buffer[col] = color; }
    St7789_Begin_Write(lcd, x, y, x + width - 1, y + height - 1);
    for (int32_t row = 0; row < height; row++) {
        St7789_Write_Pixels(lcd, (uint8_t*)g_line_buffer, (uint32_t)width * sizeof(uint16_t));
    }
    St7789_End_Write(lcd);
}

static void draw_glyph(
    St7789* lcd, int32_t x, int32_t y, const uint8_t* glyph, uint8_t scale, uint16_t color) {
    if (glyph == NULL || scale == 0) { return; }

    for (int32_t column = 0; column < 5; column++) {
        for (int32_t row = 0; row < 7; row++) {
            if ((glyph[column] & (1u << row)) != 0) {
                Game_Graphics_Fill_Rect(lcd, x + column * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void Game_Graphics_Draw_Text(
    St7789* lcd, int32_t x, int32_t y, const char* text, uint8_t scale, uint16_t color) {
    if (text == NULL || scale == 0) { return; }

    while (*text != '\0') {
        if (*text == '-') {
            Game_Graphics_Fill_Rect(lcd, x, y + 3 * scale, 5 * scale, scale, color);
        } else {
            draw_glyph(lcd, x, y, glyph_for(*text), scale, color);
        }
        x += 6 * scale;
        text++;
    }
}

void Game_Graphics_Draw_U32(
    St7789* lcd, int32_t x, int32_t y, uint32_t value, uint8_t digits, uint8_t scale, uint16_t color) {
    uint32_t divisor = 1;
    for (uint8_t i = 1; i < digits; i++) { divisor *= 10u; }

    for (uint8_t i = 0; i < digits; i++) {
        const uint8_t digit = (uint8_t)((value / divisor) % 10u);
        draw_glyph(lcd, x, y, g_digits[digit], scale, color);
        x += 6 * scale;
        if (divisor > 1u) { divisor /= 10u; }
    }
}

void Game_Graphics_Draw_Bitmap(
    St7789* lcd, int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
    if (lcd == NULL || data == NULL || w <= 0 || h <= 0) { return; }
    St7789_Begin_Write(lcd, x, y, x + w - 1, y + h - 1);
    for (int32_t row = 0; row < h; row++) {
        St7789_Write_Pixels(lcd, (uint8_t*)(data + row * w), (uint32_t)w * sizeof(uint16_t));
    }
    St7789_End_Write(lcd);
}

/* ── 4-bit grayscale (16 levels) → RGB565 lookup ── */
static const uint16_t g_gray4_lut[16] = {
    0x0000u, /*  0 — black */
    0x1082u, /*  1 */
    0x2104u, /*  2 */
    0x31a6u, /*  3 */
    0x4228u, /*  4 */
    0x52aau, /*  5 */
    0x632cu, /*  6 */
    0x73aeu, /*  7 */
    0x8c51u, /*  8 */
    0x9cd3u, /*  9 */
    0xad55u, /* 10 */
    0xbdd7u, /* 11 */
    0xce59u, /* 12 */
    0xdefbu, /* 13 */
    0xef7du, /* 14 */
    0xffffu, /* 15 — white */
};

void Game_Graphics_Draw_Gray4_Bitmap(
    St7789* lcd, int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t* data) {
    if (lcd == NULL || data == NULL || w <= 0 || h <= 0) { return; }
    if (w > SCREEN_WIDTH) { return; }

    St7789_Begin_Write(lcd, x, y, x + w - 1, y + h - 1);

    const int32_t row_bytes = (w + 1) / 2; /* 2 pixels per byte, round up */

    for (int32_t row = 0; row < h; row++) {
        const uint8_t* src = data + (int32_t)row * row_bytes;
        uint16_t* dst = g_line_buffer;
        int32_t remaining = w;

        while (remaining >= 2) {
            const uint8_t byte = *src++;
            *dst++ = g_gray4_lut[byte >> 4];    /* high nibble → first pixel */
            *dst++ = g_gray4_lut[byte & 0x0Fu]; /* low nibble  → second pixel */
            remaining -= 2;
        }
        if (remaining > 0) {
            /* Odd-width image: last byte has only one valid pixel (high nibble) */
            *dst++ = g_gray4_lut[*src >> 4];
        }

        St7789_Write_Pixels(lcd, (uint8_t*)g_line_buffer, (uint32_t)w * sizeof(uint16_t));
    }

    St7789_End_Write(lcd);
}
