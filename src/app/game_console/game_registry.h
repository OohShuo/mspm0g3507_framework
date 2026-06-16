#pragma once

#include <stdint.h>

#include "game_runtime.h"

typedef enum {
    game_icon_pacman,
    game_icon_snake,
    game_icon_racing,
    game_icon_tank,
    game_icon_air,
    game_icon_tetris,
    game_icon_breakout,
    game_icon_pong,
    game_icon_gomoku,
    game_icon_2048,
    game_icon_dino,
    game_icon_flappy,
    game_icon_maze,
    game_icon_needle,
    game_icon_fps_test,
    game_icon_info,
} Game_icon;

typedef enum {
    game_id_pacman,
    game_id_snake,
    game_id_racing,
    game_id_tank,
    game_id_air,
    game_id_tetris,
    game_id_breakout,
    game_id_pong,
    game_id_gomoku,
    game_id_2048,
    game_id_dino,
    game_id_flappy,
    game_id_maze,
    game_id_needle,
    game_id_fps_test,
    game_id_info,
    game_id_count,
} Game_id;

typedef struct {
    const char* name;
    Game_icon icon;
    Game_id id;
    void (*init)(const Game_hardware* hardware);
    Game_result (*update)(const Game_input* input);
    uint32_t (*get_score)(void);
    uint8_t (*is_finished)(void);
} Game_descriptor;

uint8_t Game_Registry_Count(void);
const Game_descriptor* Game_Registry_Get(uint8_t index);
