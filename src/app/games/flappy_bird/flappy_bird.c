#include "flappy_bird.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define BAR_H         12
#define BAR_BOT       298
#define PLAY_TOP      BAR_H
#define PLAY_BOT      BAR_BOT
#define PLAY_H        (PLAY_BOT - PLAY_TOP)

#define BIRD_X        50
#define BIRD_W        16
#define BIRD_H        10
#define GROUND_Y      290
#define FLAP_VELOCITY -5
#define PIPE_W        20
#define PIPE_GAP      56
#define PIPE_SPACING  150
#define MAX_PIPES     4

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_GREEN   0x07e0u
#define COLOR_CYAN    0x07ffu
#define COLOR_YELLOW  0xffe0u
#define COLOR_RED     0xf800u
#define COLOR_DARK    0x4208u
#define COLOR_GRAY    0x8410u

typedef enum { flappy_state_ready, flappy_state_playing, flappy_state_over } Flappy_state;
typedef struct {
    int16_t x;
    uint16_t gap_y;
    uint8_t scored;
    uint8_t active;
} Pipe;

static Game_hardware g_hardware;
static Flappy_state g_state;
static int16_t g_bird_y, g_bird_vy, g_old_bird_y;
static uint8_t g_gravity_acc, g_up_prev, g_ground_offset;
static Pipe g_pipes[MAX_PIPES];
static uint32_t g_score, g_old_score, g_pipe_timer;
static uint16_t g_speed_acc;
static uint8_t g_bars_drawn;
static uint32_t g_rand_state;
static uint32_t g_start_ms;
static uint32_t g_last_tick;

static uint32_t fast_rand(void) {
    g_rand_state = g_rand_state * 1103515245 + 12345;
    return (g_rand_state >> 16) & 0x7FFF;
}

/* ── Fill helpers: bar_fill (full screen) / play_fill (game area only) ── */

static void bar_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (y + h > SCREEN_HEIGHT) { h = SCREEN_HEIGHT - y; }
    if (w <= 0 || h <= 0) { return; }
    Game_Graphics_Fill_Rect(g_hardware.lcd, x, y, w, h, c);
}

static void play_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < PLAY_TOP) {
        h -= PLAY_TOP - y;
        y = PLAY_TOP;
    }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (y + h > PLAY_BOT) { h = PLAY_BOT - y; }
    if (w <= 0 || h <= 0) { return; }
    Game_Graphics_Fill_Rect(g_hardware.lcd, x, y, w, h, c);
}

#define BASE_TICK_MS     20u
#define SPEED_MAX_FRAMES 1200u
#define SPEED_MAX_MS     (SPEED_MAX_FRAMES * BASE_TICK_MS)

/* Linear speed 2→5 over 24 s, fixed-point ×256 */
static uint16_t speed_scaled(uint32_t elapsed_ms) {
    if (elapsed_ms >= SPEED_MAX_MS) { return 1280u; }
    return 512u + (uint16_t)((elapsed_ms * 768u) / SPEED_MAX_MS);
}

/* ── Status bars (drawn once, full screen) ── */

static void draw_bars(void) {
    if (g_bars_drawn) { return; }
    g_bars_drawn = 1;
    St7789* l = g_hardware.lcd;
    bar_fill(0, 0, SCREEN_WIDTH, BAR_H, COLOR_BLACK);
    Game_Graphics_Draw_Text(l, 2, 2, "FLAPPY", 1, COLOR_WHITE);
    bar_fill(0, BAR_H - 1, SCREEN_WIDTH, 1, COLOR_DARK);
    bar_fill(0, BAR_BOT, SCREEN_WIDTH, SCREEN_HEIGHT - BAR_BOT, COLOR_BLACK);
    bar_fill(0, BAR_BOT, SCREEN_WIDTH, 1, COLOR_DARK);
    Game_Graphics_Draw_Text(l, 56, BAR_BOT + 3, "HOLD TO BACK", 1, COLOR_GRAY);
}

static void update_score(void) {
    if (g_state == flappy_state_ready) { return; }
    bar_fill(SCREEN_WIDTH - 30, 2, 30, 8, COLOR_BLACK);
    Game_Graphics_Draw_U32(g_hardware.lcd, SCREEN_WIDTH - 30, 2, g_score, 3, 1, COLOR_CYAN);
}

/* ── Bird ── */

