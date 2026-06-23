#pragma once

#include <stdint.h>

#include "buzzer.h"
#include "st7789.h"
#include "vib_motor_gpio.h"

typedef enum {
    game_direction_none,
    game_direction_up,
    game_direction_left,
    game_direction_down,
    game_direction_right,
} Game_direction;

typedef struct {
    float axis_x;
    float axis_y;
    uint8_t stick_active;

    Game_direction direction;
    uint8_t direction_pressed;

    uint8_t a_down;
    uint8_t a_pressed;
    uint8_t a_released;
    uint8_t b_down;
    uint8_t b_pressed;
    uint8_t b_released;
    uint8_t x_down;
    uint8_t x_pressed;
    uint8_t x_released;
    uint8_t y_down;
    uint8_t y_pressed;
    uint8_t y_released;
    uint8_t start_down;
    uint8_t start_pressed;
    uint8_t start_released;

    /* Compatibility aliases for existing game code. */
    uint8_t confirm_pressed;
    uint8_t back_requested;
} Game_input;

typedef struct {
    St7789* lcd;
    Buzzer* buzzer;
    Vib_motor_gpio* vib_motor;
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

uint32_t Game_Runtime_Get_Tick_Ms(void);
void Game_Runtime_Pause_Time(void);
void Game_Runtime_Resume_Time(void);
