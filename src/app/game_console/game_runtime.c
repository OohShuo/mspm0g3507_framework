#include "game_runtime.h"

#include <stddef.h>

#include "bsp_time.h"

#define GAME_RNG_ZERO_SEED 0x6D2B79F5u

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

void Game_Rng_Seed(Game_rng* rng, uint32_t seed) {
    if (rng == NULL) { return; }
    rng->state = seed == 0u ? GAME_RNG_ZERO_SEED : seed;
}

uint32_t Game_Rng_Next(Game_rng* rng) {
    if (rng == NULL) { return 0u; }
    rng->state = rng->state * 1664525u + 1013904223u;
    return rng->state;
}

uint32_t Game_Rng_Range(Game_rng* rng, uint32_t upper_bound) {
    if (rng == NULL || upper_bound == 0u) { return 0u; }
    const uint32_t threshold = (uint32_t)(0u - upper_bound) % upper_bound;
    uint32_t value;
    do { value = Game_Rng_Next(rng); } while (value < threshold);
    return value % upper_bound;
}
