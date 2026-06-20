#pragma once

#include <stdint.h>

#include "st7789.h"

void Game_Info_Screen_Draw(St7789* lcd, const char* name, const char* info_text, uint16_t fps);
