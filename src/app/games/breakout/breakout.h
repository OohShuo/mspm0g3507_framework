#pragma once

#include "game_runtime.h"

void Breakout_Init(const Game_hardware* hardware);
Game_result Breakout_Update(const Game_input* input);
uint32_t Breakout_Get_Score(void);
