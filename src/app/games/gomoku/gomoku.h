#pragma once

#include "game_runtime.h"

void Gomoku_Init(const Game_hardware* hardware);
Game_result Gomoku_Update(const Game_input* input);
uint32_t Gomoku_Get_Score(void);
