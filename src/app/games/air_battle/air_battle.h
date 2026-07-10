#pragma once

#include "game_runtime.h"

void Air_Battle_Init(const Game_hardware* hardware);
Game_result Air_Battle_Update(const Game_input* input);
uint32_t Air_Battle_Get_Score(void);
