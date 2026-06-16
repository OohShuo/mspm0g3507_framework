#include "input_vm.h"

static const uint8_t* g_key = NULL;
static volatile int g_quit = 0;

void Vm_Input_Init(void) {
    g_key = SDL_GetKeyboardState(NULL);
    g_quit = 0;
}

void Vm_Input_Handle_Event(const SDL_Event* e) {
    if (e->type == SDL_QUIT || (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE)) g_quit = 1;
}

int Vm_Input_Quit_Requested(void) { return g_quit; }

static float clamp(float v) { return v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v); }

float Vm_Input_Get_X(void) {
    if (!g_key) return 0;
    float x = 0;
    if (g_key[SDL_SCANCODE_D] || g_key[SDL_SCANCODE_RIGHT]) x += 1.0f;
    if (g_key[SDL_SCANCODE_A] || g_key[SDL_SCANCODE_LEFT]) x -= 1.0f;
    return clamp(x);
}

float Vm_Input_Get_Y(void) {
    if (!g_key) return 0;
    // W/Up = +1.0 → game_direction_up (matching read_direction: y >= 0 = up)
    float y = 0;
    if (g_key[SDL_SCANCODE_W] || g_key[SDL_SCANCODE_UP]) y += 1.0f;
    if (g_key[SDL_SCANCODE_S] || g_key[SDL_SCANCODE_DOWN]) y -= 1.0f;
    return clamp(y);
}

uint8_t Vm_Input_Get_Button(void) { return g_key ? (g_key[SDL_SCANCODE_SPACE] ? 1 : 0) : 0; }
