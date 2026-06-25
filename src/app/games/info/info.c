#include "info.h"

#include <string.h>

#include "game_graphics.h"
#include "global_config.h"
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
#define COLOR_DARK    0xA514u

#define TOTAL_PAGES   2

static St7789* g_lcd = NULL;
static Vib_motor_gpio* g_vib_motor = NULL;
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
}

/* ── Page 2: GitHub link ── */
static void render_page2(void) {
    /* Intro text */
    Game_Graphics_Draw_Text(g_lcd, 78, 35, "For more", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 60, 55, "information", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 110, 75, "and", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 60, 95, "source code,", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 78, 115, "refer to:", 2, COLOR_WHITE);

    /* URL lines */
    Game_Graphics_Draw_Text(g_lcd, 22, 194, "https://OohShuo.g", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 22, 219, "ithub.io/mspm0g35", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 22, 240, "07_framework", 2, COLOR_WHITE);

    Game_Graphics_Draw_Pal4_Bitmap(g_lcd, 35, 135, INFO_IMAGE_OOOSHUO_50_W, INFO_IMAGE_OOOSHUO_50_H,
        info_image_oooshuo_50_palette, info_image_oooshuo_50_data);
    Game_Graphics_Draw_Pal4_Bitmap(g_lcd, 95, 135, INFO_IMAGE_MORROW_50_W, INFO_IMAGE_MORROW_50_H,
        info_image_morrow_50_palette, info_image_morrow_50_data);
    Game_Graphics_Draw_Pal4_Bitmap(g_lcd, 155, 135, INFO_IMAGE_POLARIS_50_W, INFO_IMAGE_POLARIS_50_H,
        info_image_polaris_50_palette, info_image_polaris_50_data);

    /* Page indicator */
    draw_page_indicator();
}

/* ── Full screen render ── */
static void draw_version_text(void) {
    char buf[32];
    uint8_t p = 0;
    buf[p++] = 'V';
    buf[p++] = 'e';
    buf[p++] = 'r';
    buf[p++] = 's';
    buf[p++] = 'i';
    buf[p++] = 'o';
    buf[p++] = 'n';
    buf[p++] = ':';
    buf[p++] = ' ';
    buf[p++] = '0' + (char)APP_VERSION_MAJOR;
    buf[p++] = '.';
    buf[p++] = '0' + (char)APP_VERSION_MINOR;
    buf[p++] = '.';
    buf[p++] = '0' + (char)APP_VERSION_PATCH;
    buf[p] = '\0';

    Game_Graphics_Draw_Text(g_lcd, 150, 20, buf, 1, 0x053fu);
}

static void render_info_screen(void) {
    Game_Graphics_Clear_Game_Area(g_lcd);

    if (g_current_page == 0) {
        render_page1();
    } else {
        render_page2();
    }
}

void Info_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    g_vib_motor = hardware->vib_motor;
    g_current_page = 0;
    render_info_screen();
    draw_version_text();
}

Game_result Info_Update(const Game_input* input) {
    if (input->back_requested) { return game_result_exit; }

    if (input->direction_pressed) {
        if (input->direction == game_direction_left) {
            g_current_page = (g_current_page + TOTAL_PAGES - 1) % TOTAL_PAGES;
            render_info_screen();
            Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_menu_tick);
        } else if (input->direction == game_direction_right) {
            g_current_page = (g_current_page + 1) % TOTAL_PAGES;
            render_info_screen();
            Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_menu_tick);
        }
    }

    return game_result_running;
}

uint32_t Info_Get_Score(void) { return 0; }

uint8_t Info_Is_Finished(void) { return 0; }
