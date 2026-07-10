#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"
#include "game_registry.h"

#define SCREEN_WIDTH      240
#define SCREEN_HEIGHT     320
#define PLAY_TOP          GAME_TOP_BAR_H
#define PLAY_BOT          GAME_AREA_BOTTOM
#define PLAY_H            (PLAY_BOT - PLAY_TOP)

#define BIRD_X            50
#define BIRD_W            16
#define BIRD_H            10
#define GROUND_Y          290
#define FLAP_VELOCITY     -5
#define PIPE_W            20
#define PIPE_GAP          56
#define PIPE_SPACING      150
#define MAX_PIPES         4
#define GLIDE_DURATION_MS 3000u
#define GLIDE_COOLDOWN_MS 5000u
#define GLIDE_FALL_TICKS  3u

#define COLOR_BLACK       0x0000u
#define COLOR_WHITE       0xffffu
#define COLOR_GREEN       0x07e0u
#define COLOR_CYAN        0x07ffu
#define COLOR_YELLOW      0xffe0u
#define COLOR_RED         0xf800u
#define COLOR_DARK        0xA514u
#define COLOR_GRAY        0x8410u

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
static Game_rng g_rng;
static uint32_t g_start_ms;
static uint32_t g_last_tick;
static uint32_t g_glide_end_ms;
static uint32_t g_glide_ready_ms;
static uint8_t g_gliding;
static uint8_t g_glide_fall_acc;
static uint8_t g_glide_ui_state;
static uint8_t g_glide_ui_seconds;

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

static void update_score(void) {
    if (g_state == flappy_state_ready) { return; }
    /* 3 digits = 18px → x=215 (5px margin) */
    bar_fill(215, 2, 23, 8, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_U32(g_hardware.lcd, 220, 2, g_score, 3, 1, COLOR_CYAN);
}

static void update_glide_status(uint32_t now) {
    uint8_t state = 0;
    uint8_t seconds = 0;
    if (g_gliding) {
        state = 1;
        seconds = (uint8_t)((g_glide_end_ms - now + 999u) / 1000u);
    } else if (now < g_glide_ready_ms) {
        state = 2;
        seconds = (uint8_t)((g_glide_ready_ms - now + 999u) / 1000u);
    }
    if (state == g_glide_ui_state && seconds == g_glide_ui_seconds) { return; }

    g_glide_ui_state = state;
    g_glide_ui_seconds = seconds;
    bar_fill(148, 16, 61, 8, GAME_BAR_COLOR_BG);
    if (state == 1) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 153, 16, "GLD:", 1, COLOR_YELLOW);
        Game_Graphics_Draw_U32(g_hardware.lcd, 177, 16, seconds, 1, 1, COLOR_WHITE);
    } else if (state == 2) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 153, 16, "CD:", 1, COLOR_GRAY);
        Game_Graphics_Draw_U32(g_hardware.lcd, 171, 16, seconds, 1, 1, COLOR_WHITE);
    } else {
        Game_Graphics_Draw_Text(g_hardware.lcd, 153, 16, "Y:READY", 1, COLOR_CYAN);
    }
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
            g_pipes[i].gap_y = (uint16_t)(60 + Game_Rng_Range(&g_rng, GROUND_Y - PIPE_GAP - 120));
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
    /* Require A to be released after entering/replaying before it can start the run. */
    g_up_prev = 1;
    g_ground_offset = 0;
    g_score = 0;
    g_old_score = 0;
    g_pipe_timer = 0;
    g_speed_acc = 0;
    g_last_tick = 0;
    g_glide_end_ms = 0;
    g_glide_ready_ms = 0;
    g_gliding = 0;
    g_glide_fall_acc = 0;
    g_glide_ui_state = 0xffu;
    g_glide_ui_seconds = 0xffu;
    Game_Rng_Seed(&g_rng, Game_Runtime_Get_Tick_Ms() ^ 0xAD90777Du);
    for (uint8_t i = 0; i < MAX_PIPES; i++) { g_pipes[i].active = 0; }

    Game_Graphics_Clear_Game_Area(g_hardware.lcd);
    draw_ground();
    draw_bird(g_bird_y);
    Game_Graphics_Draw_Text(g_hardware.lcd, 58, 140, "A TO START", 1, COLOR_WHITE);
    update_glide_status(Game_Runtime_Get_Tick_Ms());
}

static void flappy_bird_init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

/* ── Update ── */

