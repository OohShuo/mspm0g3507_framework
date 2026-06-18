#include "game_console.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_config.h"
#include "board_config.h"
#include "bsp_time.h"
#include "button.h"
#include "buzzer.h"
#include "game_graphics.h"
#include "game_over_menu.h"
#include "game_registry.h"
#include "game_runtime.h"
#include "joystick.h"
#include "rtt_log.h"
#include "score_store.h"
#include "screensaver.h"
#include "st7789.h"
#include "task.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define BACK_HOLD_MS  1000u

/* ── 3×2 grid menu layout ── */
#define MENU_COLS     3u
#define MENU_ROWS     2u
#define MENU_PER_PAGE (MENU_COLS * MENU_ROWS)
#define CELL_W        68
#define CELL_H        88
#define CELL_GAP_X    8
#define CELL_GAP_Y    10
#define GRID_X0       10
#define GRID_Y0       30

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_BLUE    0x0010u
#define COLOR_YELLOW  0xffe0u
#define COLOR_GREEN   0x07e0u
#define COLOR_RED     0xf800u
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0x4208u

typedef enum {
    console_state_menu,
    console_state_game,
    console_state_game_over,
} Console_state;

static St7789* g_lcd = NULL;
static Joystick* g_joystick = NULL;
static Button* g_confirm_button = NULL;
static Buzzer* g_buzzer = NULL;

static Console_state g_console_state = console_state_menu;
static Game_direction g_last_direction = game_direction_none;
static Button_state g_last_button_state = button_state_up;
static uint32_t g_button_down_time = 0;
static uint8_t g_back_sent = 0;
static uint8_t g_menu_selection = 0;
static uint8_t g_current_page = 0;
static uint32_t g_last_monitor_time = 0;
static uint32_t g_last_input_tick = 0;

static Game_direction read_direction(void) {
    if (g_joystick == NULL) { return game_direction_none; }

    const float x = g_joystick->x_value;
    const float y = g_joystick->y_value;
    const float abs_x = x < 0.0f ? -x : x;
    const float abs_y = y < 0.0f ? -y : y;

    if (abs_x < JOYSTICK_DIRECTION_THRESHOLD && abs_y < JOYSTICK_DIRECTION_THRESHOLD) {
        return game_direction_none;
    }
    if (abs_x > abs_y) { return x < 0.0f ? game_direction_left : game_direction_right; }
    return y < 0.0f ? game_direction_down : game_direction_up;
}

static Game_input poll_input(void) {
    Game_input input = {0};
    const uint32_t now = Bsp_Get_Tick_Ms();
    const Button_state button_state = Button_Get_State(g_confirm_button);

    input.direction = read_direction();
    input.direction_pressed = input.direction != game_direction_none && input.direction != g_last_direction;

    if (button_state == button_state_down && g_last_button_state == button_state_up) {
        g_button_down_time = now;
        g_back_sent = 0;
    } else if (button_state == button_state_down && !g_back_sent &&
               now - g_button_down_time >= BACK_HOLD_MS) {
        input.back_requested = 1;
        g_back_sent = 1;
    } else if (button_state == button_state_up && g_last_button_state == button_state_down) {
        if (!g_back_sent) { input.confirm_pressed = 1; }
    }

    g_last_direction = input.direction;
    g_last_button_state = button_state;
    return input;
}

static void draw_pacman_icon(int32_t x, int32_t y) {
    for (int32_t row = -18; row <= 18; row++) {
        int32_t half_width = 0;
        while ((half_width + 1) * (half_width + 1) + row * row <= 18 * 18) { half_width++; }

        const int32_t abs_row = row < 0 ? -row : row;
        int32_t right = half_width;
        if (abs_row <= half_width) { right = abs_row - 1; }
        if (right >= -half_width) {
            Game_Graphics_Fill_Rect(g_lcd, x - half_width, y + row, right + half_width + 1, 1, COLOR_YELLOW);
        }
    }
}

static void draw_snake_icon(int32_t x, int32_t y) {
    static const int8_t cells[][2] = {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {3, 2}, {4, 2}};
    for (uint32_t i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
        Game_Graphics_Fill_Rect(g_lcd, x + cells[i][0] * 9, y + cells[i][1] * 9, 8, 8, COLOR_GREEN);
    }
    Game_Graphics_Fill_Rect(g_lcd, x + 39, y + 19, 2, 2, COLOR_BLACK);
}

static void draw_racing_icon(int32_t x, int32_t y) {
    Game_Graphics_Fill_Rect(g_lcd, x, y, 48, 34, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, x + 22, y, 4, 34, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 8, 18, 24, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, x + 12, y + 12, 10, 8, COLOR_BLUE);
}

static void draw_tank_icon(int32_t x, int32_t y) {
    Game_Graphics_Fill_Rect(g_lcd, x + 2, y + 5, 8, 28, COLOR_GREEN);
    Game_Graphics_Fill_Rect(g_lcd, x + 34, y + 5, 8, 28, COLOR_GREEN);
    Game_Graphics_Fill_Rect(g_lcd, x + 10, y + 9, 24, 20, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, x + 17, y + 12, 10, 10, COLOR_GREEN);
    Game_Graphics_Fill_Rect(g_lcd, x + 21, y, 3, 15, COLOR_GREEN);
}

