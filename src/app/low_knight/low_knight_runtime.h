#pragma once

#include <stdint.h>

#include "low_knight_resources.h"
#include "st7789.h"

typedef struct {
    int16_t x;
    int16_t y;
} Low_Knight_Vec2i;

typedef struct {
    int8_t move_x;
    int8_t move_y;
    uint8_t jump_down;
    uint8_t jump_pressed;
    uint8_t jump_released;
    uint8_t strike_down;
    uint8_t strike_pressed;
    uint8_t up_pressed;
} Low_Knight_Input;

typedef enum {
    low_knight_step_none = 0,
    low_knight_step_dirty = 1,
    low_knight_step_full = 2,
    low_knight_step_transition = 3,
} Low_Knight_Step_Result;

uint8_t Low_Knight_Runtime_Init(Low_Knight_Resources* resources);
void Low_Knight_Runtime_Draw(St7789* lcd);
void Low_Knight_Runtime_Draw_Dirty(St7789* lcd);
Low_Knight_Step_Result Low_Knight_Runtime_Step(const Low_Knight_Input* input);
uint8_t Low_Knight_Runtime_Move_Player(int8_t dx, int8_t dy);
Low_Knight_Vec2i Low_Knight_Runtime_Get_Player(void);
