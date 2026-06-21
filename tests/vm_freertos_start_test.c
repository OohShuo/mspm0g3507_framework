#include <assert.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include "FreeRTOS.h"
#include "task.h"
#include "task_vm.h"

static SDL_atomic_t g_queued_runs;
static SDL_atomic_t g_dynamic_runs;

static void count_once(void* arg) {
    SDL_atomic_t* counter = (SDL_atomic_t*)arg;
    SDL_AtomicIncRef(counter);
}

static void wait_for(SDL_atomic_t* counter) {
    for (uint32_t i = 0u; i < 100u && SDL_AtomicGet(counter) == 0; ++i) { SDL_Delay(1u); }
}

int main(void) {
    assert(SDL_Init(SDL_INIT_TIMER) == 0);

    assert(xTaskCreate(count_once, "queued", 128u, &g_queued_runs, 1u, NULL) == pdPASS);
    SDL_Delay(5u);
    assert(SDL_AtomicGet(&g_queued_runs) == 0);

    assert(Vm_Freertos_Start_Tasks() == pdPASS);
    wait_for(&g_queued_runs);
    assert(SDL_AtomicGet(&g_queued_runs) == 1);

    assert(xTaskCreate(count_once, "dynamic", 128u, &g_dynamic_runs, 1u, NULL) == pdPASS);
    wait_for(&g_dynamic_runs);
    assert(SDL_AtomicGet(&g_dynamic_runs) == 1);

    SDL_Quit();
    return 0;
}