static void draw_air_icon(int32_t x, int32_t y) {
    Game_Graphics_Fill_Rect(g_lcd, x + 20, y, 7, 35, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, x + 12, y + 10, 23, 15, COLOR_BLUE);
    Game_Graphics_Fill_Rect(g_lcd, x + 4, y + 17, 39, 7, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, x + 16, y + 27, 15, 5, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x + 22, y - 3, 3, 7, COLOR_YELLOW);
}

static void draw_tetris_icon(int32_t x, int32_t y) {
    static const uint16_t tetris_colors[] = {COLOR_CYAN, COLOR_YELLOW, 0x8010u, COLOR_GREEN};
    /* Draw a T-tetromino */
    Game_Graphics_Fill_Rect(g_lcd, x + 10, y + 8, 9, 9, tetris_colors[0]);
    Game_Graphics_Fill_Rect(g_lcd, x + 19, y + 8, 9, 9, tetris_colors[1]);
    Game_Graphics_Fill_Rect(g_lcd, x + 28, y + 8, 9, 9, tetris_colors[2]);
    Game_Graphics_Fill_Rect(g_lcd, x + 19, y + 17, 9, 9, tetris_colors[3]);
    /* Inner highlights */
    Game_Graphics_Fill_Rect(g_lcd, x + 11, y + 9, 4, 4, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x + 20, y + 9, 4, 4, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x + 29, y + 9, 4, 4, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x + 20, y + 18, 4, 4, COLOR_WHITE);
}

static void draw_breakout_icon(int32_t x, int32_t y) {
    /* Paddle */
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 30, 32, 6, COLOR_GREEN);
    /* Ball */
    Game_Graphics_Fill_Rect(g_lcd, x + 26, y + 24, 5, 5, COLOR_WHITE);
    /* Bricks */
    for (int32_t r = 0; r < 3; r++) {
        const uint16_t color = r == 0 ? COLOR_RED : (r == 1 ? COLOR_YELLOW : COLOR_CYAN);
        for (int32_t c = 0; c < 5; c++) {
            Game_Graphics_Fill_Rect(g_lcd, x + 3 + c * 9, y + 2 + r * 6, 7, 4, color);
        }
    }
}

static void draw_pong_icon(int32_t x, int32_t y) {
    /* Left paddle */
    Game_Graphics_Fill_Rect(g_lcd, x + 3, y + 9, 4, 20, COLOR_GREEN);
    /* Right paddle */
    Game_Graphics_Fill_Rect(g_lcd, x + 41, y + 9, 4, 20, COLOR_RED);
    /* Center line */
    for (int32_t i = 0; i < 5; i++) {
        Game_Graphics_Fill_Rect(g_lcd, x + 23, y + 4 + i * 6, 2, 3, COLOR_DARK);
    }
    /* Ball */
    Game_Graphics_Fill_Rect(g_lcd, x + 17, y + 16, 5, 5, COLOR_WHITE);
}

static void draw_gomoku_icon(int32_t x, int32_t y) {
    /* Mini board grid */
    for (int32_t i = 0; i < 4; i++) {
        Game_Graphics_Fill_Rect(g_lcd, x + 6 + i * 12, y + 4, 1, 36, COLOR_DARK);
        Game_Graphics_Fill_Rect(g_lcd, x + 4, y + 6 + i * 12, 36, 1, COLOR_DARK);
    }
    /* Black stone */
    for (int32_t dy = -4; dy <= 4; dy++) {
        for (int32_t dx = -4; dx <= 4; dx++) {
            if (dx * dx + dy * dy > 16) { continue; }
            Game_Graphics_Fill_Rect(g_lcd, x + 18 + dx, y + 16 + dy, 1, 1, COLOR_BLACK);
        }
    }
    /* White stone */
    for (int32_t dy = -4; dy <= 4; dy++) {
        for (int32_t dx = -4; dx <= 4; dx++) {
            if (dx * dx + dy * dy > 16) { continue; }
            Game_Graphics_Fill_Rect(g_lcd, x + 30 + dx, y + 28 + dy, 1, 1, COLOR_WHITE);
        }
    }
}

static void draw_2048_icon(int32_t x, int32_t y) {
    /* 4x4 mini grid */
    for (int32_t r = 0; r < 4; r++) {
        for (int32_t c = 0; c < 4; c++) {
            Game_Graphics_Fill_Rect(g_lcd, x + 4 + c * 10, y + 6 + r * 10, 8, 8, COLOR_DARK);
        }
    }
    /* "2048" tile highlight */
    Game_Graphics_Fill_Rect(g_lcd, x + 24, y + 16, 8, 8, COLOR_YELLOW);
    Game_Graphics_Draw_Text(g_lcd, x + 2, y + 7, "2", 1, COLOR_CYAN);
}

