#include "screensaver.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define SEGMENT_SIZE  3
#define STEP_SIZE     3
#define MAX_PIPES     7
#define TURN_CHANCE   18
#define MAX_LENGTH    250u

#define GRID_W ((uint16_t)(SCREEN_WIDTH / SEGMENT_SIZE))  /* 80 */
#define GRID_H ((uint16_t)(SCREEN_HEIGHT / SEGMENT_SIZE)) /* 106 */

#define COLOR_BLACK 0x0000u

/* ── pipe colour palette (desaturated) ── */
static const uint16_t g_palette[] = {
    0x0412u, /* muted cyan    */
    0x03a0u, /* muted green   */
    0x5260u, /* muted yellow  */
    0x6000u, /* muted red     */
    0x600cu, /* muted magenta */
    0x000cu, /* muted blue    */
    0x4228u, /* muted white   */

    0x0000u, 0x0000u, 0x0000u, 0x0000u,
    0x0000u, 0x0000u, 0x0000u, /* black (blends into background) */
};
#define PALETTE_SIZE (sizeof(g_palette) / sizeof(g_palette[0]))

typedef struct {
    int16_t x, y;
    int8_t dx, dy;
    uint16_t color;
    uint16_t length;
    bool alive;
} Pipe;

static St7789* g_lcd = NULL;
static Pipe g_pipes[MAX_PIPES];
static bool g_active = false;
static uint8_t g_frame_skip = 0;

/* ── xorshift PRNG (no libc dependency) ── */
static uint32_t g_rng_state = 0xdeadbeef;

static uint32_t xorshift32(void) {
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x;
    return x;
}

/* ── helpers ── */
static bool in_bounds(int16_t x, int16_t y) {
    return x >= 0 && y >= 0 && x <= SCREEN_WIDTH - SEGMENT_SIZE &&
           y <= SCREEN_HEIGHT - SEGMENT_SIZE;
}

static void draw_segment(int16_t x, int16_t y, uint16_t color) {
    Game_Graphics_Fill_Rect(g_lcd, x, y, SEGMENT_SIZE, SEGMENT_SIZE, color);
}

/* ── spawn a pipe at a random position ── */
static void spawn_pipe(Pipe* pipe) {
    int16_t cx = (int16_t)(xorshift32() % GRID_W);
    int16_t cy = (int16_t)(xorshift32() % GRID_H);

    pipe->x = cx * SEGMENT_SIZE;
    pipe->y = cy * SEGMENT_SIZE;
    if (xorshift32() & 1u) {
        pipe->dx = (int8_t)((xorshift32() & 1u) ? 1 : -1);
        pipe->dy = 0;
    } else {
        pipe->dx = 0;
        pipe->dy = (int8_t)((xorshift32() & 1u) ? 1 : -1);
    }
    pipe->color = g_palette[xorshift32() % PALETTE_SIZE];
    pipe->length = 0;
    pipe->alive = true;
    draw_segment(pipe->x, pipe->y, pipe->color);
}

/* ── public API ── */
void Screensaver_Init(St7789* lcd) {
    g_lcd = lcd;
    g_rng_state = Bsp_Get_Tick_Ms();
    if (g_rng_state == 0) { g_rng_state = 0xdeadbeef; }

    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);

    for (int i = 0; i < MAX_PIPES; i++) {
        g_pipes[i].alive = false;
        spawn_pipe(&g_pipes[i]);
    }

    g_active = true;
    g_frame_skip = 0;
}

void Screensaver_Run_Frame(void) {
    if (!g_active || g_lcd == NULL) { return; }

    /* skip every other frame — halve movement speed */
    g_frame_skip++;
    if ((g_frame_skip & 1u) == 0) { return; }

    for (int i = 0; i < MAX_PIPES; i++) {
        if (!g_pipes[i].alive) { continue; }

        Pipe* p = &g_pipes[i];

        /* maybe turn */
        if ((xorshift32() % 100u) < TURN_CHANCE) {
            if (p->dx != 0) {
                p->dy = (int8_t)((xorshift32() & 1u) ? 1 : -1);
                p->dx = 0;
            } else {
                p->dx = (int8_t)((xorshift32() & 1u) ? 1 : -1);
                p->dy = 0;
            }
        }

        int16_t nx = p->x + p->dx * STEP_SIZE;
        int16_t ny = p->y + p->dy * STEP_SIZE;

        if (!in_bounds(nx, ny)) {
            p->alive = false;
            continue;
        }

        p->x = nx;
        p->y = ny;
        p->length++;
        draw_segment(p->x, p->y, p->color);

        if (p->length >= MAX_LENGTH) { p->alive = false; }
    }

    /* respawn dead pipes */
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!g_pipes[i].alive) { spawn_pipe(&g_pipes[i]); }
    }
}

void Screensaver_Exit(void) { g_active = false; }

bool Screensaver_Is_Active(void) { return g_active; }
