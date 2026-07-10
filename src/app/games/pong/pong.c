#include "pong.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   320
#define PLAY_AREA_TOP   GAME_TOP_BAR_H

#define PADDLE_WIDTH    6
#define PADDLE_HEIGHT   48
#define PADDLE_STEP     4
#define PADDLE_LEFT_X   6
#define PADDLE_RIGHT_X  (SCREEN_WIDTH - PADDLE_WIDTH - 6)

#define BALL_SIZE       4
#define BALL_SPEED      3

#define WIN_SCORE       5

#define CENTER_LINE_Y   (PLAY_AREA_TOP + (GAME_AREA_BOTTOM - PLAY_AREA_TOP) / 2)

#define COLOR_BLACK     0x0000u
#define COLOR_WHITE     0xffffu
#define COLOR_CYAN      0x07ffu
#define COLOR_YELLOW    0xffe0u
#define COLOR_RED       0xf800u
#define COLOR_GREEN     0x07e0u
#define COLOR_GAME_OVER 0xf81fu
#define COLOR_WIN       0x07ffu

typedef enum {
    pong_state_serving,
    pong_state_playing,
    pong_state_over,
} Pong_state;

static Game_hardware g_hardware;
static Pong_state g_state = pong_state_serving;
static int16_t g_player_y = 0;
static int16_t g_ai_y = 0;
static int16_t g_old_player_y = 0;
static int16_t g_old_ai_y = 0;
static int16_t g_ball_x = 0;
static int16_t g_ball_y = 0;
static int8_t g_ball_dx = 0;
static int8_t g_ball_dy = 0;
static uint8_t g_player_score = 0;
static uint8_t g_ai_score = 0;
static uint32_t g_last_move = 0;
static uint32_t g_serve_at = 0;
static uint32_t g_last_paddle_ms = 0;
static int32_t g_player_y256 = 0;
static int32_t g_ai_y256 = 0;

#define BASE_TICK_MS 20u

static void serve_ball(uint8_t toward_player) {
    g_ball_x = SCREEN_WIDTH / 2 - BALL_SIZE / 2;
    g_ball_y = PLAY_AREA_TOP + (GAME_AREA_BOTTOM - PLAY_AREA_TOP) / 2 - BALL_SIZE / 2;
    g_ball_dx = toward_player ? (int8_t)-BALL_SPEED : BALL_SPEED;
    g_ball_dy = 0;
}

static int16_t clamp_paddle(int16_t y) {
    if (y < PLAY_AREA_TOP) { return PLAY_AREA_TOP; }
    if (y > GAME_AREA_BOTTOM - PADDLE_HEIGHT) { return (int16_t)(GAME_AREA_BOTTOM - PADDLE_HEIGHT); }
    return y;
}

static void render_paddle(int16_t x, int16_t old_y, int16_t new_y, uint16_t color) {
    /* Erase old paddle area */
    int16_t erase_start = old_y < new_y ? old_y : new_y;
    int16_t erase_end = old_y > new_y ? (int16_t)(old_y + PADDLE_HEIGHT) : (int16_t)(new_y + PADDLE_HEIGHT);
    Game_Graphics_Fill_Rect(
        g_hardware.lcd, x, erase_start, PADDLE_WIDTH, (int32_t)(erase_end - erase_start), COLOR_BLACK);
    /* Redraw center line in erased area if needed */
    for (int16_t yy = erase_start; yy < erase_end; yy++) {
        if (yy >= PLAY_AREA_TOP && yy < GAME_AREA_BOTTOM && (yy / 8) % 2 == 0) {
            Game_Graphics_Fill_Rect(g_hardware.lcd, SCREEN_WIDTH / 2 - 1, yy, 2, 1, COLOR_WHITE);
        }
    }
    /* Draw new paddle */
    Game_Graphics_Fill_Rect(g_hardware.lcd, x, new_y, PADDLE_WIDTH, PADDLE_HEIGHT, color);
    /* Paddle highlight */
    Game_Graphics_Fill_Rect(g_hardware.lcd, (int32_t)(x + 1), (int32_t)(new_y + PADDLE_HEIGHT / 2 - 6),
        PADDLE_WIDTH - 2, 12, COLOR_WHITE);
}

static void render_ball(int16_t old_x, int16_t old_y) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, old_x, old_y, BALL_SIZE, BALL_SIZE, COLOR_BLACK);
    /* Redraw center line behind erased ball */
    if (old_y >= PLAY_AREA_TOP && old_y < GAME_AREA_BOTTOM && (old_y / 8) % 2 == 0) {
        Game_Graphics_Fill_Rect(g_hardware.lcd, SCREEN_WIDTH / 2 - 1, old_y, 2, 1, COLOR_WHITE);
    }
    Game_Graphics_Fill_Rect(g_hardware.lcd, g_ball_x, g_ball_y, BALL_SIZE, BALL_SIZE, COLOR_WHITE);
}

