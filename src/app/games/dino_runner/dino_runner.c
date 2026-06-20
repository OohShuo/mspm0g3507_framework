#include "dino_runner.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define PLAY_TOP      GAME_TOP_BAR_H
#define PLAY_BOT      GAME_AREA_BOTTOM
#define GROUND_Y      200
#define DINO_X        30
#define DINO_W        20
#define DINO_H        20
#define DINO_CROUCH_H 9

#define GRAVITY       1
#define JUMP_VELOCITY -8
#define FAST_DROP_VELOCITY 8
#define MAX_OBSTACLES 6
#define PTERO_W       24
#define PTERO_H       14
#define PTERO_HIGH_Y  (GROUND_Y - 70)
#define PTERO_LOW_Y   (GROUND_Y - 28)

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_GREEN   0x07e0u
#define COLOR_CYAN    0x07ffu
#define COLOR_DARK    0x4208u
#define COLOR_RED     0xf800u
#define COLOR_GRAY    0x8410u

typedef enum { dino_state_ready, dino_state_running, dino_state_over } Dino_state;
typedef enum { obs_cactus, obs_ptero_high, obs_ptero_low } Obs_type;
typedef struct {
    int16_t x;
    uint8_t w;
    uint8_t h;
    int16_t y;
    Obs_type type;
    uint8_t active;
} Obstacle;

static Game_hardware g_hardware;
static Dino_state g_state;
static int16_t g_dino_y, g_old_dino_y, g_dino_vy;
static uint8_t g_dino_in_air, g_jump_held, g_gravity_acc, g_up_prev, g_crouching;
static Obstacle g_obstacles[MAX_OBSTACLES];
static uint16_t g_move_acc;
static uint32_t g_score, g_old_score, g_spawn_countdown, g_rand_state;
static uint8_t g_ptero_wing;
static uint32_t g_start_ms;
static uint32_t g_last_tick;

#define BASE_TICK_MS 20u

static uint32_t fast_rand(void) {
    g_rand_state = g_rand_state * 1103515245 + 12345;
    return (g_rand_state >> 16) & 0x7FFF;
}

/* ── Fill helpers ── */

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

static uint16_t speed_scaled(uint32_t elapsed_ms) {
    /* Score is now elapsed_ms / 20 (effective tick count) */
    const uint32_t tick_count = elapsed_ms / BASE_TICK_MS;
    if (tick_count >= 2000u) { return 2048u; }
    return 768u + (uint16_t)((tick_count * 1280u) / 2000u);
}

static void update_score(void) {
    if (g_state == dino_state_ready) { return; }
    /* 5 digits = 30px → x=203 (5px margin) */
    bar_fill(203, 2, 35, 8, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_U32(g_hardware.lcd, 208, 2, g_score, 5, 1, COLOR_CYAN);
}

/* ── Dino ── */

static void draw_dino(int16_t y, uint8_t crouching, uint16_t color) {
    int32_t dx = DINO_X, dy = y;
    play_fill(dx, dy, DINO_W, DINO_H, COLOR_BLACK);
    if (color == COLOR_BLACK) { return; }
    if (crouching) {
        dy += DINO_H - DINO_CROUCH_H;
        play_fill(dx + 3, dy + 2, 15, 7, color);
        play_fill(dx + 14, dy, 6, 6, color);
        play_fill(dx, dy + 5, 6, 3, color);
        play_fill(dx + 17, dy + 1, 2, 2, COLOR_WHITE);
        return;
    }
    play_fill(dx + 8, dy, 12, 10, color);
    play_fill(dx + 4, dy + 4, 9, 12, color);
    play_fill(dx, dy + 7, 6, 5, color);
    play_fill(dx + 3, dy + 14, 4, 6, color);
    play_fill(dx + 11, dy + 14, 4, 6, color);
    play_fill(dx + 16, dy + 2, 2, 2, COLOR_WHITE);
}

/* ── Cactus ── */

static void draw_cactus(int16_t x, uint8_t w, uint8_t h, uint16_t color) {
    int32_t cx = x, cy = GROUND_Y - h;
    play_fill(cx, cy, w, h, COLOR_BLACK);
    if (color == COLOR_BLACK) { return; }
    play_fill(cx + w / 3, cy, w / 3 + 1, h, color);
    play_fill(cx, cy + h / 3, w / 3, 3, color);
    if (h > 22) { play_fill(cx + w * 2 / 3, cy + h * 2 / 3, w / 3, 3, color); }
}

/* ── Ptero ── */

static void draw_ptero(int16_t x, int16_t y, uint16_t color) {
    play_fill(x, y, PTERO_W, PTERO_H, COLOR_BLACK);
    if (color == COLOR_BLACK) { return; }
    play_fill(x + 6, y + 4, 12, 6, color);
    play_fill(x + 17, y + 3, 7, 5, color);
    play_fill(x + 20, y + 3, 2, 2, COLOR_WHITE);
    if (g_ptero_wing & 1u) {
        play_fill(x + 2, y, 6, 4, color);
        play_fill(x + 16, y, 6, 4, color);
    } else {
        play_fill(x + 4, y + 8, 6, 4, color);
        play_fill(x + 14, y + 8, 6, 4, color);
    }
}

static void draw_ground(void) { play_fill(0, GROUND_Y, SCREEN_WIDTH, 2, COLOR_DARK); }

/* ── Obstacles ── */

static uint8_t obs_visible(int16_t x, uint8_t w) { return x < SCREEN_WIDTH && (x + w) > 0; }

static void move_obstacles(uint32_t elapsed_ms) {
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
        if (!g_obstacles[i].active || !obs_visible(g_obstacles[i].x, g_obstacles[i].w)) { continue; }
        play_fill(
            g_obstacles[i].x - 1, g_obstacles[i].y, g_obstacles[i].w + 2, g_obstacles[i].h, COLOR_BLACK);
    }

    g_ptero_wing = (uint8_t)((g_score / 8u) & 1u);
    g_move_acc += speed_scaled(elapsed_ms);
    uint8_t px = (uint8_t)(g_move_acc >> 8);
    g_move_acc &= 0xFFu;

    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
        if (!g_obstacles[i].active) { continue; }
        g_obstacles[i].x = (int16_t)(g_obstacles[i].x - px);
        if (g_obstacles[i].x + g_obstacles[i].w < -10) { g_obstacles[i].active = 0; }
    }

    draw_ground();
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
        if (!g_obstacles[i].active || !obs_visible(g_obstacles[i].x, g_obstacles[i].w)) { continue; }
        if (g_obstacles[i].type == obs_cactus) {
            draw_cactus(g_obstacles[i].x, g_obstacles[i].w, g_obstacles[i].h, COLOR_DARK);
        } else {
            draw_ptero(g_obstacles[i].x, g_obstacles[i].y, COLOR_CYAN);
        }
    }
}

