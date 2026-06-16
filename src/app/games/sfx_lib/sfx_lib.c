#include "sfx_lib.h"

#include <stddef.h>

#include "bsp_time.h"
#include "buzzer.h"
#include "game_graphics.h"

#define SCREEN_WIDTH   240
#define SCREEN_HEIGHT  320
#define TOP_BAR_H      24
#define LIST_TOP       (TOP_BAR_H + 2)
#define ROW_H          11
#define VISIBLE_ROWS   24
/* 底栏紧贴列表下方，留 2px 空隙 */
#define BOTTOM_HINT_Y  (LIST_TOP + VISIBLE_ROWS * ROW_H + 2)
#define DAS_INITIAL_MS 280u
#define DAS_REPEAT_MS  45u

#define COLOR_BLACK    0x0000u
#define COLOR_WHITE    0xffffu
#define COLOR_CYAN     0x07ffu
#define COLOR_GREEN    0x07e0u
#define COLOR_GRAY     0x8410u
#define COLOR_DARK     0x4208u
#define COLOR_HILITE   0xc618u /* 浅灰底 */

/* ── SFX 名称，顺序必须与 Buzzer_sfx_idx 一致 ── */
static const char* const g_sfx_names[] = {"menu move", "menu select", "pellet", "power", "ghost",
    "pacman waka", "snake eat", "snake turn", "snake grow", "lane change", "overtake", "racing crash",
    "tank fire", "tank hit", "air fire", "air hit", "air pickup", "boss alert", "tetris move",
    "tetris rotate", "tetris lock", "tetris line clear", "tetris tetris", "breakout bounce", "breakout brick",
    "pong paddle", "pong wall", "pong score", "gomoku place", "slide", "merge", "dino jump", "dino duck",
    "flappy flap", "flappy score", "maze move", "maze goal", "needle launch", "needle stick", "explosion",
    "life lost", "victory", "defeat"};

static Game_hardware g_hardware;
static uint8_t g_cursor;
static uint8_t g_scroll;

/* DAS state */
static uint8_t g_das_dir; /* 0=none, 1=up, 2=down, 3=left, 4=right */
static uint32_t g_das_time;
static uint8_t g_das_fired;

static uint8_t sfx_count(void) { return (uint8_t)buzzer_sfx_count; }

/* ══════════════════════════════════════════════════════════════════════
 *  绘制
 * ══════════════════════════════════════════════════════════════════════ */

static void draw_top_bar(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, TOP_BAR_H, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 6, 5, "SFX LIBRARY", 2, COLOR_CYAN);
    Game_Graphics_Draw_U32(g_hardware.lcd, 152, 5, sfx_count(), 2, 1, COLOR_GRAY);
    Game_Graphics_Draw_Text(g_hardware.lcd, 170, 5, "SFX", 1, COLOR_GRAY);
    Game_Graphics_Fill_Rect(g_hardware.lcd, 4, TOP_BAR_H - 1, SCREEN_WIDTH - 8, 1, COLOR_DARK);
}

/* 底栏：分隔线 + 提示文字，列表刷新后会重绘 */
static void draw_bottom_hint(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 4, BOTTOM_HINT_Y, SCREEN_WIDTH - 8, 1, COLOR_DARK);
    Game_Graphics_Fill_Rect(
        g_hardware.lcd, 0, BOTTOM_HINT_Y + 1, SCREEN_WIDTH, SCREEN_HEIGHT - BOTTOM_HINT_Y - 1, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 14, BOTTOM_HINT_Y + 5, "^ v", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_hardware.lcd, 38, BOTTOM_HINT_Y + 5, "nav", 1, COLOR_GRAY);
    Game_Graphics_Draw_Text(g_hardware.lcd, 70, BOTTOM_HINT_Y + 5, "PRESS", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_hardware.lcd, 106, BOTTOM_HINT_Y + 5, "to play", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_hardware.lcd, 152, BOTTOM_HINT_Y + 5, "HOLD TO BACK", 1, COLOR_GRAY);
}

static int32_t row_y(uint8_t visible_index) { return LIST_TOP + (int32_t)visible_index * ROW_H; }

/* 只清空列表区域，不触碰底栏 */
static void clear_list_area(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, LIST_TOP, SCREEN_WIDTH, BOTTOM_HINT_Y - LIST_TOP, COLOR_BLACK);
}

