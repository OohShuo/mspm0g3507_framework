#include "haptics_vm.h"

#include <stdio.h>

static SDL_atomic_t g_strength;
static SDL_GameController* g_controller;
static SDL_JoystickID g_controller_id = -1;
static uint8_t g_applied_strength = 0xffu;

static uint8_t clamp_percent(uint8_t value) { return value > 100u ? 100u : value; }

void Vm_Haptics_Set_Strength(uint8_t strength_percent) {
    SDL_AtomicSet(&g_strength, clamp_percent(strength_percent));
}

uint8_t Vm_Haptics_Get_Strength(void) { return (uint8_t)SDL_AtomicGet(&g_strength); }

static void close_controller(void) {
    if (g_controller == NULL) { return; }
    SDL_GameControllerRumble(g_controller, 0u, 0u, 0u);
    SDL_GameControllerClose(g_controller);
    g_controller = NULL;
    g_controller_id = -1;
    g_applied_strength = 0xffu;
    printf("[VM] Haptic controller disconnected\n");
}

static void open_controller(int device_index) {
    if (g_controller != NULL || !SDL_IsGameController(device_index)) { return; }
    g_controller = SDL_GameControllerOpen(device_index);
    if (g_controller == NULL) { return; }
    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(g_controller);
    g_controller_id = SDL_JoystickInstanceID(joystick);
    g_applied_strength = 0xffu;
    printf("[VM] Haptic controller: %s\n", SDL_GameControllerName(g_controller));
}

void Vm_Haptics_Init(void) {
    SDL_AtomicSet(&g_strength, 0);
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0) {
        fprintf(stderr, "[VM] SDL controller haptics disabled: %s\n", SDL_GetError());
        return;
    }
    for (int index = 0; index < SDL_NumJoysticks() && g_controller == NULL; index++) {
        open_controller(index);
    }
}

void Vm_Haptics_Handle_Event(const SDL_Event* event) {
    if (event == NULL) { return; }
    if (event->type == SDL_CONTROLLERDEVICEADDED) {
        open_controller(event->cdevice.which);
    } else if (event->type == SDL_CONTROLLERDEVICEREMOVED && event->cdevice.which == g_controller_id) {
        close_controller();
    }
}

void Vm_Haptics_Update(void) {
    const uint8_t strength = Vm_Haptics_Get_Strength();
    if (g_controller == NULL || strength == g_applied_strength) { return; }
    const uint16_t magnitude = (uint16_t)((uint32_t)strength * 0xffffu / 100u);
    SDL_GameControllerRumble(g_controller, magnitude, magnitude, strength == 0u ? 0u : 1000u);
    g_applied_strength = strength;
}

void Vm_Haptics_Deinit(void) {
    Vm_Haptics_Set_Strength(0u);
    close_controller();
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);
}
