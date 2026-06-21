#pragma once

#include <SDL2/SDL.h>
#include <stdint.h>

void Vm_Haptics_Init(void);
void Vm_Haptics_Deinit(void);
void Vm_Haptics_Handle_Event(const SDL_Event* event);
void Vm_Haptics_Update(void);
void Vm_Haptics_Set_Strength(uint8_t strength_percent);
uint8_t Vm_Haptics_Get_Strength(void);
