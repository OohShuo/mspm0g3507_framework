#include "display_vm.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#include "haptics_vm.h"

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;
static uint16_t g_fb[320][240];  // [y][x] RGB565
static volatile int g_dirty = 0;
static int g_ready = 0;
static uint8_t g_last_vibration_strength = 0xffu;

void Vm_Display_Init(void) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "[VM] SDL video init failed: %s\n", SDL_GetError());
        return;
    }
    g_window = SDL_CreateWindow("Game Console VM [240x320 @2x]", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, 480, 640, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!g_window) return;
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) return;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 240, 320);
    if (!g_texture) return;
    memset(g_fb, 0, sizeof(g_fb));
    g_ready = 1;
}

void Vm_Display_Deinit(void) {
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

int Vm_Display_Is_Ready(void) { return g_ready; }

void Vm_Display_Write_Pixels(
    int16_t x1, int16_t y1, int16_t x2, int16_t y2, const uint8_t* pixels, uint32_t pixel_count) {
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= 240) x2 = 239;
    if (y2 >= 320) y2 = 319;
    if (x1 > x2 || y1 > y2) return;
    const uint16_t* px = (const uint16_t*)pixels;
    uint32_t idx = 0;
    for (int16_t y = y1; y <= y2 && idx < pixel_count; y++)
        for (int16_t x = x1; x <= x2 && idx < pixel_count; x++) g_fb[y][x] = px[idx++];
}

void Vm_Display_Frame_Done(void) { g_dirty = 1; }

static void draw_vibration_overlay(uint8_t strength) {
    if (strength == 0u) { return; }
    int output_width = 0;
    int output_height = 0;
    SDL_GetRendererOutputSize(g_renderer, &output_width, &output_height);

    const int thickness = 2 + strength / 12u;
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 255u, 120u, 24u, (uint8_t)(70u + strength));
    for (int inset = 0; inset < thickness; inset++) {
        const SDL_Rect border = {inset, inset, output_width - inset * 2, output_height - inset * 2};
        SDL_RenderDrawRect(g_renderer, &border);
    }

    const int bar_margin = thickness + 8;
    const int bar_width = (output_width - bar_margin * 2) * strength / 100u;
    const SDL_Rect bar = {bar_margin, output_height - thickness - 8, bar_width, 5};
    SDL_RenderFillRect(g_renderer, &bar);
}

void Vm_Display_Render(void) {
    if (!g_ready) return;
    const uint8_t vibration_strength = Vm_Haptics_Get_Strength();
    if (!g_dirty && vibration_strength == g_last_vibration_strength) return;
    g_dirty = 0;
    g_last_vibration_strength = vibration_strength;
    SDL_UpdateTexture(g_texture, NULL, g_fb, 240 * sizeof(uint16_t));
    SDL_SetRenderDrawColor(g_renderer, 0u, 0u, 0u, 255u);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    draw_vibration_overlay(vibration_strength);
    SDL_RenderPresent(g_renderer);
}
