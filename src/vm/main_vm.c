#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "display_vm.h"
#include "hal.h"
#include "input_vm.h"
#include "local_lib.h"

static volatile int g_run = 1;

static void on_signal(int s) {
    (void)s;
    g_run = 0;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    SDL_SetMainReady();

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "[VM] SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    Vm_Display_Init();
    Vm_Input_Init();

    printf("[VM] Starting virtual device...\n");

    Local_Lib_Init();
    extern void Bsp_Init(void);
    Bsp_Init();
    Hal_Init();
    App_Init();
    Hal_Task_Def();
    App_Task_Def();

    printf("[VM] Controls: WASD/Arrow = joystick, Space = button, ESC = quit\n");

    while (g_run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) g_run = 0;
        }
        Vm_Input_Poll();  // snapshot keyboard → shared struct (main thread only)
        Vm_Display_Render();
        SDL_Delay(5);
    }

    printf("[VM] Shutting down...\n");
    Vm_Display_Deinit();
    SDL_Quit();
    return 0;
}
