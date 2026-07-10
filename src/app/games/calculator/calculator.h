#pragma once

#include "game_runtime.h"

void Calc_Init(const Game_hardware* hardware);
Game_result Calc_Update(const Game_input* input);
uint32_t Calc_Get_Score(void);
