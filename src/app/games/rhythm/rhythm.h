#pragma once

#include "game_runtime.h"

void Rhythm_Init(const Game_hardware* hardware);
Game_result Rhythm_Update(const Game_input* input);
uint32_t Rhythm_Get_Score(void);
