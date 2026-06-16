#pragma once

#include <stdint.h>

void Vm_Display_Init(void);
void Vm_Display_Deinit(void);

void Vm_Display_Write_Pixels(
    int16_t x1, int16_t y1, int16_t x2, int16_t y2, const uint8_t* pixels, uint32_t pixel_count);
void Vm_Display_Frame_Done(void);
void Vm_Display_Render(void);
int Vm_Display_Is_Ready(void);