/* ── Spawn ── */

static uint8_t next_free(void) {
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
        if (!g_obstacles[i].active) { return i; }
    }
    return MAX_OBSTACLES;
}

static void add_cactus(uint8_t s, int16_t x, uint8_t w, uint8_t h) {
    g_obstacles[s].x = x;
    g_obstacles[s].w = w;
    g_obstacles[s].h = h;
    g_obstacles[s].y = GROUND_Y - h;
    g_obstacles[s].type = obs_cactus;
    g_obstacles[s].active = 1;
}

static void add_ptero(uint8_t s, int16_t x, Obs_type t) {
    g_obstacles[s].x = x;
    g_obstacles[s].w = PTERO_W;
    g_obstacles[s].h = PTERO_H;
    g_obstacles[s].y = (t == obs_ptero_high) ? PTERO_HIGH_Y : PTERO_LOW_Y;
    g_obstacles[s].type = t;
    g_obstacles[s].active = 1;
}

static void spawn_obstacle(void) {
    uint32_t r = fast_rand() % 100;
    if ((g_score > 400u) && r < 20) {
        uint8_t s = next_free();
        if (s < MAX_OBSTACLES)
            add_ptero(s, SCREEN_WIDTH + 20, (fast_rand() % 2u) ? obs_ptero_high : obs_ptero_low);
        return;
    }
    if ((g_score > 600u) && r < 65 && r >= 35) {
        uint8_t cnt = 2u + (fast_rand() % 2u);
        int16_t cx = SCREEN_WIDTH + 10;
        for (uint8_t j = 0; j < cnt; j++) {
            uint8_t s = next_free();
            if (s >= MAX_OBSTACLES) { break; }
            uint8_t h, w;
            uint32_t sr = fast_rand() % 100;
            if (sr < 50) {
                w = 10;
                h = 18;
            } else if (sr < 80) {
                w = 14;
                h = 26;
            } else {
                w = 18;
                h = 34;
            }
            add_cactus(s, cx, w, h);
            cx = (int16_t)(cx + w + 6 + (fast_rand() % 10));
        }
        return;
    }
    uint8_t s = next_free();
    if (s >= MAX_OBSTACLES) { return; }
    uint8_t h, w;
    if (r < 50) {
        w = 10;
        h = 18;
    } else if (r < 85) {
        w = 14;
        h = 26;
    } else {
        w = 18;
        h = 34;
    }
    add_cactus(s, SCREEN_WIDTH + 10, w, h);
}

/* ── Collision ── */

static uint8_t check_collision(void) {
    int16_t dl = DINO_X + 3, dr = DINO_X + DINO_W - 3;
    int16_t dt = g_crouching ? GROUND_Y - DINO_CROUCH_H + 1 : g_dino_y + 2;
    int16_t db = g_dino_y + DINO_H - 2;
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
        if (!g_obstacles[i].active) { continue; }
        int16_t ol = g_obstacles[i].x + 3, orr = g_obstacles[i].x + g_obstacles[i].w - 3;
        int16_t ot = g_obstacles[i].y + 2, ob = g_obstacles[i].y + g_obstacles[i].h - 2;
        if (dr > ol && dl < orr && db > ot && dt < ob) { return 1; }
    }
    return 0;
}

