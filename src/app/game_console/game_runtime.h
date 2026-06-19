#pragma once

#include <stdint.h>

#include "buzzer.h"
#include "st7789.h"

typedef enum {
    game_direction_none,
    game_direction_up,
    game_direction_left,
    game_direction_down,
    game_direction_right,
} Game_direction;

typedef struct {
    Game_direction direction;
    uint8_t direction_pressed;
    uint8_t confirm_pressed;
    uint8_t back_requested;
} Game_input;

typedef struct {
    St7789* lcd;
    Buzzer* buzzer;
} Game_hardware;

typedef enum {
    game_result_running,
    game_result_exit,
} Game_result;

/* ── Unified screen layout ── */
#define GAME_TOP_BAR_H    30
#define GAME_BOTTOM_BAR_H 20
#define GAME_AREA_Y       GAME_TOP_BAR_H
#define GAME_AREA_BOTTOM  300
#define GAME_AREA_H       (GAME_AREA_BOTTOM - GAME_AREA_Y)