static void draw_dino_icon(int32_t x, int32_t y) {
    /* Ground line */
    Game_Graphics_Fill_Rect(g_lcd, x + 2, y + 32, 44, 3, COLOR_DARK);
    /* Back leg */
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 22, 4, 10, COLOR_GREEN);
    /* Front leg */
    Game_Graphics_Fill_Rect(g_lcd, x + 22, y + 22, 4, 10, COLOR_GREEN);
    /* Body */
    Game_Graphics_Fill_Rect(g_lcd, x + 10, y + 12, 16, 12, COLOR_GREEN);
    /* Head */
    Game_Graphics_Fill_Rect(g_lcd, x + 22, y + 4, 16, 12, COLOR_GREEN);
    /* Eye */
    Game_Graphics_Fill_Rect(g_lcd, x + 32, y + 6, 3, 3, COLOR_WHITE);
    /* Tail */
    Game_Graphics_Fill_Rect(g_lcd, x + 4, y + 16, 8, 4, COLOR_GREEN);
}

static void draw_flappy_icon(int32_t x, int32_t y) {
    /* Ground */
    Game_Graphics_Fill_Rect(g_lcd, x + 2, y + 34, 44, 2, COLOR_DARK);
    /* Bird body */
    Game_Graphics_Fill_Rect(g_lcd, x + 14, y + 14, 10, 8, COLOR_YELLOW);
    /* Beak */
    Game_Graphics_Fill_Rect(g_lcd, x + 24, y + 16, 4, 2, COLOR_RED);
    /* Eye */
    Game_Graphics_Fill_Rect(g_lcd, x + 22, y + 14, 2, 2, COLOR_WHITE);
    /* Wing */
    Game_Graphics_Fill_Rect(g_lcd, x + 12, y + 16, 4, 3, COLOR_DARK);
    /* Top pipe */
    Game_Graphics_Fill_Rect(g_lcd, x + 4, y + 2, 10, 10, COLOR_GREEN);
    Game_Graphics_Fill_Rect(g_lcd, x + 2, y + 9, 14, 3, COLOR_GREEN);
    /* Bottom pipe */
    Game_Graphics_Fill_Rect(g_lcd, x + 34, y + 20, 10, 14, COLOR_GREEN);
    Game_Graphics_Fill_Rect(g_lcd, x + 32, y + 20, 14, 3, COLOR_GREEN);
}

static void draw_maze_icon(int32_t x, int32_t y) {
    /* 迷你迷宫：5x4 网格，外框 + 一条蜿蜒路径 */
    const int32_t grid_left = x;
    const int32_t grid_top = y;
    const int32_t s = 9; /* 格大小 */

    /* 外框 */
    Game_Graphics_Fill_Rect(g_lcd, grid_left, grid_top, s * 5, 1, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, grid_left, grid_top, 1, s * 4, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, grid_left, grid_top + s * 4 - 1, s * 5, 1, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, grid_left + s * 5 - 1, grid_top, 1, s * 4, COLOR_DARK);

    /* 内部迷宫路径 */
    Game_Graphics_Fill_Rect(g_lcd, grid_left + 1, grid_top + s, s * 2, 1, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, grid_left + s, grid_top + s * 3 - 1, s * 3, 1, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, grid_left + s * 3 - 1, grid_top + 1, 1, s, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, grid_left + s * 2 - 1, grid_top + s, 1, s * 2, COLOR_DARK);

    /* 起点（青色小方块） */
    Game_Graphics_Fill_Rect(g_lcd, grid_left + 2, grid_top + 2, 4, 4, COLOR_CYAN);
    /* 终点（红色小方块） */
    Game_Graphics_Fill_Rect(g_lcd, grid_left + s * 4 + 2, grid_top + s * 3 + 2, 4, 4, COLOR_RED);
    /* 宝石（黄色小点） */
    Game_Graphics_Fill_Rect(g_lcd, grid_left + s * 3 + 2, grid_top + s * 2 + 2, 3, 3, COLOR_YELLOW);
}

static void draw_needle_icon(int32_t x, int32_t y) {
    /* 圆盘（三层方形） */
    const int32_t cx = x + 22;
    const int32_t cy = y + 14;
    Game_Graphics_Fill_Rect(g_lcd, cx - 16, cy - 16, 32, 32, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, cx - 10, cy - 10, 20, 20, COLOR_GRAY);
    Game_Graphics_Fill_Rect(g_lcd, cx - 4, cy - 4, 8, 8, COLOR_WHITE);
    /* 针尖 */
    Game_Graphics_Fill_Rect(g_lcd, cx + 12, cy + 12, 4, 4, COLOR_RED);
    Game_Graphics_Fill_Rect(g_lcd, cx - 16, cy - 4, 4, 4, COLOR_YELLOW);
    Game_Graphics_Fill_Rect(g_lcd, cx + 2, cy - 18, 4, 4, COLOR_GREEN);
    /* 下方待发射针 */
    Game_Graphics_Fill_Rect(g_lcd, cx - 1, cy + 22, 2, 10, COLOR_WHITE);
}

