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
#define ITEMS_PER_PAGE 24
#define BOTTOM_HINT_Y  (LIST_TOP + ITEMS_PER_PAGE * ROW_H + 2)

#define DAS_INITIAL_MS 280u
#define DAS_REPEAT_MS  200u /* 5 Hz */

#define COLOR_BLACK    0x0000u
#define COLOR_WHITE    0xffffu
#define COLOR_CYAN     0x07ffu
#define COLOR_GREEN    0x07e0u
#define COLOR_GRAY     0x8410u
#define COLOR_DARK     0x4208u
#define COLOR_HILITE   0xc618u

/* ── SFX 名称，顺序必须与 Buzzer_sfx_idx 一致 ── */
static const char* const g_sfx_names[] = {
    "menu move",
    "menu select",
    "pellet",
    "power",
    "ghost",
    "pacman waka",
    "snake eat",
    "snake turn",
    "snake grow",
    "lane change",
    "overtake",
    "racing crash",
    "tank fire",
    "tank hit",
    "air fire",
    "air hit",
    "air pickup",
    "boss alert",
    "tetris move",
    "tetris rotate",
    "tetris lock",
    "tetris line clear",
    "tetris tetris",
    "breakout bounce",
    "breakout brick",
    "pong paddle",
    "pong wall",
    "pong score",
    "gomoku place",
    "slide",
    "merge",
    "dino jump",
    "dino duck",
    "flappy flap",
    "flappy score",
    "maze move",
    "maze goal",
    "needle launch",
    "needle stick",
    "explosion",
    "life lost",
    "victory",
    "defeat",
};

static Game_hardware g_hardware;
static uint8_t g_cursor; /* 当前页内选中位置 (0 起始) */
static uint8_t g_page;   /* 当前页码 */
static uint8_t g_old_cursor;

/* DAS state */
static uint8_t g_das_dir;
static uint32_t g_das_time;
static uint8_t g_das_fired;

static uint8_t sfx_count(void) { return (uint8_t)buzzer_sfx_count; }

