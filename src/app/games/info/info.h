#pragma once

#include "game_runtime.h"

void Info_Init(const Game_hardware* hardware);
Game_result Info_Update(const Game_input* input);
uint32_t Info_Get_Score(void);
uint8_t Info_Is_Finished(void);
