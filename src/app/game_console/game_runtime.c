#include "game_runtime.h"

#include "bsp_time.h"

static uint32_t g_paused_at;
static uint32_t g_paused_total;
static uint8_t g_time_paused;

uint32_t Game_Runtime_Get_Tick_Ms(void) {
    const uint32_t now = g_time_paused ? g_paused_at : Bsp_Get_Tick_Ms();
    return now - g_paused_total;
}

void Game_Runtime_Pause_Time(void) {
    if (g_time_paused) { return; }
    g_paused_at = Bsp_Get_Tick_Ms();
    g_time_paused = 1;
}

void Game_Runtime_Resume_Time(void) {
    if (!g_time_paused) { return; }
    g_paused_total += Bsp_Get_Tick_Ms() - g_paused_at;
    g_time_paused = 0;
}
