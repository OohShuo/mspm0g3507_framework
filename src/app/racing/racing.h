#pragma once

#include "game_runtime.h"

void Racing_Init(const Game_hardware* hardware);
Game_result Racing_Update(const Game_input* input);
uint32_t Racing_Get_Score(void);
uint8_t Racing_Is_Finished(void);
