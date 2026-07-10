#pragma once

#include <stdint.h>

#include "game_runtime.h"

void Bad_Apple_Init(const Game_hardware* hardware);
Game_result Bad_Apple_Update(const Game_input* input);
uint32_t Bad_Apple_Get_Score(void);