static Game_result flappy_bird_update(const Game_input* input) {
    uint8_t flapped = 0;
    uint8_t scored = 0;
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }
    if (g_state == flappy_state_over) { return game_result_lost; }

    if (g_state == flappy_state_ready) {
        int16_t bob = (int16_t)((Game_Runtime_Get_Tick_Ms() / 300u) % 2u);
        int16_t by = (int16_t)(160 + bob * 4 - 2);
        if (g_bird_y != by) { play_fill(BIRD_X - 1, g_old_bird_y - 1, BIRD_W + 3, BIRD_H + 2, COLOR_BLACK); }
        g_old_bird_y = g_bird_y;
        g_bird_y = by;
        if (g_bird_y != g_old_bird_y) { draw_bird(g_bird_y); }
        uint8_t up = input->a_down;
        if (up && !g_up_prev) {
            g_state = flappy_state_playing;
            g_start_ms = Game_Runtime_Get_Tick_Ms();
            g_last_tick = 0;
            g_bird_vy = FLAP_VELOCITY;
            g_gravity_acc = 0;
            play_fill(20, 130, SCREEN_WIDTH - 40, 20, COLOR_BLACK);
            update_score();
            Vib_Motor_Gpio_Play_Effect(g_hardware.vib_motor, vib_effect_jump);
        }
        g_up_prev = up;
        return game_result_running;
    }

    /* ═══ Playing ═══ */

    const uint32_t now = Game_Runtime_Get_Tick_Ms();
    if (g_gliding && (!input->y_down || now >= g_glide_end_ms)) {
        const uint32_t glide_stop_ms = now >= g_glide_end_ms ? g_glide_end_ms : now;
        g_gliding = 0;
        g_glide_ready_ms = glide_stop_ms + GLIDE_COOLDOWN_MS;
        g_bird_vy = 0;
        g_gravity_acc = 0;
    }
    if (input->y_pressed && !g_gliding && now >= g_glide_ready_ms) {
        g_gliding = 1;
        g_glide_end_ms = now + GLIDE_DURATION_MS;
        g_glide_fall_acc = 0;
        g_bird_vy = 0;
        g_gravity_acc = 0;
    }
    update_glide_status(now);

    /* ── Input (once per real call, applied to upcoming tick) ── */
    {
        uint8_t up = input->a_down;
        if (!g_gliding && up && !g_up_prev) {
            g_bird_vy = FLAP_VELOCITY;
            g_gravity_acc = 0;
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_flappy_flap);
            flapped = 1;
        }
        g_up_prev = up;
    }

    /* ── Tick-accumulator: run one logic tick per 20 ms of elapsed time ── */
    const uint32_t elapsed = now - g_start_ms;
    uint32_t effective_ticks = elapsed / BASE_TICK_MS;
    /* Cap catch-up to prevent spiral on stall */
    if (effective_ticks - g_last_tick > 5u) { effective_ticks = g_last_tick + 5u; }

    const int16_t bird_before = g_bird_y;

    while (g_last_tick < effective_ticks) {
        g_last_tick++;
        const uint32_t tick_ms = g_last_tick * BASE_TICK_MS;

        /* Gravity / glide */
        if (g_gliding) {
            if (++g_glide_fall_acc >= GLIDE_FALL_TICKS) {
                g_glide_fall_acc = 0;
                g_bird_y++;
            }
        } else {
            g_gravity_acc += 1u;
            if (g_gravity_acc >= 2u) {
                g_gravity_acc -= 2u;
                g_bird_vy++;
            }
            g_bird_y += g_bird_vy;
        }
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
                        scored = 1;
                    }
                }
            }
        }

        /* Pipe spawn */
        if (g_pipe_timer == 0) {
            spawn_pipe();
            g_pipe_timer = PIPE_SPACING / (speed_scaled(tick_ms) >> 8) + Game_Rng_Range(&g_rng, 30u);
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
        update_score();
        return game_result_lost;
    } else if (scored) {
        Vib_Motor_Gpio_Play_Effect(g_hardware.vib_motor, vib_effect_score);
    } else if (flapped) {
        Vib_Motor_Gpio_Play_Effect(g_hardware.vib_motor, vib_effect_jump);
    }

    return game_result_running;
}

static uint32_t flappy_bird_get_score(void) { return g_score; }

static void flappy_bird_draw_icon(St7789* lcd, int32_t x, int32_t y) {
    x += 0;
    y += 1;
    /* Ground */
    Game_Graphics_Fill_Rect(lcd, x + 2, y + 34, 44, 2, 0xA514u);
    /* Bird body */
    Game_Graphics_Fill_Rect(lcd, x + 14, y + 14, 10, 8, 0xffe0u);
    /* Beak */
    Game_Graphics_Fill_Rect(lcd, x + 24, y + 16, 4, 2, 0xf800u);
    /* Eye */
    Game_Graphics_Fill_Rect(lcd, x + 22, y + 14, 2, 2, 0xffffu);
    /* Wing */
    Game_Graphics_Fill_Rect(lcd, x + 12, y + 16, 4, 3, 0xA514u);
    /* Top pipe */
    Game_Graphics_Fill_Rect(lcd, x + 4, y + 2, 10, 10, 0x07e0u);
    Game_Graphics_Fill_Rect(lcd, x + 2, y + 9, 14, 3, 0x07e0u);
    /* Bottom pipe */
    Game_Graphics_Fill_Rect(lcd, x + 34, y + 20, 10, 14, 0x07e0u);
    Game_Graphics_Fill_Rect(lcd, x + 32, y + 20, 14, 3, 0x07e0u);
}

const Game_descriptor game_flappy_bird_entry = {
    .draw_icon = flappy_bird_draw_icon,
    .name_color = 0x07ffu,
    .name = "FLAPPY",
    .id = game_id_flappy_bird,
    .control_hint = "A FLAP Y GLIDE",
    .info_text =
        "DESCRIPTION\nFly between narrow pipe gaps.\nGlide briefly when ready.\n\nGOAL\nPass as "
        "many pipes as possible.\n\nCONTROLS\nA FLAP\nY GLIDE\nX/B PAUSE",
    .is_game = 1,
    .init = flappy_bird_init,
    .update = flappy_bird_update,
    .get_score = flappy_bird_get_score,
};