static void draw_sfx_lib_icon(int32_t x, int32_t y) {
    /* Note head */
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 20, 12, 4, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x + 6, y + 22, 16, 5, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 27, 12, 4, COLOR_WHITE);
    /* Stem */
    Game_Graphics_Fill_Rect(g_lcd, x + 18, y + 0, 4, 24, COLOR_WHITE);
    /* Flag */
    Game_Graphics_Fill_Rect(g_lcd, x + 22, y + 0, 10, 3, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, x + 22, y + 3, 7, 3, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, x + 22, y + 6, 4, 3, COLOR_CYAN);
    /* Sound lines from the note */
    Game_Graphics_Fill_Rect(g_lcd, x + 2, y + 18, 4, 2, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, x + 0, y + 22, 4, 2, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, x + 2, y + 26, 4, 2, COLOR_CYAN);
}

static void draw_volume_control_icon(int32_t x, int32_t y) {
    /* Speaker body */
    Game_Graphics_Fill_Rect(g_lcd, x + 4, y + 10, 8, 16, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x + 12, y + 6, 4, 24, COLOR_WHITE);
    /* Cone */
    Game_Graphics_Fill_Rect(g_lcd, x + 4, y + 7, 10, 22, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, x + 6, y + 10, 6, 16, COLOR_WHITE);
    /* Sound waves */
    Game_Graphics_Fill_Rect(g_lcd, x + 20, y + 12, 4, 12, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, x + 28, y + 8, 4, 20, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, x + 36, y + 4, 4, 28, COLOR_CYAN);
    /* Wave gaps (black notches) */
    Game_Graphics_Fill_Rect(g_lcd, x + 20, y + 16, 4, 1, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, x + 20, y + 18, 4, 1, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, x + 28, y + 12, 4, 1, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, x + 28, y + 15, 4, 1, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, x + 28, y + 18, 4, 1, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, x + 36, y + 8, 4, 1, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, x + 36, y + 12, 4, 1, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, x + 36, y + 16, 4, 1, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, x + 36, y + 20, 4, 1, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, x + 36, y + 24, 4, 1, COLOR_BLACK);
}

static void draw_info_icon(int32_t x, int32_t y) {
    /* Circle */
    const int32_t cx = x + 22;
    const int32_t cy = y + 18;
    for (int32_t row = -16; row <= 16; row++) {
        int32_t half = 0;
        while ((half + 1) * (half + 1) + row * row <= 16 * 16) { half++; }
        Game_Graphics_Fill_Rect(g_lcd, cx - half, cy + row, half * 2 + 1, 1, COLOR_CYAN);
    }
    /* "i" dot */
    Game_Graphics_Fill_Rect(g_lcd, cx - 2, cy - 7, 4, 4, COLOR_WHITE);
    /* "i" stem */
    Game_Graphics_Fill_Rect(g_lcd, cx - 2, cy + 1, 4, 10, COLOR_WHITE);
}

static void draw_calculator_icon(int32_t x, int32_t y) {
    /* Calculator body */
    Game_Graphics_Fill_Rect(g_lcd, x + 6, y + 4, 36, 30, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 6, 32, 26, COLOR_BLACK);

    /* Display bar */
    Game_Graphics_Fill_Rect(g_lcd, x + 10, y + 8, 28, 6, 0x2104u);

    /* Button grid: 4 cols × 3 rows */
    for (int32_t r = 0; r < 3; r++) {
        for (int32_t c = 0; c < 4; c++) {
            Game_Graphics_Fill_Rect(g_lcd, x + 10 + c * 7, y + 16 + r * 5, 5, 3, 0x3186u);
        }
    }

    /* Highlighted "=" button area */
    Game_Graphics_Fill_Rect(g_lcd, x + 24, y + 26, 5, 3, COLOR_CYAN);
}

static void draw_fps_test_icon(int32_t x, int32_t y) {
    /* Screen outline */
    Game_Graphics_Fill_Rect(g_lcd, x + 6, y + 2, 36, 30, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 4, 32, 26, COLOR_BLACK);

    /* Top bar */
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 4, 32, 5, COLOR_CYAN);
    /* "FPS" text hint in top bar */
    Game_Graphics_Draw_Text(g_lcd, x + 21, y + 4, "F", 1, COLOR_BLACK);

    /* Bottom bar */
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 25, 32, 5, COLOR_DARK);

    /* Color bars in middle — simulate refresh cycling */
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 9, 8, 16, COLOR_RED);
    Game_Graphics_Fill_Rect(g_lcd, x + 16, y + 9, 8, 16, COLOR_GREEN);
    Game_Graphics_Fill_Rect(g_lcd, x + 24, y + 9, 8, 16, COLOR_BLUE);
    Game_Graphics_Fill_Rect(g_lcd, x + 32, y + 9, 8, 16, COLOR_YELLOW);
}

