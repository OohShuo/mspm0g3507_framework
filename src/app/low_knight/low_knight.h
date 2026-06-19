#pragma once

#include <stdint.h>

#include "game_runtime.h"

void Low_Knight_Init(const Game_hardware* hardware);
Game_result Low_Knight_Update(const Game_input* input);
uint32_t Low_Knight_Get_Score(void);
uint8_t Low_Knight_Is_Finished(void);
