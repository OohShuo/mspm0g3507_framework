#pragma once

#include <stdint.h>

#include "game_runtime.h"

typedef enum {

#define game_entry(name) game_id_##name,
#include "game_entries.inc"
#undef game_entry

    game_id_count,
} Game_id;

typedef void (*Game_draw_icon)(St7789* lcd, int32_t x, int32_t y);

typedef struct {
    const char* name;
    Game_id id;
    const char* control_hint;
    const char* info_text;
    Game_draw_icon draw_icon;
    uint16_t name_color;
    void (*init)(const Game_hardware* hardware);
    Game_result (*update)(const Game_input* input);
    uint32_t (*get_score)(void);
    uint8_t is_game;
} Game_descriptor;

uint8_t Game_Registry_Count(void);
const Game_descriptor* Game_Registry_Get(uint8_t index);
