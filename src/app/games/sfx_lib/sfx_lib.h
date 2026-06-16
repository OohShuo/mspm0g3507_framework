#pragma once

#include "game_runtime.h"

void Sfx_Lib_Init(const Game_hardware* hardware);
Game_result Sfx_Lib_Update(const Game_input* input);
uint32_t Sfx_Lib_Get_Score(void);
uint8_t Sfx_Lib_Is_Finished(void);
