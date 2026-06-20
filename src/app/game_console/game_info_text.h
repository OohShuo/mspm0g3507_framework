#pragma once

#include <stdint.h>

#define GAME_INFO_LINE_MAX         34u
#define GAME_INFO_LINE_BUFFER_SIZE (GAME_INFO_LINE_MAX + 1u)

uint8_t Game_Info_Text_Next_Line(
    const char** cursor, char line[GAME_INFO_LINE_BUFFER_SIZE]);
