#include "pong.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   320
#define PLAY_AREA_TOP   44

#define PADDLE_WIDTH    6
#define PADDLE_HEIGHT   48
#define PADDLE_STEP     4
#define PADDLE_LEFT_X   6
#define PADDLE_RIGHT_X  (SCREEN_WIDTH - PADDLE_WIDTH - 6)

#define BALL_SIZE       4
#define BALL_SPEED      3

#define WIN_SCORE       5

#define CENTER_LINE_Y   (PLAY_AREA_TOP + (SCREEN_HEIGHT - PLAY_AREA_TOP) / 2)

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

static void serve_ball(uint8_t toward_player) {
    g_ball_x = SCREEN_WIDTH / 2 - BALL_SIZE / 2;
    g_ball_y = PLAY_AREA_TOP + (SCREEN_HEIGHT - PLAY_AREA_TOP) / 2 - BALL_SIZE / 2;
    g_ball_dx = toward_player ? (int8_t)-BALL_SPEED : BALL_SPEED;
    g_ball_dy = 0;
}

static int16_t clamp_paddle(int16_t y) {
    if (y < PLAY_AREA_TOP) { return PLAY_AREA_TOP; }
    if (y > SCREEN_HEIGHT - PADDLE_HEIGHT) { return (int16_t)(SCREEN_HEIGHT - PADDLE_HEIGHT); }
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
        if (yy >= PLAY_AREA_TOP && yy < SCREEN_HEIGHT && (yy / 8) % 2 == 0) {
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
    if (old_y >= PLAY_AREA_TOP && old_y < SCREEN_HEIGHT && (old_y / 8) % 2 == 0) {
        Game_Graphics_Fill_Rect(g_hardware.lcd, SCREEN_WIDTH / 2 - 1, old_y, 2, 1, COLOR_WHITE);
    }
    Game_Graphics_Fill_Rect(g_hardware.lcd, g_ball_x, g_ball_y, BALL_SIZE, BALL_SIZE, COLOR_WHITE);
}

static void draw_center_line(void) {
    for (int16_t y = PLAY_AREA_TOP; y < SCREEN_HEIGHT; y++) {
        if ((y / 8) % 2 == 0) {
            Game_Graphics_Fill_Rect(g_hardware.lcd, SCREEN_WIDTH / 2 - 1, y, 2, 1, COLOR_WHITE);
        }
    }
}

static void render_hud(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, PLAY_AREA_TOP, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 16, 10, "YOU", 2, COLOR_CYAN);
    Game_Graphics_Draw_U32(g_hardware.lcd, 90, 10, g_player_score, 1, 2, COLOR_CYAN);
    Game_Graphics_Draw_Text(g_hardware.lcd, 140, 10, "AI", 2, COLOR_YELLOW);
    Game_Graphics_Draw_U32(g_hardware.lcd, 188, 10, g_ai_score, 1, 2, COLOR_YELLOW);

    /* Clear status text rows (7px glyphs at y=284 and y=300) */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 284, SCREEN_WIDTH, 7, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 300, SCREEN_WIDTH, 7, COLOR_BLACK);
    if (g_state == pong_state_serving) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 53, 300, "PRESS TO SERVE", 1, COLOR_WHITE);
    } else if (g_state == pong_state_over) {
        const uint8_t won = g_player_score >= WIN_SCORE;
        Game_Graphics_Draw_Text(g_hardware.lcd, won ? 79 : 55, 300, won ? "YOU WIN" : "AI WINS", 1,
            won ? COLOR_WIN : COLOR_GAME_OVER);
        Game_Graphics_Draw_Text(g_hardware.lcd, won ? 40 : 40, 284, "PRESS RESTART", 1, COLOR_WHITE);
    } else {
        Game_Graphics_Draw_Text(g_hardware.lcd, 58, 300, "HOLD FOR MENU", 1, COLOR_WHITE);
    }
}

static void render_full(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
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
    g_player_y = PLAY_AREA_TOP + (SCREEN_HEIGHT - PLAY_AREA_TOP) / 2 - PADDLE_HEIGHT / 2;
    g_ai_y = g_player_y;
    g_old_player_y = g_player_y;
    g_old_ai_y = g_ai_y;
    g_last_move = Bsp_Get_Tick_Ms();
    g_serve_at = g_last_move + 800u;
    serve_ball(0);
    render_full();
    Buzzer_Play_Music(g_hardware.buzzer, music_idx_racing_theme, 1);
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
        if (input->confirm_pressed) {
            Buzzer_Stop(g_hardware.buzzer);
            restart_game();
        }
        return game_result_running;
    }

    /* Player paddle */
    g_old_player_y = g_player_y;
    if (input->direction == game_direction_up) {
        g_player_y = clamp_paddle((int16_t)(g_player_y - PADDLE_STEP));
    } else if (input->direction == game_direction_down) {
        g_player_y = clamp_paddle((int16_t)(g_player_y + PADDLE_STEP));
    }
    if (g_player_y != g_old_player_y) {
        render_paddle(PADDLE_LEFT_X, g_old_player_y, g_player_y, COLOR_GREEN);
    }

    /* Serve */
    if (g_state == pong_state_serving) {
        const uint32_t now = Bsp_Get_Tick_Ms();
        if (input->confirm_pressed || now >= g_serve_at) {
            g_state = pong_state_playing;
            render_hud();
        }
        return game_result_running;
    }

    /* AI paddle - follows ball with lag and error */
    g_old_ai_y = g_ai_y;
    const int16_t ai_center = (int16_t)(g_ai_y + PADDLE_HEIGHT / 2);
    const int16_t ball_center = (int16_t)(g_ball_y + BALL_SIZE / 2);
    if (ball_center < ai_center - 6) {
        g_ai_y = clamp_paddle((int16_t)(g_ai_y - PADDLE_STEP + 1));
    } else if (ball_center > ai_center + 6) {
        g_ai_y = clamp_paddle((int16_t)(g_ai_y + PADDLE_STEP - 1));
    }
    if (g_ai_y != g_old_ai_y) { render_paddle(PADDLE_RIGHT_X, g_old_ai_y, g_ai_y, COLOR_RED); }

    /* Move ball */
    const uint32_t now2 = Bsp_Get_Tick_Ms();
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
    } else if (g_ball_y + BALL_SIZE >= SCREEN_HEIGHT) {
        g_ball_y = (int16_t)(SCREEN_HEIGHT - BALL_SIZE);
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
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_menu_move);
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
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_explosion);
        if (g_ai_score >= WIN_SCORE) {
            g_state = pong_state_over;
            Buzzer_Play_Music(g_hardware.buzzer, music_idx_defeat, 0);
        } else {
            g_state = pong_state_serving;
            g_serve_at = now2 + 1000u;
            serve_ball(0);
        }
        render_hud();
        return game_result_running;
    }

    if (g_ball_x > SCREEN_WIDTH) {
        /* Player scores */
        g_player_score++;
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_air_pickup);
        if (g_player_score >= WIN_SCORE) {
            g_state = pong_state_over;
            Buzzer_Play_Music(g_hardware.buzzer, music_idx_victory, 0);
        } else {
            g_state = pong_state_serving;
            g_serve_at = now2 + 1000u;
            serve_ball(1);
        }
        render_hud();
        return game_result_running;
    }

    render_ball(old_ball_x, old_ball_y);
    return game_result_running;
}

uint32_t Pong_Get_Score(void) { return (uint32_t)(g_player_score * 100u); }

uint8_t Pong_Is_Finished(void) { return g_state == pong_state_over; }
