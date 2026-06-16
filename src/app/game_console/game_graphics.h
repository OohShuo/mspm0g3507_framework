#pragma once

#include <stdint.h>

#include "st7789.h"

uint16_t* Game_Graphics_Get_Line_Buffer(void);
void Game_Graphics_Fill_Rect(
    St7789* lcd, int32_t x, int32_t y, int32_t width, int32_t height, uint16_t color);
void Game_Graphics_Draw_Text(
    St7789* lcd, int32_t x, int32_t y, const char* text, uint8_t scale, uint16_t color);
void Game_Graphics_Draw_U32(
    St7789* lcd, int32_t x, int32_t y, uint32_t value, uint8_t digits, uint8_t scale, uint16_t color);
void Game_Graphics_Draw_Bitmap(St7789* lcd, int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data);
void Game_Graphics_Draw_Gray4_Bitmap(
    St7789* lcd, int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t* data);