static uint8_t total_pages(void) {
    const uint8_t total = sfx_count();
    return (uint8_t)((total + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
}

/* 当前页的 SFX 起始索引 */
static uint8_t page_start(void) { return (uint8_t)(g_page * ITEMS_PER_PAGE); }

/* 当前页实际条目数 */
static uint8_t page_items(void) {
    const uint8_t total = sfx_count();
    const uint8_t start = page_start();
    return start + ITEMS_PER_PAGE <= total ? ITEMS_PER_PAGE : (uint8_t)(total - start);
}

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

static void draw_bottom_hint(void) {
    const uint8_t pages = total_pages();

    Game_Graphics_Fill_Rect(g_hardware.lcd, 4, BOTTOM_HINT_Y, SCREEN_WIDTH - 8, 1, COLOR_DARK);
    Game_Graphics_Fill_Rect(
        g_hardware.lcd, 0, BOTTOM_HINT_Y + 1, SCREEN_WIDTH, SCREEN_HEIGHT - BOTTOM_HINT_Y - 1, COLOR_BLACK);

    /* 页码 */
    Game_Graphics_Draw_U32(g_hardware.lcd, 14, BOTTOM_HINT_Y + 5, (uint32_t)(g_page + 1), 1, 1, COLOR_CYAN);
    Game_Graphics_Draw_Text(g_hardware.lcd, 22, BOTTOM_HINT_Y + 5, "/", 1, COLOR_GRAY);
    Game_Graphics_Draw_U32(g_hardware.lcd, 30, BOTTOM_HINT_Y + 5, pages, 1, 1, COLOR_GRAY);

    Game_Graphics_Draw_Text(g_hardware.lcd, 58, BOTTOM_HINT_Y + 5, "^ v", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_hardware.lcd, 82, BOTTOM_HINT_Y + 5, "nav", 1, COLOR_GRAY);
    Game_Graphics_Draw_Text(g_hardware.lcd, 110, BOTTOM_HINT_Y + 5, "PRESS", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_hardware.lcd, 146, BOTTOM_HINT_Y + 5, "play", 1, COLOR_GRAY);
    Game_Graphics_Draw_Text(g_hardware.lcd, 176, BOTTOM_HINT_Y + 5, "< >", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_hardware.lcd, 200, BOTTOM_HINT_Y + 5, "page", 1, COLOR_GRAY);
}

static int32_t row_y(uint8_t pos) { return LIST_TOP + (int32_t)pos * ROW_H; }

/* ── 单行绘制 ── */
static void draw_row(uint8_t pos, uint8_t selected) {
    const uint8_t sfx_index = (uint8_t)(page_start() + pos);
    const uint8_t total = sfx_count();
    if (sfx_index >= total) { return; }

    const int32_t y = row_y(pos);

    if (selected) {
        Game_Graphics_Fill_Rect(g_hardware.lcd, 4, y, SCREEN_WIDTH - 8, ROW_H - 1, COLOR_HILITE);
    } else {
        Game_Graphics_Fill_Rect(g_hardware.lcd, 4, y, SCREEN_WIDTH - 8, ROW_H - 1, COLOR_BLACK);
    }

    Game_Graphics_Draw_U32(
        g_hardware.lcd, 14, y + 2, (uint32_t)(sfx_index + 1), 2, 1, selected ? COLOR_WHITE : COLOR_GRAY);

    Game_Graphics_Draw_Text(g_hardware.lcd, 38, y + 2, g_sfx_names[sfx_index], 1, COLOR_WHITE);
}

/* ── 局部刷新：只重绘旧/新两行 ── */
static void delta_redraw(uint8_t old_pos, uint8_t new_pos) {
    draw_row(old_pos, 0);
    draw_row(new_pos, 1);
}

/* ── 全页刷新 ── */
static void draw_full_page(void) {
    const uint8_t items = page_items();

    /* 清空列表区域 */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, LIST_TOP, SCREEN_WIDTH, BOTTOM_HINT_Y - LIST_TOP, COLOR_BLACK);

    for (uint8_t i = 0; i < items; i++) { draw_row(i, i == g_cursor); }

    draw_bottom_hint();
}

/* ══════════════════════════════════════════════════════════════════════
 *  翻页 + 循环
 * ══════════════════════════════════════════════════════════════════════ */

/* 返回 1 表示翻页（需要全刷），0 表示页内移动（局部刷新） */
static uint8_t move_cursor(Game_direction dir) {
    const uint8_t items = page_items();
    const uint8_t pages = total_pages();
    if (items == 0) { return 0; }

    if (dir == game_direction_up) {
        if (g_cursor > 0) {
            g_cursor--;
            return 0;
        }
        /* 翻到上一页，光标放末尾 */
        if (pages <= 1) { return 0; }
        g_page = g_page > 0 ? (uint8_t)(g_page - 1) : (uint8_t)(pages - 1);
        g_cursor = (uint8_t)(page_items() - 1);
        return 1;
    }
    if (dir == game_direction_down) {
        if (g_cursor + 1 < items) {
            g_cursor++;
            return 0;
        }
        /* 翻到下一页 */
        if (pages <= 1) { return 0; }
        g_page = g_page + 1 < pages ? (uint8_t)(g_page + 1) : 0;
        g_cursor = 0;
        return 1;
    }
    if (dir == game_direction_left) {
        /* 上一页，光标保持在相同位置 */
        if (pages <= 1) { return 0; }
        g_page = g_page > 0 ? (uint8_t)(g_page - 1) : (uint8_t)(pages - 1);
        if (g_cursor >= page_items()) { g_cursor = (uint8_t)(page_items() - 1); }
        return 1;
    }
    if (dir == game_direction_right) {
        /* 下一页 */
        if (pages <= 1) { return 0; }
        g_page = g_page + 1 < pages ? (uint8_t)(g_page + 1) : 0;
        if (g_cursor >= page_items()) { g_cursor = (uint8_t)(page_items() - 1); }
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
    g_page = 0;
    g_old_cursor = 0;
    g_das_dir = 0;
    g_das_fired = 0;
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_top_bar();
    draw_full_page();
}

Game_result Sfx_Lib_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    const uint32_t now = Bsp_Get_Tick_Ms();
    uint8_t full_redraw = 0;
    uint8_t local_dirty = 0;
    uint8_t old_cursor = g_cursor;

    /* ── 方向键 + DAS ── */
    if (input->direction == game_direction_up || input->direction == game_direction_down ||
        input->direction == game_direction_left || input->direction == game_direction_right) {
        const uint8_t cur_dir = (uint8_t)(input->direction + 1);

        if (input->direction_pressed || cur_dir != g_das_dir) {
            g_das_dir = cur_dir;
            g_das_fired = 0;
            g_das_time = now;
            if (move_cursor(input->direction)) {
                full_redraw = 1;
            } else {
                local_dirty = 1;
            }
        } else {
            const uint32_t threshold = g_das_fired ? DAS_REPEAT_MS : DAS_INITIAL_MS;
            while (now - g_das_time >= threshold) {
                g_das_time += threshold;
                g_das_fired = 1;
                if (move_cursor(input->direction)) {
                    full_redraw = 1;
                    break; /* 翻页后退出 while，等下一帧继续 */
                }
                local_dirty = 1;
            }
        }
    } else {
        g_das_dir = 0;
        g_das_fired = 0;
    }

    if (full_redraw) {
        draw_full_page();
        g_old_cursor = g_cursor;
    } else if (local_dirty && g_cursor != old_cursor) {
        delta_redraw(old_cursor, g_cursor);
        g_old_cursor = g_cursor;
    }

    if (input->confirm_pressed) {
        Buzzer_Play_Sfx(g_hardware.buzzer, (Buzzer_sfx_idx)(page_start() + g_cursor));
    }

    return game_result_running;
}

uint32_t Sfx_Lib_Get_Score(void) { return 0; }

uint8_t Sfx_Lib_Is_Finished(void) { return 0; }
