#pragma once

#include "game_runtime.h"

void Snake_Init(const Game_hardware* hardware);
Game_result Snake_Update(const Game_input* input);
uint32_t Snake_Get_Score(void);
uint8_t Snake_Is_Finished(void);
