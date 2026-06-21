#include <SDL2/SDL.h>
#include <signal.h>
#include <stdio.h>

#include "display_vm.h"
#include "haptics_vm.h"
#include "input_vm.h"
#include "platform.h"
#include "task_vm.h"

static volatile sig_atomic_t g_run = 1;

static void on_signal(int signal_number) {
    (void)signal_number;
    g_run = 0;
}

int Platform_Init(void) {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "[VM] SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

    Vm_Display_Init();
    Vm_Input_Init();
    Vm_Haptics_Init();
    printf("[VM] Starting virtual device...\n");
    return 0;
}

int Platform_Start(void) {
    if (Vm_Freertos_Start_Tasks() != pdPASS) {
        fprintf(stderr, "[VM] Failed to start virtual tasks\n");
        Vm_Haptics_Deinit();
        Vm_Display_Deinit();
        SDL_Quit();
        return -1;
    }

    printf("[VM] Controls: Arrows = joystick, W/A/S/D = X/Y/A/B, Space = START, ESC = quit\n");
    while (g_run) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            Vm_Haptics_Handle_Event(&event);
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                g_run = 0;
            }
        }
        Vm_Input_Poll();
        Vm_Haptics_Update();
        Vm_Display_Render();
        SDL_Delay(5u);
    }

    printf("[VM] Shutting down...\n");
    Vm_Haptics_Deinit();
    Vm_Display_Deinit();
    SDL_Quit();
    return 0;
}
