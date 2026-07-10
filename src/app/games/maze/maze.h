#pragma once

#include "game_runtime.h"

void Maze_Init(const Game_hardware* hardware);
Game_result Maze_Update(const Game_input* input);
uint32_t Maze_Get_Score(void);
