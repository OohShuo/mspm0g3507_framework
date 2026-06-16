#include "info.h"

#include <string.h>

#include "game_graphics.h"
#include "info_image_hitsz.h"
#include "info_image_morrow.h"
#include "info_image_oooshuo.h"
#include "info_image_polaris.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_BLUE    0x053fu
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0x4208u

#define TOTAL_PAGES   2

static St7789* g_lcd = NULL;
static uint8_t g_current_page = 0;

/* ── Page indicator bar at bottom ── */
static void draw_page_indicator(void) {
    const int32_t bar_y = 268;
    const int32_t text_y = bar_y + 4;

    /* Separator */
    Game_Graphics_Fill_Rect(g_lcd, 10, bar_y, SCREEN_WIDTH - 20, 1, COLOR_DARK);

    /* < */
    const uint16_t left_color = g_current_page > 0 ? COLOR_CYAN : COLOR_DARK;
    Game_Graphics_Draw_Text(g_lcd, 40, text_y, "<", 2, left_color);

    /* Page number */
    char page_text[4];
    page_text[0] = (char)('1' + g_current_page);
    page_text[1] = '/';
    page_text[2] = (char)('1' + TOTAL_PAGES - 1);
    page_text[3] = '\0';
    Game_Graphics_Draw_Text(g_lcd, 106, text_y, page_text, 1, COLOR_WHITE);

    /* > */
    const uint16_t right_color = g_current_page < TOTAL_PAGES - 1 ? COLOR_CYAN : COLOR_DARK;
    Game_Graphics_Draw_Text(g_lcd, 178, text_y, ">", 2, right_color);

    /* Separator */
    Game_Graphics_Fill_Rect(g_lcd, 10, bar_y + 18, SCREEN_WIDTH - 20, 1, COLOR_DARK);
}

/* ── Page 1: development info + HITSZ 100x100 logo ── */
static void render_page1(void) {
    /* Top status bar */
    Game_Graphics_Draw_Text(g_lcd, 10, 5, "INFO", 1, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, 10, 22, SCREEN_WIDTH - 20, 1, COLOR_DARK);

    /* Credits */
    Game_Graphics_Draw_Text(g_lcd, 48, 38, "Designed by:", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 96, 64, "Shuo", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 60, 90, "MorrowHome", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 78, 116, "Polaris", 2, COLOR_WHITE);

    /* 100x100 HITSZ logo — centered */
    Game_Graphics_Draw_Gray4_Bitmap(
        g_lcd, 70, 148, INFO_IMAGE_HITSZ_W, INFO_IMAGE_HITSZ_H, info_image_hitsz_data);

    /* Page indicator */
    draw_page_indicator();

    /* Bottom hint */
    Game_Graphics_Draw_Text(g_lcd, 64, 300, "HOLD TO BACK", 1, COLOR_GRAY);
}

/* ── Page 2: GitHub link ── */
static void render_page2(void) {
    /* Top status bar */
    Game_Graphics_Draw_Text(g_lcd, 10, 5, "INFO", 1, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, 10, 22, SCREEN_WIDTH - 20, 1, COLOR_DARK);

    /* Intro text */
    Game_Graphics_Draw_Text(g_lcd, 78, 35, "For more", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 60, 55, "information", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 110, 75, "and", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 60, 95, "source code,", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 78, 115, "refer to:", 2, COLOR_WHITE);

    /* URL lines */
    Game_Graphics_Draw_Text(g_lcd, 22, 194, "github.com/OohShu", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 22, 219, "o/mspm0g3507_fram", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 22, 240, "ework.git", 2, COLOR_WHITE);

    Game_Graphics_Draw_Gray4_Bitmap(
        g_lcd, 35, 135, INFO_IMAGE_OOOSHUO_50_W, INFO_IMAGE_OOOSHUO_50_H, info_image_oooshuo_50_data);
    Game_Graphics_Draw_Gray4_Bitmap(
        g_lcd, 95, 135, INFO_IMAGE_MORROW_50_W, INFO_IMAGE_MORROW_50_H, info_image_morrow_50_data);
    Game_Graphics_Draw_Gray4_Bitmap(
        g_lcd, 155, 135, INFO_IMAGE_POLARIS_50_W, INFO_IMAGE_POLARIS_50_H, info_image_polaris_50_data);

    /* Page indicator */
    draw_page_indicator();

    /* Bottom hint */
    Game_Graphics_Draw_Text(g_lcd, 64, 300, "HOLD TO BACK", 1, COLOR_GRAY);
}

/* ── Full screen render ── */
static void render_info_screen(void) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);

    if (g_current_page == 0) {
        render_page1();
    } else {
        render_page2();
    }
}

void Info_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    g_current_page = 0;
    render_info_screen();
}

Game_result Info_Update(const Game_input* input) {
    if (input->back_requested) { return game_result_exit; }

    if (input->direction_pressed) {
        if (input->direction == game_direction_left) {
            g_current_page = (g_current_page + TOTAL_PAGES - 1) % TOTAL_PAGES;
            render_info_screen();
        } else if (input->direction == game_direction_right) {
            g_current_page = (g_current_page + 1) % TOTAL_PAGES;
            render_info_screen();
        }
    }

    return game_result_running;
}

uint32_t Info_Get_Score(void) { return 0; }

uint8_t Info_Is_Finished(void) { return 0; }
