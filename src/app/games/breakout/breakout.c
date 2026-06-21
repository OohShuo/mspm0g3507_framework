#include "breakout.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   320

#define BRICK_COLS      8
#define BRICK_ROWS      7
#define BRICK_WIDTH     25
#define BRICK_HEIGHT    8
#define BRICK_GAP_X     2
#define BRICK_GAP_Y     2
#define BRICK_AREA_W    (BRICK_COLS * BRICK_WIDTH + (BRICK_COLS - 1) * BRICK_GAP_X)
#define BRICK_AREA_H    (BRICK_ROWS * BRICK_HEIGHT + (BRICK_ROWS - 1) * BRICK_GAP_Y)
#define BRICK_OFFSET_X  ((SCREEN_WIDTH - BRICK_AREA_W) / 2)
#define BRICK_OFFSET_Y  56

#define PADDLE_WIDTH    40
#define PADDLE_HEIGHT   6
#define PADDLE_Y        292
#define PADDLE_STEP     5

#define BALL_SIZE       4
#define BALL_SPEED      3

#define COLOR_BLACK     0x0000u
#define COLOR_WHITE     0xffffu
#define COLOR_BORDER    0x07ffu
#define COLOR_PADDLE    0x07e0u
#define COLOR_BALL      0xffffu
#define COLOR_GAME_OVER 0xf81fu
#define COLOR_WIN       0x07ffu

/* Row colors: rainbow from top to bottom */
static const uint16_t g_brick_colors[BRICK_ROWS] = {
    0xf800u, /* red    */
    0xfd20u, /* orange */
    0xffe0u, /* yellow */
    0x07e0u, /* green  */
    0x07ffu, /* cyan   */
    0x001fu, /* blue   */
    0x8010u, /* purple */
};

typedef enum {
    breakout_state_serving,
    breakout_state_playing,
    breakout_state_over,
    breakout_state_win,
} Breakout_state;

static Game_hardware g_hardware;
static uint8_t g_bricks[BRICK_ROWS][BRICK_COLS];
static Breakout_state g_state = breakout_state_serving;
static int16_t g_paddle_x = 0;
static int16_t g_ball_x = 0;
static int16_t g_ball_y = 0;
static int8_t g_ball_dx = 0;
static int8_t g_ball_dy = 0;
static uint32_t g_score = 0;
static uint8_t g_lives = 0;
static uint8_t g_bricks_remaining = 0;
static uint32_t g_last_move = 0;
static uint32_t g_last_paddle_ms = 0;
static int32_t g_paddle_x256 = 0;

#define BASE_TICK_MS 20u

static void serve_ball(void) {
    g_ball_x = (int16_t)(g_paddle_x + PADDLE_WIDTH / 2 - BALL_SIZE / 2);
    g_ball_y = (int16_t)(PADDLE_Y - BALL_SIZE);
    g_ball_dx = 2;
    g_ball_dy = -BALL_SPEED;
}

static void render_brick(int8_t row, int8_t col) {
    const int32_t x = BRICK_OFFSET_X + col * (BRICK_WIDTH + BRICK_GAP_X);
    const int32_t y = BRICK_OFFSET_Y + row * (BRICK_HEIGHT + BRICK_GAP_Y);
    const uint16_t color = g_bricks[row][col] ? g_brick_colors[row] : COLOR_BLACK;
    if (color == COLOR_BLACK) {
        Game_Graphics_Fill_Rect(g_hardware.lcd, x, y, BRICK_WIDTH, BRICK_HEIGHT, COLOR_BLACK);
    } else {
        Game_Graphics_Fill_Rect(g_hardware.lcd, x, y, BRICK_WIDTH, BRICK_HEIGHT, color);
        Game_Graphics_Fill_Rect(g_hardware.lcd, (int32_t)(x + 1), (int32_t)(y + 1), BRICK_WIDTH - 2,
            BRICK_HEIGHT - 2, COLOR_BLACK);
        Game_Graphics_Fill_Rect(
            g_hardware.lcd, (int32_t)(x + 2), (int32_t)(y + 2), BRICK_WIDTH - 4, BRICK_HEIGHT - 4, color);
    }
}

static void render_all_bricks(void) {
    for (int8_t r = 0; r < BRICK_ROWS; r++) {
        for (int8_t c = 0; c < BRICK_COLS; c++) { render_brick(r, c); }
    }
}

