#include <stddef.h>

#include "bsp_time.h"
#include "buzzer.h"
#include "game_graphics.h"
#include "game_registry.h"

#define SCREEN_WIDTH   240
#define SCREEN_HEIGHT  320
#define TOP_BAR_H      GAME_TOP_BAR_H
#define LIST_TOP       (GAME_AREA_Y + 2)
#define ROW_H          11
#define ITEMS_PER_PAGE ((GAME_AREA_BOTTOM - LIST_TOP) / ROW_H)

#define DAS_INITIAL_MS 280u
#define DAS_REPEAT_MS  200u /* 5 Hz */

#define COLOR_BLACK    0x0000u
#define COLOR_WHITE    0xffffu
#define COLOR_CYAN     0x07ffu
#define COLOR_GREEN    0x07e0u
#define COLOR_GRAY     0x8410u
#define COLOR_DARK     0xA514u
#define COLOR_HILITE   0xc618u

/* ── SFX 名称，顺序必须与 Buzzer_sfx_idx 一致 ── */
static const char* const g_sfx_names[] = {"menu move", "menu select", "boot", "pellet", "power", "ghost",
    "pacman waka", "snake eat", "snake turn", "snake grow", "lane change", "overtake", "racing crash",
    "tank fire", "tank hit", "air fire", "air hit", "air pickup", "boss alert", "tetris move",
    "tetris rotate", "tetris lock", "tetris line clear", "tetris tetris", "breakout bounce", "breakout brick",
    "pong paddle", "pong wall", "pong score", "gomoku place", "slide", "merge", "dino jump", "dino duck",
    "flappy flap", "flappy score", "maze move", "maze goal", "needle launch", "needle stick", "explosion",
    "life lost", "victory", "defeat"};

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
    /* "XX SFX"=36px → x=197 (5px margin) */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 197, 4, 41, 10, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_U32(g_hardware.lcd, 202, 5, sfx_count(), 2, 1, COLOR_GRAY);
    Game_Graphics_Draw_Text(g_hardware.lcd, 218, 5, "SFX", 1, COLOR_GRAY);
}

static void draw_page_indicator(void) {
    const uint8_t pages = total_pages();
    const int32_t y = LIST_TOP + (int32_t)ITEMS_PER_PAGE * ROW_H; /* right below last row */

    /* Separator */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 4, y, SCREEN_WIDTH - 8, 1, COLOR_DARK);

    /* Clear below separator */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, y + 1, SCREEN_WIDTH, GAME_AREA_BOTTOM - y - 1, COLOR_BLACK);

    /* Page number only (nav hints handled by console) */
    Game_Graphics_Draw_U32(g_hardware.lcd, 14, y + 3, (uint32_t)(g_page + 1), 1, 1, COLOR_CYAN);
    Game_Graphics_Draw_Text(g_hardware.lcd, 22, y + 3, "/", 1, COLOR_GRAY);
    Game_Graphics_Draw_U32(g_hardware.lcd, 30, y + 3, pages, 1, 1, COLOR_GRAY);
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

    /* Clear list and page-indicator area */
    Game_Graphics_Fill_Rect(
        g_hardware.lcd, 0, LIST_TOP, SCREEN_WIDTH, GAME_AREA_BOTTOM - LIST_TOP, COLOR_BLACK);

    for (uint8_t i = 0; i < items; i++) { draw_row(i, i == g_cursor); }

    draw_page_indicator();
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

static void sfx_lib_init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    g_cursor = 0;
    g_page = 0;
    g_old_cursor = 0;
    g_das_dir = 0;
    g_das_fired = 0;
    Game_Graphics_Clear_Game_Area(g_hardware.lcd);
    draw_top_bar();
    draw_full_page();
}

static Game_result sfx_lib_update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    const uint32_t now = Game_Runtime_Get_Tick_Ms();
    uint8_t full_redraw = 0;
    uint8_t local_dirty = 0;
    uint8_t old_cursor = g_cursor;
    const uint8_t old_page = g_page;

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
        Vib_Motor_Gpio_Play_Effect(g_hardware.vib_motor, vib_effect_menu_select);
    } else if (g_page != old_page || g_cursor != old_cursor) {
        Vib_Motor_Gpio_Play_Effect(g_hardware.vib_motor, vib_effect_menu_tick);
    }

    return game_result_running;
}

static uint32_t sfx_lib_get_score(void) { return 0; }

static void sfx_lib_draw_icon(St7789* lcd, int32_t x, int32_t y) {
    x += 2;
    /* Note head */
    Game_Graphics_Fill_Rect(lcd, x + 8, y + 20, 12, 4, 0xffffu);
    Game_Graphics_Fill_Rect(lcd, x + 6, y + 22, 16, 5, 0xffffu);
    Game_Graphics_Fill_Rect(lcd, x + 8, y + 27, 12, 4, 0xffffu);
    /* Stem */
    Game_Graphics_Fill_Rect(lcd, x + 18, y + 0, 4, 24, 0xffffu);
    /* Flag */
    Game_Graphics_Fill_Rect(lcd, x + 22, y + 0, 10, 3, 0x07ffu);
    Game_Graphics_Fill_Rect(lcd, x + 22, y + 3, 7, 3, 0x07ffu);
    Game_Graphics_Fill_Rect(lcd, x + 22, y + 6, 4, 3, 0x07ffu);
    /* Sound lines from the note */
    Game_Graphics_Fill_Rect(lcd, x + 2, y + 18, 4, 2, 0x07ffu);
    Game_Graphics_Fill_Rect(lcd, x + 0, y + 22, 4, 2, 0x07ffu);
    Game_Graphics_Fill_Rect(lcd, x + 2, y + 26, 4, 2, 0x07ffu);
}

const Game_descriptor game_sfx_lib_entry = {
    .draw_icon = sfx_lib_draw_icon,
    .name_color = 0x07ffu,
    .name = "SFX",
    .id = game_id_sfx_lib,
    .control_hint = NULL,
    .info_text =
        "DESCRIPTION\nBrowse the sound effect library.\nPreview each built-in "
        "sound.\n\nGOAL\nFind and test an effect.\n\nCONTROLS\nJOY SELECT\nA PLAY\nB BACK",
    .is_game = 0,
    .init = sfx_lib_init,
    .update = sfx_lib_update,
    .get_score = sfx_lib_get_score,
};
