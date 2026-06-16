#pragma once

#include "game_runtime.h"

void Fps_Test_Init(const Game_hardware* hardware);
Game_result Fps_Test_Update(const Game_input* input);
uint32_t Fps_Test_Get_Score(void);
uint8_t Fps_Test_Is_Finished(void);
