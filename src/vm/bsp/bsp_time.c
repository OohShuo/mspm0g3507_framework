#include "bsp_time.h"
#include <SDL2/SDL.h>
uint32_t Bsp_Get_Tick_Ms(void) { return (uint32_t)SDL_GetTicks(); }
