#pragma once

#include <stdint.h>

#include "buzzer.h"
#include "game_runtime.h"
#include "st7789.h"

typedef enum {
    game_over_action_none,
    game_over_action_replay,
    game_over_action_menu,
} Game_over_action;

void Game_Over_Menu_Open(St7789* lcd, Buzzer* buzzer, uint8_t game_id, const char* game_name, uint32_t score);
Game_over_action Game_Over_Menu_Update(const Game_input* input);
void Game_Over_Menu_Redraw(void);
