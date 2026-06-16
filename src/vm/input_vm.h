#pragma once

#include <stdint.h>

void Vm_Input_Init(void);

// Called from main thread after SDL_PollEvent — snapshots keyboard state.
// Only this function touches SDL APIs.
void Vm_Input_Poll(void);

// Thread-safe: read the snapshot, can be called from any thread.
float Vm_Input_Get_X(void);
float Vm_Input_Get_Y(void);
uint8_t Vm_Input_Get_Button(void);
int Vm_Input_Quit_Requested(void);
