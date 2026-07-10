#pragma once

#include "game_runtime.h"

void Tetris_Init(const Game_hardware* hardware);
Game_result Tetris_Update(const Game_input* input);
uint32_t Tetris_Get_Score(void);
