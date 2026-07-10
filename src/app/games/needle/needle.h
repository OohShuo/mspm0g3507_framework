#pragma once

#include "game_runtime.h"

void Needle_Init(const Game_hardware* hardware);
Game_result Needle_Update(const Game_input* input);
uint32_t Needle_Get_Score(void);