/* ── Restart ── */

static void restart_game(void) {
    g_state = dino_state_ready;
    g_dino_y = GROUND_Y - DINO_H;
    g_old_dino_y = g_dino_y;
    g_dino_vy = 0;
    g_dino_in_air = 0;
    g_jump_held = 0;
    g_gravity_acc = 0;
    g_up_prev = 0;
    g_crouching = 0;
    g_move_acc = 0;
    g_score = 0;
    g_old_score = 0;
    g_spawn_countdown = 60;
    g_rand_state = Game_Runtime_Get_Tick_Ms();
    g_last_tick = 0;
    g_ptero_wing = 0;
    for (uint8_t i = 0; i < MAX_OBSTACLES; i++) { g_obstacles[i].active = 0; }

    Game_Graphics_Clear_Game_Area(g_hardware.lcd);
    draw_ground();
    draw_dino(g_dino_y, 0, COLOR_GREEN);
    Game_Graphics_Draw_Text(g_hardware.lcd, 58, 140, "A TO START", 1, COLOR_WHITE);
}

void Dino_Runner_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

/* ── Update ── */

Game_result Dino_Runner_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }
    St7789* lcd = g_hardware.lcd;

    if (g_state == dino_state_over) {
        if (input->a_pressed) { restart_game(); }
        return game_result_running;
    }

    if (g_state == dino_state_ready) {
        if (input->a_pressed) {
            g_state = dino_state_running;
            g_start_ms = Game_Runtime_Get_Tick_Ms();
            g_last_tick = 0;
            play_fill(20, 130, SCREEN_WIDTH - 40, 20, COLOR_BLACK);
            update_score();
        }
        g_up_prev = input->a_down;
        return game_result_running;
    }

    /* ── Input (once per real call, applied to upcoming tick) ── */
    {
        uint8_t up = input->a_down;
        if (!g_dino_in_air && up && !g_up_prev) {
            g_crouching = 0;
            g_dino_vy = JUMP_VELOCITY;
            g_dino_in_air = 1;
            g_jump_held = 1;
            g_gravity_acc = 0;
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_dino_jump);
        }
        if (!up || g_dino_vy >= 0) { g_jump_held = 0; }
        if (g_dino_in_air && input->y_down) {
            g_jump_held = 0;
            if (g_dino_vy < FAST_DROP_VELOCITY) { g_dino_vy = FAST_DROP_VELOCITY; }
        } else if (!g_dino_in_air) {
            g_crouching = input->y_down;
        }
        g_up_prev = up;
    }

    /* ── Tick-accumulator: run one logic tick per 20 ms of elapsed time ── */
    const uint32_t now = Game_Runtime_Get_Tick_Ms();
    const uint32_t elapsed = now - g_start_ms;
    uint32_t effective_ticks = elapsed / BASE_TICK_MS;
    if (effective_ticks - g_last_tick > 5u) { effective_ticks = g_last_tick + 5u; }

    while (g_last_tick < effective_ticks) {
        g_last_tick++;

        /* Dino physics */
        g_old_dino_y = g_dino_y;
        if (g_dino_in_air) {
            g_gravity_acc += g_jump_held ? 1u : 2u;
            if (g_gravity_acc >= 2u) {
                g_gravity_acc -= 2u;
                g_dino_vy += GRAVITY;
            }
            g_dino_y += g_dino_vy;
            if (g_dino_y >= GROUND_Y - DINO_H) {
                g_dino_y = GROUND_Y - DINO_H;
                g_dino_vy = 0;
                g_dino_in_air = 0;
                g_jump_held = 0;
            }
        }

        if (g_dino_y != g_old_dino_y) { draw_dino(g_old_dino_y, 0, COLOR_BLACK); }
        move_obstacles(g_last_tick * BASE_TICK_MS);
        draw_dino(g_dino_y, g_crouching, COLOR_GREEN);

        /* Spawn */
        if (g_spawn_countdown == 0) {
            spawn_obstacle();
            g_spawn_countdown =
                50u + (fast_rand() % 80u) - (speed_scaled(g_last_tick * BASE_TICK_MS) >> 8) * 4u;
            if (g_spawn_countdown < 15u) { g_spawn_countdown = 15u; }
        } else {
            g_spawn_countdown--;
        }

        g_score++;
    }

    update_score();

    if (check_collision()) {
        g_state = dino_state_over;
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_life_lost);
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_defeat);
        draw_dino(g_dino_y, g_crouching, COLOR_RED);
        update_score();
        play_fill(50, 148, 140, 9, COLOR_BLACK);
        Game_Graphics_Draw_Text(lcd, 60, 150, "GAME OVER", 1, COLOR_RED);
        play_fill(25, 168, 190, 9, COLOR_BLACK);
        Game_Graphics_Draw_Text(lcd, 48, 170, "A TO RESTART", 1, COLOR_WHITE);
    }

    return game_result_running;
}

uint32_t Dino_Runner_Get_Score(void) { return g_score; }
uint8_t Dino_Runner_Is_Finished(void) { return g_state == dino_state_over; }