static void draw_bird(int16_t y) {
    int32_t bx = BIRD_X, by = y;
    play_fill(bx - 1, by - 1, BIRD_W + 3, BIRD_H + 2, COLOR_BLACK);
    play_fill(bx + 2, by, 8, BIRD_H, COLOR_YELLOW);
    play_fill(bx + 6, by + 1, 6, 6, COLOR_YELLOW);
    play_fill(bx + 10, by + 3, 3, 2, COLOR_RED);
    play_fill(bx + 9, by + 1, 2, 2, COLOR_WHITE);
    play_fill(bx + 3, by + 3, 4, 2, COLOR_DARK);
}

/* ── Pipes: delta render within play area ── */

static void pipe_delta(Pipe* p, int16_t sp) {
    int16_t ox = p->x, gt = (int16_t)p->gap_y, gb = gt + PIPE_GAP;
    int16_t er = ox + PIPE_W, nx = ox - sp;

    /* Erase rightmost sp+2 px (covers cap overhang) */
    play_fill(er - sp - 2, PLAY_TOP, sp + 4, PLAY_H, COLOR_BLACK);

    /* Draw leftmost sp px with pipe pattern */
    if (gt > PLAY_TOP) { play_fill(nx + 2, PLAY_TOP, sp, gt - PLAY_TOP - 4, COLOR_GREEN); }
    /* Top cap */
    play_fill(nx, gt - 4, sp, 4, COLOR_GREEN);
    /* Bottom cap */
    play_fill(nx, gb, sp, 4, COLOR_GREEN);
    if (gb + 4 < PLAY_BOT) { play_fill(nx + 2, gb + 4, sp, PLAY_BOT - gb - 4, COLOR_GREEN); }
}

/* ── Ground (play area only) ── */

static void draw_ground(void) {
    for (int16_t x = -(int16_t)g_ground_offset; x < SCREEN_WIDTH; x += 16) {
        play_fill(x, GROUND_Y, 12, 2, COLOR_DARK);
    }
}

/* ── Spawn ── */

static void spawn_pipe(void) {
    for (uint8_t i = 0; i < MAX_PIPES; i++) {
        if (!g_pipes[i].active) {
            g_pipes[i].gap_y = (uint16_t)(60 + (fast_rand() % (GROUND_Y - PIPE_GAP - 120)));
            g_pipes[i].x = SCREEN_WIDTH + 10;
            g_pipes[i].scored = 0;
            g_pipes[i].active = 1;
            return;
        }
    }
}

/* ── Collision ── */

static uint8_t check_collision(void) {
    int32_t bl = BIRD_X + 2, br = BIRD_X + BIRD_W - 2;
    int32_t bt = g_bird_y + 1, bb = g_bird_y + BIRD_H - 1;
    if (bb >= GROUND_Y || bt <= PLAY_TOP) { return 1; }
    for (uint8_t i = 0; i < MAX_PIPES; i++) {
        if (!g_pipes[i].active) { continue; }
        int32_t pl = g_pipes[i].x + 2, pr = g_pipes[i].x + PIPE_W - 2;
        int32_t gt = (int32_t)g_pipes[i].gap_y, gb = gt + PIPE_GAP;
        if (br <= pl || bl >= pr) { continue; }
        if (bt < gt || bb > gb) { return 1; }
    }
    return 0;
}

/* ── Restart ── */

static void restart_game(void) {
    g_state = flappy_state_ready;
    g_bird_y = 160;
    g_bird_vy = 0;
    g_old_bird_y = g_bird_y;
    g_gravity_acc = 0;
    g_up_prev = 0;
    g_ground_offset = 0;
    g_score = 0;
    g_old_score = 0;
    g_pipe_timer = 0;
    g_speed_acc = 0;
    g_last_tick = 0;
    g_rand_state = Bsp_Get_Tick_Ms();
    g_bars_drawn = 0;
    for (uint8_t i = 0; i < MAX_PIPES; i++) { g_pipes[i].active = 0; }

    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_bars();
    draw_ground();
    draw_bird(g_bird_y);
    Game_Graphics_Draw_Text(g_hardware.lcd, 28, 140, "PUSH UP TO START", 1, COLOR_WHITE);
}

void Flappy_Bird_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

/* ── Update ── */

