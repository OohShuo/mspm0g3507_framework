#pragma once

#include "game_runtime.h"

void Pong_Init(const Game_hardware* hardware);
Game_result Pong_Update(const Game_input* input);
uint32_t Pong_Get_Score(void);
uint8_t Pong_Is_Finished(void);