static void draw_dodge_box_icon(int32_t x, int32_t y) {
    /* Arena border — the "box" */
    Game_Graphics_Fill_Rect(g_lcd, x - 14, y - 11, 28, 22, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, x - 15, y - 12, 30, 1, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x - 15, y + 11, 30, 1, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x - 15, y - 12, 1, 24, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x + 14, y - 12, 1, 24, COLOR_WHITE);

    /* Player square in center */
    Game_Graphics_Fill_Rect(g_lcd, x - 2, y - 2, 5, 5, COLOR_WHITE);

    /* Incoming laser warning — horizontal line above player */
    Game_Graphics_Fill_Rect(g_lcd, x - 12, y - 7, 24, 1, COLOR_RED);

    /* Incoming rect attack — small block on the right */
    Game_Graphics_Fill_Rect(g_lcd, x + 7, y + 3, 5, 5, COLOR_RED);
}

static int32_t cell_x(uint8_t col) { return GRID_X0 + (int32_t)col * (CELL_W + CELL_GAP_X); }
static int32_t cell_y(uint8_t row) { return GRID_Y0 + (int32_t)row * (CELL_H + CELL_GAP_Y); }

static void draw_dot(int32_t x, int32_t y, uint8_t filled, uint16_t color) {
    if (filled) {
        Game_Graphics_Fill_Rect(g_lcd, x - 3, y - 3, 7, 7, color);
    } else {
        Game_Graphics_Fill_Rect(g_lcd, x - 3, y - 3, 7, 7, COLOR_DARK);
        Game_Graphics_Fill_Rect(g_lcd, x - 1, y - 1, 3, 3, color);
    }
}

static void draw_grid_cell(uint8_t row, uint8_t col, uint8_t selected, uint8_t game_index) {
    const Game_descriptor* game = Game_Registry_Get(game_index);
    if (game == NULL) { return; }

    const int32_t cx = cell_x(col);
    const int32_t cy = cell_y(row);
    const uint16_t border = selected ? COLOR_CYAN : COLOR_DARK;
    const uint16_t background = selected ? COLOR_BLUE : COLOR_BLACK;

    Game_Graphics_Fill_Rect(g_lcd, cx - 2, cy - 2, CELL_W + 4, CELL_H + 4, border);
    Game_Graphics_Fill_Rect(g_lcd, cx, cy, CELL_W, CELL_H, background);

    /* Icon centered in upper portion of cell */
    const int32_t icon_cx = cx + CELL_W / 2;
    const int32_t icon_cy = cy + 26;
    if (game->icon == game_icon_pacman) {
        draw_pacman_icon(icon_cx, icon_cy);
    } else if (game->icon == game_icon_snake) {
        draw_snake_icon(cx + 11, cy + 8);
    } else if (game->icon == game_icon_racing) {
        draw_racing_icon(cx + 10, cy + 5);
    } else if (game->icon == game_icon_tank) {
        draw_tank_icon(cx + 12, cy + 6);
    } else if (game->icon == game_icon_tetris) {
        draw_tetris_icon(cx + 14, cy + 8);
    } else if (game->icon == game_icon_breakout) {
        draw_breakout_icon(cx + 16, cy + 8);
    } else if (game->icon == game_icon_pong) {
        draw_pong_icon(cx + 12, cy + 7);
    } else if (game->icon == game_icon_gomoku) {
        draw_gomoku_icon(cx + 14, cy + 6);
    } else if (game->icon == game_icon_2048) {
        draw_2048_icon(cx + 16, cy + 8);
    } else if (game->icon == game_icon_dino) {
        draw_dino_icon(cx + 10, cy + 5);
    } else if (game->icon == game_icon_flappy) {
        draw_flappy_icon(cx + 10, cy + 5);
    } else if (game->icon == game_icon_maze) {
        draw_maze_icon(cx + 12, cy + 6);
    } else if (game->icon == game_icon_needle) {
        draw_needle_icon(cx + 10, cy + 2);
    } else if (game->icon == game_icon_info) {
        draw_info_icon(cx + 10, cy + 2);
    } else if (game->icon == game_icon_calculator) {
        draw_calculator_icon(cx + 10, cy + 2);
    } else if (game->icon == game_icon_fps_test) {
        draw_fps_test_icon(cx + 10, cy + 2);
    } else if (game->icon == game_icon_sfx_lib) {
        draw_sfx_lib_icon(cx + 12, cy + 4);
    } else if (game->icon == game_icon_volume_control) {
        draw_volume_control_icon(cx + 12, cy + 4);
    } else if (game->icon == game_icon_dodge_box) {
        draw_dodge_box_icon(icon_cx, icon_cy);
    } else {
        draw_air_icon(cx + 12, cy + 5);
    }

    /* Game name below icon */
    const int32_t name_len = (int32_t)strlen(game->name);
    const int32_t name_x = cx + (CELL_W - name_len * 6) / 2;
    Game_Graphics_Draw_Text(g_lcd, name_x, cy + 54, game->name, 1,
        selected                         ? COLOR_WHITE
        : game->icon == game_icon_snake  ? COLOR_GREEN
        : game->icon == game_icon_pacman ? COLOR_YELLOW
                                         : COLOR_CYAN);

    /* High score */
    Game_Graphics_Draw_Text(g_lcd, cx + 8, cy + 70, "HI", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, cx + 26, cy + 70, Score_Store_Get(game->id), 5, 1, COLOR_WHITE);
}