Game_result Flappy_Bird_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }
    St7789* lcd = g_hardware.lcd;

    if (g_state == flappy_state_over) {
        if (input->direction_pressed) { restart_game(); }
        return game_result_running;
    }

    if (g_state == flappy_state_ready) {
        int16_t bob = (int16_t)((Bsp_Get_Tick_Ms() / 300u) % 2u);
        int16_t by = (int16_t)(160 + bob * 4 - 2);
        if (g_bird_y != by) { play_fill(BIRD_X - 1, g_old_bird_y - 1, BIRD_W + 3, BIRD_H + 2, COLOR_BLACK); }
        g_old_bird_y = g_bird_y;
        g_bird_y = by;
        if (g_bird_y != g_old_bird_y) { draw_bird(g_bird_y); }
        uint8_t up = (input->direction == game_direction_up);
        if (up && !g_up_prev) {
            g_state = flappy_state_playing;
            g_start_ms = Bsp_Get_Tick_Ms();
            g_last_tick = 0;
            g_bird_vy = FLAP_VELOCITY;
            g_gravity_acc = 0;
            play_fill(20, 130, SCREEN_WIDTH - 40, 20, COLOR_BLACK);
            update_score();
        }
        g_up_prev = up;
        return game_result_running;
    }

    /* ═══ Playing ═══ */

    /* ── Input (once per real call, applied to upcoming tick) ── */
    {
        uint8_t up = (input->direction == game_direction_up);
        if (up && !g_up_prev) {
            g_bird_vy = FLAP_VELOCITY;
            g_gravity_acc = 0;
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_flappy_flap);
        }
        g_up_prev = up;
    }

    /* ── Tick-accumulator: run one logic tick per 20 ms of elapsed time ── */
    const uint32_t now = Bsp_Get_Tick_Ms();
    const uint32_t elapsed = now - g_start_ms;
    uint32_t effective_ticks = elapsed / BASE_TICK_MS;
    /* Cap catch-up to prevent spiral on stall */
    if (effective_ticks - g_last_tick > 5u) { effective_ticks = g_last_tick + 5u; }

    const int16_t bird_before = g_bird_y;

    while (g_last_tick < effective_ticks) {
        g_last_tick++;
        const uint32_t tick_ms = g_last_tick * BASE_TICK_MS;

        /* Gravity */
        g_gravity_acc += 1u;
        if (g_gravity_acc >= 2u) {
            g_gravity_acc -= 2u;
            g_bird_vy++;
        }
        g_bird_y += g_bird_vy;
        if (g_bird_y < PLAY_TOP) { g_bird_y = PLAY_TOP; }

        /* Pipe movement */
        {
            const uint16_t speed = speed_scaled(tick_ms);
            g_speed_acc += speed;
            uint8_t sp = (uint8_t)(g_speed_acc >> 8);
            g_speed_acc &= 0xFFu;
            if (sp > 0) {
                for (uint8_t i = 0; i < MAX_PIPES; i++) {
                    if (!g_pipes[i].active) { continue; }
                    pipe_delta(&g_pipes[i], (int16_t)sp);
                    g_pipes[i].x -= sp;
                    if (g_pipes[i].x + PIPE_W < -10) { g_pipes[i].active = 0; }
                    if (!g_pipes[i].scored && g_pipes[i].x + PIPE_W < BIRD_X) {
                        g_pipes[i].scored = 1;
                        g_score++;
                        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_flappy_score);
                    }
                }
            }
        }

        /* Pipe spawn */
        if (g_pipe_timer == 0) {
            spawn_pipe();
            g_pipe_timer = PIPE_SPACING / (speed_scaled(tick_ms) >> 8) + (fast_rand() % 30);
        } else {
            g_pipe_timer--;
        }

        /* Ground scroll */
        {
            uint8_t s = speed_scaled(tick_ms) >> 8;
            if (s < 2) { s = 2; }
            g_ground_offset = (uint8_t)((g_ground_offset + s) % 16);
        }
    }

    /* ── Rendering (once per real call) ── */
    if (g_bird_y != bird_before) {
        play_fill(BIRD_X - 1, bird_before - 1, BIRD_W + 3, BIRD_H + 2, COLOR_BLACK);
        draw_bird(g_bird_y);
    }
    draw_ground();

    if (g_score != g_old_score) {
        g_old_score = g_score;
        update_score();
    }

    if (check_collision()) {
        g_state = flappy_state_over;
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_life_lost);
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_defeat);
        play_fill(BIRD_X - 1, g_bird_y - 1, BIRD_W + 3, BIRD_H + 2, COLOR_RED);
        update_score();
        play_fill(50, 148, 140, 9, COLOR_BLACK);
        Game_Graphics_Draw_Text(lcd, 60, 150, "GAME OVER", 1, COLOR_RED);
        play_fill(25, 168, 190, 9, COLOR_BLACK);
        Game_Graphics_Draw_Text(lcd, 30, 170, "PUSH TO RESTART", 1, COLOR_WHITE);
    }

    return game_result_running;
}

uint32_t Flappy_Bird_Get_Score(void) { return g_score; }
uint8_t Flappy_Bird_Is_Finished(void) { return g_state == flappy_state_over; }
