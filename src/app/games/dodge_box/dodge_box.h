#pragma once

#include <stdint.h>

#include "game_runtime.h"

void Dodge_Box_Init(const Game_hardware* hardware);
Game_result Dodge_Box_Update(const Game_input* input);
uint32_t Dodge_Box_Get_Score(void);
uint8_t Dodge_Box_Is_Finished(void);