static void draw_page_indicator(void) {
    const uint8_t game_count = Game_Registry_Count();
    const uint8_t total_pages = (uint8_t)((game_count + MENU_PER_PAGE - 1) / MENU_PER_PAGE);
    const int32_t bar_y = 222;
    const int32_t dot_y = bar_y + 12;

    /* Separator line */
    Game_Graphics_Fill_Rect(g_lcd, 10, bar_y, SCREEN_WIDTH - 20, 1, COLOR_DARK);

    /* Left arrow */
    const uint16_t left_color = g_current_page > 0 ? COLOR_CYAN : COLOR_DARK;
    Game_Graphics_Draw_Text(g_lcd, 20, bar_y + 6, "<", 2, left_color);

    /* Page dots */
    const int32_t dots_x = (int32_t)(SCREEN_WIDTH / 2 - total_pages * 8);
    for (uint8_t p = 0; p < total_pages; p++) {
        draw_dot(dots_x + (int32_t)p * 16, dot_y, p == g_current_page, COLOR_CYAN);
    }

    /* Right arrow */
    const uint16_t right_color = g_current_page < total_pages - 1 ? COLOR_CYAN : COLOR_DARK;
    Game_Graphics_Draw_Text(g_lcd, SCREEN_WIDTH - 36, bar_y + 6, ">", 2, right_color);

    /* Page number */
    char page_text[8];
    snprintf(page_text, sizeof(page_text), "%u/%u", g_current_page + 1, total_pages);
    Game_Graphics_Draw_Text(g_lcd, SCREEN_WIDTH - 60, bar_y + 6, page_text, 1, COLOR_WHITE);

    /* Separator line */
    Game_Graphics_Fill_Rect(g_lcd, 10, bar_y + 22, SCREEN_WIDTH - 20, 1, COLOR_DARK);
}

static void render_menu(void) {
    const uint8_t game_count = Game_Registry_Count();
    const uint8_t page_start = (uint8_t)(g_current_page * MENU_PER_PAGE);
    const uint8_t page_end = page_start + MENU_PER_PAGE;
    const uint8_t page_game_count =
        page_end <= game_count ? MENU_PER_PAGE : (uint8_t)(game_count - page_start);

    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);

    /* Title bar */
    Game_Graphics_Draw_Text(g_lcd, 62, 5, "SELECT GAME", 2, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, 10, 26, SCREEN_WIDTH - 20, 1, COLOR_DARK);

    /* Draw grid cells */
    for (uint8_t slot = 0; slot < page_game_count; slot++) {
        const uint8_t row = slot / MENU_COLS;
        const uint8_t col = slot % MENU_COLS;
        const uint8_t game_index = (uint8_t)(page_start + slot);
        draw_grid_cell(row, col, g_menu_selection == game_index, game_index);
    }

    /* Page indicator with arrows and dots */
    draw_page_indicator();

    /* Bottom hints */
    Game_Graphics_Draw_Text(g_lcd, 14, 258, "< > ^ v", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 60, 258, "to select", 1, COLOR_GRAY);
    Game_Graphics_Draw_Text(g_lcd, 135, 258, "PRESS", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 170, 258, "to pick", 1, COLOR_GRAY);

    Game_Graphics_Draw_Text(g_lcd, 46, 282, "HOLD TO BACK", 1, COLOR_GRAY);
}

static void enter_menu(void) {
    g_console_state = console_state_menu;
    render_menu();
}

static void redraw_cell(uint8_t game_index, uint8_t selected) {
    const uint8_t page_start = (uint8_t)(g_current_page * MENU_PER_PAGE);
    const uint8_t slot = (uint8_t)(game_index - page_start);
    draw_grid_cell(slot / MENU_COLS, slot % MENU_COLS, selected, game_index);
}

