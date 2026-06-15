#pragma once

#include <stdint.h>
#include <SDL2/SDL.h>

void    Vm_Input_Init(void);
void    Vm_Input_Handle_Event(const SDL_Event* event);
float   Vm_Input_Get_X(void);
float   Vm_Input_Get_Y(void);
uint8_t Vm_Input_Get_Button(void);
int     Vm_Input_Quit_Requested(void);
