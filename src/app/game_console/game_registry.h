#pragma once

#include <stdint.h>

#include "game_runtime.h"

typedef enum {
    game_icon_pacman,
    game_icon_snake,
    game_icon_racing,
} Game_icon;

typedef enum {
    game_id_pacman,
    game_id_snake,
    game_id_racing,
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
