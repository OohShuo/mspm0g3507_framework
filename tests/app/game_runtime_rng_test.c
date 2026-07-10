#include <assert.h>
#include <stdint.h>

#include "game_runtime.h"

uint32_t Bsp_Get_Tick_Ms(void) { return 0; }

int main(void) {
    Game_rng a;
    Game_rng b;

    Game_Rng_Seed(&a, 0u);
    assert(a.state == 0x6D2B79F5u);

    Game_Rng_Seed(&a, 1u);
    assert(Game_Rng_Next(&a) == 1015568748u);
    assert(Game_Rng_Next(&a) == 1586005467u);
    assert(Game_Rng_Next(&a) == 2165703038u);

    Game_Rng_Seed(&a, 0x12345678u);
    Game_Rng_Seed(&b, 0x12345678u);
    assert(Game_Rng_Next(&a) == Game_Rng_Next(&b));
    (void)Game_Rng_Next(&a);
    assert(a.state != b.state);

    assert(Game_Rng_Range(&a, 0u) == 0u);
    for (uint32_t bound = 1u; bound < 65u; bound++) {
        for (uint32_t i = 0; i < 1000u; i++) { assert(Game_Rng_Range(&a, bound) < bound); }
    }

    Game_Rng_Seed(&a, 1u);
    assert(Game_Rng_Range(&a, 0x80000001u) == 18219389u);
    return 0;
}
