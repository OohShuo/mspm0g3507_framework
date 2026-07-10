#include <string.h>

#include "bsp_time.h"
#include "game_graphics.h"
#include "game_registry.h"
#include "global_config.h"
#include "info_image_hitsz.h"
#include "info_image_morrow.h"
#include "info_image_oooshuo.h"
#include "info_image_polaris.h"
#include "lfs.h"
#include "storage.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_BLUE    0x053fu
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0xA514u

#define TOTAL_PAGES   2

/* ── Konami code easter egg ── */
static const Game_direction g_konami_sequence[] = {
    game_direction_up,
    game_direction_up,
    game_direction_down,
    game_direction_down,
    game_direction_left,
    game_direction_right,
    game_direction_left,
    game_direction_right,
};
#define KONAMI_LENGTH     8
#define KONAMI_TIMEOUT_MS 500u

static St7789* g_lcd = NULL;
static Vib_motor_gpio* g_vib_motor = NULL;
static uint8_t g_current_page = 0;
static uint8_t g_is_easter_egg = 0;
static uint8_t g_konami_position = 0;
static uint32_t g_konami_last_tick = 0;

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

/* ── Easter egg: nailong image from W25Q32 ── */
#define EE_R565_HEADER_SIZE 16u

static uint16_t ee_rd_u16_le(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

static void render_easter_egg(void) {
    /* Draw status bars first */
    Game_Graphics_Draw_Top_Bar(g_lcd, "INFO");
    Game_Graphics_Clear_Game_Area(g_lcd);
    Game_Graphics_Draw_Bottom_Bar(g_lcd, "B BACK", 0);

    /* Open image directly via LittleFS */
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return; }

    lfs_file_t file;
    Storage_Lock();
    int rc = lfs_file_open(lfs, &file, "/nl.r565", LFS_O_RDONLY);
    Storage_Unlock();
    if (rc < 0) { return; }

    /* Read and validate R565 header */
    uint8_t header[EE_R565_HEADER_SIZE];
    Storage_Lock();
    lfs_ssize_t n = lfs_file_read(lfs, &file, header, sizeof(header));
    Storage_Unlock();
    if (n != (lfs_ssize_t)sizeof(header) || header[0] != 'R' || header[1] != '5' || header[2] != '6' ||
        header[3] != '5' || header[4] != 1) {
        Storage_Lock();
        lfs_file_close(lfs, &file);
        Storage_Unlock();
        return;
    }

    const uint16_t img_w = ee_rd_u16_le(&header[6]);
    const uint16_t img_h = ee_rd_u16_le(&header[8]);
    if (img_w == 0 || img_h == 0 || img_w > SCREEN_WIDTH) {
        Storage_Lock();
        lfs_file_close(lfs, &file);
        Storage_Unlock();
        return;
    }

    /* Center image vertically in the game area */
    const int32_t img_y = GAME_AREA_Y + (GAME_AREA_H - (int32_t)img_h) / 2;
    uint16_t* line_buffer = Game_Graphics_Get_Line_Buffer();
    const uint32_t row_bytes = (uint32_t)img_w * 2u;

    for (uint16_t row = 0; row < img_h; row++) {
        Storage_Lock();
        n = lfs_file_read(lfs, &file, line_buffer, row_bytes);
        Storage_Unlock();
        if (n != (lfs_ssize_t)row_bytes) { break; }

        St7789_Begin_Write(g_lcd, 0, img_y + (int32_t)row, (int32_t)img_w - 1, img_y + (int32_t)row);
        St7789_Write_Pixels(g_lcd, (uint8_t*)line_buffer, row_bytes);
        St7789_End_Write(g_lcd);
    }

    Storage_Lock();
    lfs_file_close(lfs, &file);
    Storage_Unlock();
}

static void info_init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    g_vib_motor = hardware->vib_motor;
    g_current_page = 0;
    g_is_easter_egg = 0;
    g_konami_position = 0;
    render_info_screen();
    draw_version_text();
}

static Game_result info_update(const Game_input* input) {
    /* ── Easter egg state: B returns to normal info ── */
    if (g_is_easter_egg) {
        if (input->back_requested) {
            g_is_easter_egg = 0;
            render_info_screen();
            draw_version_text();
        }
        return game_result_running;
    }

    /* ── Konami code detection: ↑ ↑ ↓ ↓ ← → ← → with 500ms timeout ── */
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (g_konami_position > 0 && now - g_konami_last_tick > KONAMI_TIMEOUT_MS) { g_konami_position = 0; }

    if (input->direction_pressed) {
        if (input->direction == g_konami_sequence[g_konami_position]) {
            g_konami_position++;
            g_konami_last_tick = now;
            if (g_konami_position >= KONAMI_LENGTH) {
                g_konami_position = 0;
                g_is_easter_egg = 1;
                render_easter_egg();
                return game_result_running;
            }
        } else {
            g_konami_position = 0;
        }
    }

    /* ── Normal info navigation (only when Konami sequence is not active) ── */
    if (input->back_requested) { return game_result_exit; }

    if (g_konami_position == 0 && input->direction_pressed) {
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

static uint32_t info_get_score(void) { return 0; }

static void info_draw_icon(St7789* lcd, int32_t x, int32_t y) {
    y += -2;
    /* Circle */
    const int32_t cx = x + 22;
    const int32_t cy = y + 18;
    for (int32_t row = -16; row <= 16; row++) {
        int32_t half = 0;
        while ((half + 1) * (half + 1) + row * row <= 16 * 16) { half++; }
        Game_Graphics_Fill_Rect(lcd, cx - half, cy + row, half * 2 + 1, 1, 0x07ffu);
    }
    /* "i" dot */
    Game_Graphics_Fill_Rect(lcd, cx - 2, cy - 7, 4, 4, 0xffffu);
    /* "i" stem */
    Game_Graphics_Fill_Rect(lcd, cx - 2, cy + 1, 4, 10, 0xffffu);
}

const Game_descriptor game_info_entry = {
    .draw_icon = info_draw_icon,
    .name_color = 0x07ffu,
    .name = "INFO",
    .id = game_id_info,
    .control_hint = NULL,
    .info_text =
        "DESCRIPTION\nView project and team pages.\nBrowse the built-in gallery.\n\nGOAL\nRead "
        "each information page.\n\nCONTROLS\nJOY LEFT OR RIGHT\nB BACK",
    .is_game = 0,
    .init = info_init,
    .update = info_update,
    .get_score = info_get_score,
};
