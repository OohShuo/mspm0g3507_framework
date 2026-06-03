#include "hal.h"

#include "led_simple.h"

void Hal_Init(void) { Led_Simple_Init(); }

void Hal_Led_Loop(void) { Led_Simple_Update_All(); }