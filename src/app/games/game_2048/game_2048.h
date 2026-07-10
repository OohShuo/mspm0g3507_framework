#pragma once

#include "game_runtime.h"

void Game_2048_Init(const Game_hardware* hardware);
Game_result Game_2048_Update(const Game_input* input);
uint32_t Game_2048_Get_Score(void);
