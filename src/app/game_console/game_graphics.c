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
    if (character >= '0' && character <= '9') { return g_digits[character - '0']; }
    return NULL;
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