static void draw_list(void) {
    const uint8_t total = sfx_count();
    clear_list_area();

    for (uint8_t i = 0; i < VISIBLE_ROWS; i++) {
        const uint8_t sfx_index = (uint8_t)(g_scroll + i);
        if (sfx_index >= total) { break; }

        const int32_t y = row_y(i);
        const uint8_t is_selected = (sfx_index == g_cursor);

        if (is_selected) {
            Game_Graphics_Fill_Rect(g_hardware.lcd, 4, y, SCREEN_WIDTH - 8, ROW_H - 1, COLOR_HILITE);
        }

        Game_Graphics_Draw_U32(g_hardware.lcd, 14, y + 2, (uint32_t)(sfx_index + 1), 2, 1,
            is_selected ? COLOR_WHITE : COLOR_GRAY);

        Game_Graphics_Draw_Text(g_hardware.lcd, 38, y + 2, g_sfx_names[sfx_index], 1, COLOR_WHITE);
    }

    /* 每次都重绘底栏，确保不被列表刷掉 */
    draw_bottom_hint();
}

static void scroll_to_cursor(void) {
    if (g_cursor < g_scroll) {
        g_scroll = g_cursor;
    } else if (g_cursor >= g_scroll + VISIBLE_ROWS) {
        g_scroll = (uint8_t)(g_cursor - VISIBLE_ROWS + 1);
    }
}

/* ══════════════════════════════════════════════════════════════════════
 *  列表循环 + DAS 移动
 * ══════════════════════════════════════════════════════════════════════ */

static uint8_t move_cursor(Game_direction dir) {
    const uint8_t total = sfx_count();
    if (total == 0) { return 0; }

    if (dir == game_direction_up) {
        if (g_cursor == 0) {
            g_cursor = (uint8_t)(total - 1); /* 循环到末尾 */
        } else {
            g_cursor--;
        }
        return 1;
    }
    if (dir == game_direction_down) {
        if (g_cursor >= total - 1) {
            g_cursor = 0; /* 循环到开头 */
        } else {
            g_cursor++;
        }
        return 1;
    }
    if (dir == game_direction_left) {
        /* 向左跳 5 行，支持循环 */
        if (g_cursor < 5) {
            g_cursor = (uint8_t)(total - (5 - g_cursor));
            if (g_cursor >= total) { g_cursor = (uint8_t)(total - 1); }
        } else {
            g_cursor = (uint8_t)(g_cursor - 5);
        }
        return 1;
    }
    if (dir == game_direction_right) {
        if (g_cursor + 5 >= total) {
            g_cursor = (uint8_t)((g_cursor + 5) - total);
        } else {
            g_cursor = (uint8_t)(g_cursor + 5);
        }
        return 1;
    }
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 *  API
 * ══════════════════════════════════════════════════════════════════════ */

void Sfx_Lib_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    g_cursor = 0;
    g_scroll = 0;
    g_das_dir = 0;
    g_das_fired = 0;
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_top_bar();
    draw_list();
}

Game_result Sfx_Lib_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    const uint32_t now = Bsp_Get_Tick_Ms();
    uint8_t dirty = 0;

    /* ── 方向键 + DAS 长按 ── */
    if (input->direction == game_direction_up || input->direction == game_direction_down ||
        input->direction == game_direction_left || input->direction == game_direction_right) {
        const uint8_t cur_dir = (uint8_t)(input->direction + 1); /* 1=up,2=down,3=left,4=right */

        if (input->direction_pressed || cur_dir != g_das_dir) {
            /* 首次按下或方向改变：立即移动一步 */
            g_das_dir = cur_dir;
            g_das_fired = 0;
            g_das_time = now;
            dirty = move_cursor(input->direction);
        } else {
            /* 同一方向保持：DAS 重复 */
            const uint32_t threshold = g_das_fired ? DAS_REPEAT_MS : DAS_INITIAL_MS;
            while (now - g_das_time >= threshold) {
                g_das_time += threshold;
                g_das_fired = 1;
                dirty |= move_cursor(input->direction);
            }
        }
    } else {
        g_das_dir = 0;
        g_das_fired = 0;
    }

    if (dirty) {
        scroll_to_cursor();
        draw_list();
    }

    if (input->confirm_pressed) { Buzzer_Play_Sfx(g_hardware.buzzer, (Buzzer_sfx_idx)g_cursor); }

    return game_result_running;
}

uint32_t Sfx_Lib_Get_Score(void) { return 0; }

uint8_t Sfx_Lib_Is_Finished(void) { return 0; }