static void update_menu(const Game_input* input, const Game_hardware* hardware) {
    const uint8_t game_count = Game_Registry_Count();
    if (game_count == 0) { return; }

    const uint8_t page_start = (uint8_t)(g_current_page * MENU_PER_PAGE);
    const uint8_t page_game_count =
        page_start + MENU_PER_PAGE <= game_count ? MENU_PER_PAGE : (uint8_t)(game_count - page_start);

    if (input->direction_pressed) {
        const uint8_t old_selection = g_menu_selection;
        const uint8_t old_page = g_current_page;
        uint8_t changed = 0;

        /* Compute cursor slot within current page */
        const uint8_t slot = (uint8_t)(g_menu_selection - page_start);
        const uint8_t row = slot / MENU_COLS;
        const uint8_t col = slot % MENU_COLS;

        if (input->direction == game_direction_up) {
            if (row == 0) {
                /* Wrap to last row */
                const uint8_t last_row = (uint8_t)((page_game_count - 1) / MENU_COLS);
                const uint8_t new_slot = last_row * MENU_COLS + col;
                g_menu_selection = new_slot < page_game_count ? (uint8_t)(page_start + new_slot)
                                                              : (uint8_t)(page_start + page_game_count - 1);
            } else {
                g_menu_selection = (uint8_t)(page_start + (row - 1) * MENU_COLS + col);
            }
            changed = 1;
        } else if (input->direction == game_direction_down) {
            const uint8_t new_row = row + 1;
            const uint8_t new_slot = new_row * MENU_COLS + col;
            if (new_slot < page_game_count) {
                g_menu_selection = (uint8_t)(page_start + new_slot);
            } else {
                /* Wrap to top row, same column */
                g_menu_selection = col < page_game_count ? (uint8_t)(page_start + col) : page_start;
            }
            changed = 1;
        } else if (input->direction == game_direction_left) {
            if (col > 0) {
                g_menu_selection = (uint8_t)(g_menu_selection - 1);
            } else {
                /* Flip page left */
                const uint8_t total_pages = (uint8_t)((game_count + MENU_PER_PAGE - 1) / MENU_PER_PAGE);
                if (g_current_page > 0) {
                    g_current_page--;
                } else {
                    g_current_page = (uint8_t)(total_pages - 1);
                }
                /* Position cursor on rightmost column of the same row */
                const uint8_t new_start = (uint8_t)(g_current_page * MENU_PER_PAGE);
                const uint8_t new_count = new_start + MENU_PER_PAGE <= game_count
                                              ? MENU_PER_PAGE
                                              : (uint8_t)(game_count - new_start);
                const uint8_t new_rows = (uint8_t)((new_count + MENU_COLS - 1) / MENU_COLS);
                const uint8_t target_row = row < new_rows ? row : 0;
                const uint8_t target_slot = target_row * MENU_COLS + (MENU_COLS - 1);
                g_menu_selection = target_slot < new_count ? (uint8_t)(new_start + target_slot)
                                                           : (uint8_t)(new_start + new_count - 1);
            }
            changed = 1;
        } else if (input->direction == game_direction_right) {
            const uint8_t next_slot = slot + 1;
            if (col < MENU_COLS - 1 && next_slot < page_game_count) {
                g_menu_selection = (uint8_t)(g_menu_selection + 1);
            } else {
                /* Flip page right */
                const uint8_t total_pages = (uint8_t)((game_count + MENU_PER_PAGE - 1) / MENU_PER_PAGE);
                if (g_current_page < total_pages - 1) {
                    g_current_page++;
                } else {
                    g_current_page = 0;
                }
                /* Position cursor on leftmost column of the same row */
                const uint8_t new_start = (uint8_t)(g_current_page * MENU_PER_PAGE);
                const uint8_t new_count = new_start + MENU_PER_PAGE <= game_count
                                              ? MENU_PER_PAGE
                                              : (uint8_t)(game_count - new_start);
                const uint8_t new_rows = (uint8_t)((new_count + MENU_COLS - 1) / MENU_COLS);
                const uint8_t target_row = row < new_rows ? row : 0;
                g_menu_selection = (uint8_t)(new_start + target_row * MENU_COLS);
            }
            changed = 1;
        }

        if (changed) {
            Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_move);
            if (g_current_page != old_page) {
                /* Page flip — full screen redraw */
                render_menu();
            } else if (g_menu_selection != old_selection) {
                /* Same page — only redraw the two changed cells */
                redraw_cell(old_selection, 0);
                redraw_cell(g_menu_selection, 1);
            }
        }
    }

    if (!input->confirm_pressed) { return; }

    const Game_descriptor* game = Game_Registry_Get(g_menu_selection);
    if (game != NULL) {
        Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_select);
        g_console_state = console_state_game;
        game->init(hardware);
    }
}

static void monitor_resources(void) {
#if GAME_RUNTIME_MONITOR_ENABLE
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (now - g_last_monitor_time < GAME_RUNTIME_MONITOR_INTERVAL_MS) { return; }
    g_last_monitor_time = now;

    printf("[MEM] heap=%u min_heap=%u game_stack=%u words save=%u\n", (unsigned)xPortGetFreeHeapSize(),
        (unsigned)xPortGetMinimumEverFreeHeapSize(), (unsigned)uxTaskGetStackHighWaterMark(NULL),
        (unsigned)Score_Store_Is_Available());
#endif
}

static Game_result update_active_game(const Game_input* input) {
    const Game_descriptor* game = Game_Registry_Get(g_menu_selection);
    if (game == NULL) { return game_result_exit; }

    const Game_result result = game->update(input);
    if (result == game_result_exit) { return result; }
    if (game->is_finished()) {
        g_console_state = console_state_game_over;
        Game_Over_Menu_Open(g_lcd, g_buzzer, game->id, game->name, game->get_score());
    }
    return game_result_running;
}