static void render_paddle(int16_t old_x, int16_t new_x) {
    /* Erase old paddle trail */
    int16_t erase_start = old_x < new_x ? old_x : new_x;
    int16_t erase_end = old_x > new_x ? (int16_t)(old_x + PADDLE_WIDTH) : (int16_t)(new_x + PADDLE_WIDTH);
    Game_Graphics_Fill_Rect(g_hardware.lcd, erase_start, PADDLE_Y, (int32_t)(erase_end - erase_start),
        PADDLE_HEIGHT, COLOR_BLACK);
    /* Draw new paddle */
    Game_Graphics_Fill_Rect(g_hardware.lcd, new_x, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT, COLOR_PADDLE);
}

static void render_ball(int16_t old_x, int16_t old_y) {
    /* Erase old */
    Game_Graphics_Fill_Rect(g_hardware.lcd, old_x, old_y, BALL_SIZE, BALL_SIZE, COLOR_BLACK);
    /* Draw new */
    Game_Graphics_Fill_Rect(g_hardware.lcd, g_ball_x, g_ball_y, BALL_SIZE, BALL_SIZE, COLOR_BALL);
}

static void render_hud(void) {
    /* "SC:012345"=54px → x=179 (5px margin), "L:3"=18px → x=215 */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 179, 4, 59, 8, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_Text(g_hardware.lcd, 184, 4, "SC:", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 204, 4, g_score, 6, 1, COLOR_BORDER);
    Game_Graphics_Fill_Rect(g_hardware.lcd, 215, 16, 23, 8, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_Text(g_hardware.lcd, 220, 16, "L:", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 234, 16, g_lives, 1, 1, COLOR_PADDLE);
}

static void render_full(void) {
    Game_Graphics_Clear_Game_Area(g_hardware.lcd);
    render_all_bricks();
    render_paddle(g_paddle_x, g_paddle_x);
    render_ball(g_ball_x, g_ball_y);
    render_hud();
}

static int16_t clamp_paddle(int16_t x) {
    if (x < 0) { return 0; }
    if (x > SCREEN_WIDTH - PADDLE_WIDTH) { return (int16_t)(SCREEN_WIDTH - PADDLE_WIDTH); }
    return x;
}

static uint8_t ball_hits_brick(int16_t bx, int16_t by) {
    for (int8_t r = 0; r < BRICK_ROWS; r++) {
        for (int8_t c = 0; c < BRICK_COLS; c++) {
            if (!g_bricks[r][c]) { continue; }
            const int32_t brick_x = BRICK_OFFSET_X + c * (BRICK_WIDTH + BRICK_GAP_X);
            const int32_t brick_y = BRICK_OFFSET_Y + r * (BRICK_HEIGHT + BRICK_GAP_Y);
            if (bx + BALL_SIZE > brick_x && bx < brick_x + BRICK_WIDTH && by + BALL_SIZE > brick_y &&
                by < brick_y + BRICK_HEIGHT) {
                g_bricks[r][c] = 0;
                render_brick(r, c);
                g_score += (uint32_t)((BRICK_ROWS - r) * 10u);
                g_bricks_remaining--;
                return 1;
            }
        }
    }
    return 0;
}

static void restart_game(void) {
    for (int8_t r = 0; r < BRICK_ROWS; r++) {
        for (int8_t c = 0; c < BRICK_COLS; c++) { g_bricks[r][c] = 1; }
    }
    g_bricks_remaining = BRICK_ROWS * BRICK_COLS;
    g_score = 0;
    g_lives = 3;
    g_state = breakout_state_serving;
    g_paddle_x = (SCREEN_WIDTH - PADDLE_WIDTH) / 2;
    g_paddle_x256 = (int32_t)g_paddle_x * 256;
    serve_ball();
    g_last_move = Game_Runtime_Get_Tick_Ms();
    g_last_paddle_ms = g_last_move;
    render_full();
}

void Breakout_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

Game_result Breakout_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state == breakout_state_over || g_state == breakout_state_win) {
        if (input->confirm_pressed) { restart_game(); }
        return game_result_running;
    }

    /* Paddle movement — dt-scaled, independent of task frequency */
    {
        const uint32_t now = Game_Runtime_Get_Tick_Ms();
        uint32_t dt = now - g_last_paddle_ms;
        if (dt > 100u) { dt = 100u; }
        g_last_paddle_ms = now;

        int32_t new_x256 = g_paddle_x256;
        if (input->direction == game_direction_left) {
            new_x256 -= (int32_t)PADDLE_STEP * 256 * (int32_t)dt / (int32_t)BASE_TICK_MS;
        } else if (input->direction == game_direction_right) {
            new_x256 += (int32_t)PADDLE_STEP * 256 * (int32_t)dt / (int32_t)BASE_TICK_MS;
        }

        const int32_t max_x256 = (SCREEN_WIDTH - PADDLE_WIDTH) * 256;
        if (new_x256 < 0) { new_x256 = 0; }
        if (new_x256 > max_x256) { new_x256 = max_x256; }
        const int16_t new_x = clamp_paddle((int16_t)(new_x256 / 256));
        if (new_x != g_paddle_x) {
            render_paddle(g_paddle_x, new_x);
            g_paddle_x = new_x;
            if (g_state == breakout_state_serving) {
                g_ball_x = (int16_t)(g_paddle_x + PADDLE_WIDTH / 2 - BALL_SIZE / 2);
            }
        }
        g_paddle_x256 = new_x256;
    }

    /* Launch ball */
    if (g_state == breakout_state_serving) {
        if (input->confirm_pressed) {
            g_state = breakout_state_playing;
            serve_ball();
            render_hud();
        }
        return game_result_running;
    }

    /* Move ball */
    const uint32_t now = Game_Runtime_Get_Tick_Ms();
    if (now - g_last_move < 20u) { return game_result_running; }
    g_last_move = now;

    const int16_t old_ball_x = g_ball_x;
    const int16_t old_ball_y = g_ball_y;
    g_ball_x = (int16_t)(g_ball_x + g_ball_dx);
    g_ball_y = (int16_t)(g_ball_y + g_ball_dy);

    /* Wall collisions */
    if (g_ball_x <= 0) {
        g_ball_x = 0;
        g_ball_dx = (int8_t)-g_ball_dx;
    } else if (g_ball_x + BALL_SIZE >= SCREEN_WIDTH) {
        g_ball_x = (int16_t)(SCREEN_WIDTH - BALL_SIZE);
        g_ball_dx = (int8_t)-g_ball_dx;
    }
    if (g_ball_y <= GAME_TOP_BAR_H) {
        g_ball_y = (int16_t)GAME_TOP_BAR_H;
        g_ball_dy = (int8_t)-g_ball_dy;
    }

    /* Brick collision */
    if (ball_hits_brick(g_ball_x, g_ball_y) || ball_hits_brick(g_ball_x, old_ball_y) ||
        ball_hits_brick(old_ball_x, g_ball_y)) {
        g_ball_dy = (int8_t)-g_ball_dy;
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_breakout_bounce);
        if (g_bricks_remaining == 0) {
            g_state = breakout_state_win;
            g_score += 1000u;
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_victory);
            Vib_Motor_Play_Effect(g_hardware.vib_motor, vib_effect_victory);
            render_hud();
        } else {
            Vib_Motor_Play_Effect(g_hardware.vib_motor, vib_effect_hit_light);
        }
    }

    /* Paddle collision */
    if (g_ball_y + BALL_SIZE >= PADDLE_Y && g_ball_y + BALL_SIZE <= PADDLE_Y + PADDLE_HEIGHT + 4 &&
        g_ball_x + BALL_SIZE > g_paddle_x && g_ball_x < g_paddle_x + PADDLE_WIDTH && g_ball_dy > 0) {
        /* Angle based on hit position */
        const int16_t hit_pos = (int16_t)(g_ball_x + BALL_SIZE / 2 - g_paddle_x - PADDLE_WIDTH / 2);
        g_ball_dx = (int8_t)(hit_pos / 4);
        if (g_ball_dx == 0) { g_ball_dx = 1; }
        g_ball_dy = (int8_t)-g_ball_dy;
        g_ball_y = (int16_t)(PADDLE_Y - BALL_SIZE);
    }

    /* Ball falls off bottom */
    if (g_ball_y > GAME_AREA_BOTTOM) {
        if (g_lives > 1) {
            g_lives--;
            g_state = breakout_state_serving;
            const int16_t old_x = g_paddle_x;
            g_paddle_x = (SCREEN_WIDTH - PADDLE_WIDTH) / 2;
            g_paddle_x256 = (int32_t)g_paddle_x * 256;
            render_paddle(old_x, g_paddle_x);
            serve_ball();
            render_hud();
        } else {
            g_lives = 0;
            g_state = breakout_state_over;
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_defeat);
            Vib_Motor_Play_Effect(g_hardware.vib_motor, vib_effect_defeat);
            render_hud();
        }
        return game_result_running;
    }

    render_ball(old_ball_x, old_ball_y);
    return game_result_running;
}

uint32_t Breakout_Get_Score(void) { return g_score; }

uint8_t Breakout_Is_Finished(void) { return g_state == breakout_state_over || g_state == breakout_state_win; }
