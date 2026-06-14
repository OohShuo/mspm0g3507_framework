#pragma once

#include <stdint.h>

void Score_Store_Init(uint8_t game_count);
uint8_t Score_Store_Is_Available(void);
uint32_t Score_Store_Get(uint8_t game_index);
void Score_Store_Observe(uint8_t game_index, uint32_t score);
void Score_Store_Commit(void);
