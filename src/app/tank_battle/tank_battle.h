#pragma once

#include "game_runtime.h"

void Tank_Battle_Init(const Game_hardware* hardware);
Game_result Tank_Battle_Update(const Game_input* input);
uint32_t Tank_Battle_Get_Score(void);
uint8_t Tank_Battle_Is_Finished(void);
