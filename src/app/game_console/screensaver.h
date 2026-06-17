#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "st7789.h"

void Screensaver_Init(St7789* lcd);
void Screensaver_Run_Frame(void);
void Screensaver_Exit(void);
bool Screensaver_Is_Active(void);
