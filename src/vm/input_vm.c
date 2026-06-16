#include "input_vm.h"

#include <SDL2/SDL.h>

// Snapshot: written only by main thread (in Vm_Input_Poll), read by all threads.
// No mutex needed — x86 guarantees aligned 32-bit reads are atomic,
// and each field is independently valid (missing an update by ≤1 frame is fine).
static volatile struct {
    float x, y;
    uint8_t button;
    int quit;
} g_snap;

void Vm_Input_Init(void) {
    g_snap.x = 0;
    g_snap.y = 0;
    g_snap.button = 0;
    g_snap.quit = 0;
}

void Vm_Input_Poll(void) {
    const uint8_t* keys = SDL_GetKeyboardState(NULL);

    float x = 0, y = 0;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) x += 1.0f;
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) x -= 1.0f;
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) y += 1.0f;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) y -= 1.0f;

    g_snap.x = x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
    g_snap.y = y > 1.0f ? 1.0f : (y < -1.0f ? -1.0f : y);
    g_snap.button = keys[SDL_SCANCODE_SPACE] ? 1 : 0;
}

float Vm_Input_Get_X(void) { return g_snap.x; }
float Vm_Input_Get_Y(void) { return g_snap.y; }
uint8_t Vm_Input_Get_Button(void) { return g_snap.button; }
int Vm_Input_Quit_Requested(void) { return g_snap.quit; }
