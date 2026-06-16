#pragma once

#include "game_runtime.h"

void Pacman_Init(const Game_hardware* hardware);
Game_result Pacman_Update(const Game_input* input);
uint32_t Pacman_Get_Score(void);
uint8_t Pacman_Is_Finished(void);
