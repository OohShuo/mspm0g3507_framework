#pragma once

#include <stdint.h>

#include "low_knight_resources.h"
#include "st7789.h"

typedef struct {
    int16_t x;
    int16_t y;
} Low_Knight_Vec2i;

uint8_t Low_Knight_Runtime_Init(Low_Knight_Resources* resources);
void Low_Knight_Runtime_Draw(St7789* lcd);
void Low_Knight_Runtime_Draw_Dirty(St7789* lcd);
uint8_t Low_Knight_Runtime_Move_Player(int8_t dx, int8_t dy);
Low_Knight_Vec2i Low_Knight_Runtime_Get_Player(void);
