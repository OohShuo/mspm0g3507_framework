#pragma once

#include "game_runtime.h"

void Volume_Control_Init(const Game_hardware* hardware);
Game_result Volume_Control_Update(const Game_input* input);
uint32_t Volume_Control_Get_Score(void);
