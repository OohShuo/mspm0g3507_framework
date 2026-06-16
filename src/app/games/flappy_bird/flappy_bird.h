#pragma once

#include "game_runtime.h"

void Flappy_Bird_Init(const Game_hardware* hardware);
Game_result Flappy_Bird_Update(const Game_input* input);
uint32_t Flappy_Bird_Get_Score(void);
uint8_t Flappy_Bird_Is_Finished(void);
