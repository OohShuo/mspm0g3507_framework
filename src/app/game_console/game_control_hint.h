#pragma once

#include <stdint.h>

#define GAME_CONTROL_HINT_VISIBLE_MAX 28u
#define GAME_CONTROL_HINT_TEXT_MAX    (GAME_CONTROL_HINT_VISIBLE_MAX + 1u)

void Game_Control_Hint_Format(const char* controls, uint8_t is_game, char output[GAME_CONTROL_HINT_TEXT_MAX]);