static void update_game_over(const Game_input* input, const Game_hardware* hardware) {
    const Game_over_action action = Game_Over_Menu_Update(input);
    if (action == game_over_action_menu) {
        enter_menu();
    } else if (action == game_over_action_replay) {
        const Game_descriptor* game = Game_Registry_Get(g_menu_selection);
        if (game == NULL) {
            enter_menu();
            return;
        }
        g_console_state = console_state_game;
        game->init(hardware);
    }
}

static void console_init(void) {
    const Joystick_config joystick_config = {
        .adc_idx = ADC_JOYSTICK_IDX,
        .adc_channel_x = ADC_JOYSTICK_X_CHANNEL,
        .adc_channel_y = ADC_JOYSTICK_Y_CHANNEL,
        .x_min_voltage = JOYSTICK_X_MIN_VOLTAGE,
        .x_max_voltage = JOYSTICK_X_MAX_VOLTAGE,
        .y_min_voltage = JOYSTICK_Y_MIN_VOLTAGE,
        .y_max_voltage = JOYSTICK_Y_MAX_VOLTAGE,
        .x_offset = JOYSTICK_X_OFFSET,
        .y_offset = JOYSTICK_Y_OFFSET,
        .x_dead_zone = JOYSTICK_X_DEAD_ZONE,
        .y_dead_zone = JOYSTICK_Y_DEAD_ZONE,
        .x_reverse = JOYSTICK_X_REVERSE,
        .y_reverse = JOYSTICK_Y_REVERSE,
    };
    g_joystick = Joystick_Create(&joystick_config);
    configASSERT(g_joystick != NULL);
    Joystick_Calibrate_Center(g_joystick, JOYSTICK_CALIBRATION_SAMPLES, JOYSTICK_CALIBRATION_INTERVAL_MS);

    const Button_config button_config = {
        .gpio_idx = GPIO_SW_BTN_IDX,
        .gpio_state_when_pressed = bsp_gpio_state_reset,
    };
    g_confirm_button = Button_Create(&button_config);
    configASSERT(g_confirm_button != NULL);

    const Buzzer_config buzzer_config = {.pwm_idx = PWM_BUZZER_IDX};
    g_buzzer = Buzzer_Create(&buzzer_config);
    configASSERT(g_buzzer != NULL);

    const St7789_config lcd_config = {
        .spi_idx = SOFT_SPI_LCD_IDX,
        .cs_gpio_idx = (uint32_t)-1,
        .dc_gpio_idx = GPIO_TFT_DC_IDX,
        .rst_gpio_idx = GPIO_TFT_RST_IDX,
        .bkl_gpio_idx = GPIO_TFT_BLK_IDX,
        .hor_res = SCREEN_WIDTH,
        .ver_res = SCREEN_HEIGHT,
        .flags = {.mirror_x = LCD_MIRROR_X, .mirror_y = LCD_MIRROR_Y, .color_use_bgr = LCD_COLOR_USE_BGR},
    };
    g_lcd = St7789_Create(&lcd_config);
    configASSERT(g_lcd != NULL);
    St7789_Init(g_lcd);
    St7789_Set_Backlight(g_lcd, 1);

    Score_Store_Init(game_id_count);
    render_menu();
}

static void console_task(void* arg) {
    (void)arg;
    console_init();
    g_last_input_tick = Bsp_Get_Tick_Ms();

    const Game_hardware hardware = {
        .lcd = g_lcd,
        .buzzer = g_buzzer,
    };
    uint32_t tick = xTaskGetTickCount();

    while (1) {
        const Game_input input = poll_input();
        Game_result result = game_result_running;

        /* ── idle tracking ── */
        if (input.direction_pressed || input.confirm_pressed || input.back_requested) {
            g_last_input_tick = Bsp_Get_Tick_Ms();
        }

        /* ── screensaver activation ── */
        if (!Screensaver_Is_Active()) {
            if (g_console_state != console_state_game && Bsp_Get_Tick_Ms() - g_last_input_tick > 30000u) {
                Screensaver_Init(g_lcd);
            }
        }

        /* ── screensaver loop ── */
        if (Screensaver_Is_Active()) {
            if (input.direction_pressed || input.confirm_pressed || input.back_requested) {
                Screensaver_Exit();
                g_last_input_tick = Bsp_Get_Tick_Ms();
                /* redraw the screen that was underneath */
                if (g_console_state == console_state_menu) {
                    render_menu();
                } else if (g_console_state == console_state_game_over) {
                    Game_Over_Menu_Redraw();
                }
            } else {
                Screensaver_Run_Frame();
            }
            vTaskDelayUntil(&tick, pdMS_TO_TICKS(20));
            continue;
        }

        /* ── normal dispatch ── */
        if (g_console_state == console_state_menu) {
            update_menu(&input, &hardware);
        } else if (g_console_state == console_state_game) {
            result = update_active_game(&input);
        } else {
            update_game_over(&input, &hardware);
        }

        if (result == game_result_exit) { enter_menu(); }
        monitor_resources();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(20));
    }
}

void Game_Console_Task_Def(void) {
    const BaseType_t result = xTaskCreate(console_task, "Game", 1024, NULL, 1, NULL);
    configASSERT(result == pdPASS);
}