static void draw_center_line(void) {
    for (int16_t y = PLAY_AREA_TOP; y < GAME_AREA_BOTTOM; y++) {
        if ((y / 8) % 2 == 0) {
            Game_Graphics_Fill_Rect(g_hardware.lcd, SCREEN_WIDTH / 2 - 1, y, 2, 1, COLOR_WHITE);
        }
    }
}

static void render_hud(void) {
    /* "YOU 0"=30px → x=203, "AI 0"=24px → x=209 (5px margin) */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 203, 4, 35, 8, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_Text(g_hardware.lcd, 208, 4, "YOU", 1, COLOR_CYAN);
    Game_Graphics_Draw_U32(g_hardware.lcd, 232, 4, g_player_score, 1, 1, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_hardware.lcd, 209, 16, 29, 8, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_Text(g_hardware.lcd, 214, 16, "AI", 1, COLOR_YELLOW);
    Game_Graphics_Draw_U32(g_hardware.lcd, 232, 16, g_ai_score, 1, 1, COLOR_YELLOW);
}

static void render_full(void) {
    Game_Graphics_Clear_Game_Area(g_hardware.lcd);
    draw_center_line();
    render_paddle(PADDLE_LEFT_X, g_player_y, g_player_y, COLOR_GREEN);
    render_paddle(PADDLE_RIGHT_X, g_ai_y, g_ai_y, COLOR_RED);
    render_ball(g_ball_x, g_ball_y);
    render_hud();
}

static void restart_game(void) {
    g_player_score = 0;
    g_ai_score = 0;
    g_state = pong_state_serving;
    g_player_y = PLAY_AREA_TOP + (GAME_AREA_BOTTOM - PLAY_AREA_TOP) / 2 - PADDLE_HEIGHT / 2;
    g_ai_y = g_player_y;
    g_old_player_y = g_player_y;
    g_old_ai_y = g_ai_y;
    g_player_y256 = (int32_t)g_player_y * 256;
    g_ai_y256 = (int32_t)g_ai_y * 256;
    g_last_move = Game_Runtime_Get_Tick_Ms();
    g_last_paddle_ms = g_last_move;
    g_serve_at = g_last_move + 800u;
    serve_ball(0);
    render_full();
}

void Pong_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

Game_result Pong_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state == pong_state_over) {
        return g_player_score > g_ai_score ? game_result_won : game_result_lost;
    }

    /* ── Paddle dt (shared by player + AI) ── */
    const uint32_t now = Game_Runtime_Get_Tick_Ms();
    uint32_t dt = now - g_last_paddle_ms;
    if (dt > 100u) { dt = 100u; }
    g_last_paddle_ms = now;
    const int32_t dt_step256 = (int32_t)PADDLE_STEP * 256 * (int32_t)dt / (int32_t)BASE_TICK_MS;

    /* Player paddle — dt-scaled */
    g_old_player_y = g_player_y;
    {
        int32_t new_y256 = g_player_y256;
        if (input->direction == game_direction_up) {
            new_y256 -= dt_step256;
        } else if (input->direction == game_direction_down) {
            new_y256 += dt_step256;
        }

        const int32_t min_y256 = PLAY_AREA_TOP * 256;
        const int32_t max_y256 = (GAME_AREA_BOTTOM - PADDLE_HEIGHT) * 256;
        if (new_y256 < min_y256) { new_y256 = min_y256; }
        if (new_y256 > max_y256) { new_y256 = max_y256; }
        const int16_t new_y = clamp_paddle((int16_t)(new_y256 / 256));
        if (new_y != g_player_y) {
            render_paddle(PADDLE_LEFT_X, g_old_player_y, new_y, COLOR_GREEN);
            g_player_y = new_y;
        }
        g_player_y256 = new_y256;
    }

    /* Serve */
    if (g_state == pong_state_serving) {
        const uint32_t now = Game_Runtime_Get_Tick_Ms();
        if (input->confirm_pressed || now >= g_serve_at) {
            g_state = pong_state_playing;
            render_hud();
        }
        return game_result_running;
    }

    /* AI paddle — follows ball with lag and error, dt-scaled */
    g_old_ai_y = g_ai_y;
    {
        const int16_t ai_center = (int16_t)(g_ai_y + PADDLE_HEIGHT / 2);
        const int16_t ball_center = (int16_t)(g_ball_y + BALL_SIZE / 2);
        const int32_t ai_step256 = dt_step256 * (PADDLE_STEP - 1) / PADDLE_STEP;

        int32_t new_y256 = g_ai_y256;
        if (ball_center < ai_center - 6) {
            new_y256 -= ai_step256;
        } else if (ball_center > ai_center + 6) {
            new_y256 += ai_step256;
        }

        const int32_t min_y256 = PLAY_AREA_TOP * 256;
        const int32_t max_y256 = (GAME_AREA_BOTTOM - PADDLE_HEIGHT) * 256;
        if (new_y256 < min_y256) { new_y256 = min_y256; }
        if (new_y256 > max_y256) { new_y256 = max_y256; }
        const int16_t new_y = clamp_paddle((int16_t)(new_y256 / 256));
        if (new_y != g_ai_y) {
            render_paddle(PADDLE_RIGHT_X, g_old_ai_y, new_y, COLOR_RED);
            g_ai_y = new_y;
        }
        g_ai_y256 = new_y256;
    }

    /* Move ball */
    const uint32_t now2 = Game_Runtime_Get_Tick_Ms();
    if (now2 - g_last_move < 18u) { return game_result_running; }
    g_last_move = now2;

    const int16_t old_ball_x = g_ball_x;
    const int16_t old_ball_y = g_ball_y;
    g_ball_x = (int16_t)(g_ball_x + g_ball_dx);
    g_ball_y = (int16_t)(g_ball_y + g_ball_dy);

    /* Top/bottom wall bounce */
    if (g_ball_y <= PLAY_AREA_TOP) {
        g_ball_y = PLAY_AREA_TOP;
        g_ball_dy = (int8_t)-g_ball_dy;
    } else if (g_ball_y + BALL_SIZE >= GAME_AREA_BOTTOM) {
        g_ball_y = (int16_t)(GAME_AREA_BOTTOM - BALL_SIZE);
        g_ball_dy = (int8_t)-g_ball_dy;
    }

    /* Left paddle (player) collision */
    if (g_ball_x <= PADDLE_LEFT_X + PADDLE_WIDTH && g_ball_x + BALL_SIZE >= PADDLE_LEFT_X &&
        g_ball_y + BALL_SIZE > g_player_y && g_ball_y < g_player_y + PADDLE_HEIGHT) {
        g_ball_x = PADDLE_LEFT_X + PADDLE_WIDTH;
        g_ball_dx = BALL_SPEED;
        /* Add spin based on hit position */
        const int16_t hit_pos = (int16_t)(g_ball_y + BALL_SIZE / 2 - g_player_y - PADDLE_HEIGHT / 2);
        g_ball_dy = (int8_t)(hit_pos / 6);
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_pong_paddle);
    }

    /* Right paddle (AI) collision */
    if (g_ball_x + BALL_SIZE >= PADDLE_RIGHT_X && g_ball_x <= PADDLE_RIGHT_X + PADDLE_WIDTH &&
        g_ball_y + BALL_SIZE > g_ai_y && g_ball_y < g_ai_y + PADDLE_HEIGHT) {
        g_ball_x = (int16_t)(PADDLE_RIGHT_X - BALL_SIZE);
        g_ball_dx = (int8_t)-BALL_SPEED;
        const int16_t hit_pos = (int16_t)(g_ball_y + BALL_SIZE / 2 - g_ai_y - PADDLE_HEIGHT / 2);
        g_ball_dy = (int8_t)(hit_pos / 6);
    }

    /* Scoring */
    if (g_ball_x + BALL_SIZE < 0) {
        /* AI scores */
        g_ai_score++;
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_pong_score);
        if (g_ai_score >= WIN_SCORE) {
            g_state = pong_state_over;
        } else {
            Vib_Motor_Gpio_Play_Effect(g_hardware.vib_motor, vib_effect_score);
            g_state = pong_state_serving;
            g_serve_at = now2 + 1000u;
            serve_ball(0);
        }
        render_hud();
        return g_state == pong_state_over ? game_result_lost : game_result_running;
    }

    if (g_ball_x > SCREEN_WIDTH) {
        /* Player scores */
        g_player_score++;
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_pong_score);
        if (g_player_score >= WIN_SCORE) {
            g_state = pong_state_over;
        } else {
            Vib_Motor_Gpio_Play_Effect(g_hardware.vib_motor, vib_effect_score);
            g_state = pong_state_serving;
            g_serve_at = now2 + 1000u;
            serve_ball(1);
        }
        render_hud();
        return g_state == pong_state_over ? game_result_won : game_result_running;
    }

    render_ball(old_ball_x, old_ball_y);
    return game_result_running;
}

uint32_t Pong_Get_Score(void) { return (uint32_t)(g_player_score * 100u); }
