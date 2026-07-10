#pragma once

#include "game_runtime.h"

void Dino_Runner_Init(const Game_hardware* hardware);
Game_result Dino_Runner_Update(const Game_input* input);
uint32_t Dino_Runner_Get_Score(void);
