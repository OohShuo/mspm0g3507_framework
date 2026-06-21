#include "fps_test.h"

#include <stddef.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH     240
#define SCREEN_HEIGHT    320

#define TOP_BAR_Y        0
#define TOP_BAR_H        GAME_TOP_BAR_H
#define BOTTOM_BAR_Y     GAME_AREA_BOTTOM
#define BOTTOM_BAR_H     GAME_BOTTOM_BAR_H
#define MIDDLE_Y         (TOP_BAR_Y + TOP_BAR_H)
#define MIDDLE_H         (BOTTOM_BAR_Y - MIDDLE_Y)

#define TEST_DURATION_MS 1000u

#define COLOR_BLACK      0x0000u
#define COLOR_WHITE      0xffffu
#define COLOR_RED        0xf800u
#define COLOR_GREEN      0x07e0u
#define COLOR_BLUE       0x001fu
#define COLOR_CYAN       0x07ffu
#define COLOR_YELLOW     0xffe0u
#define COLOR_MAGENTA    0xf81fu
#define COLOR_ORANGE     0xfd20u
#define COLOR_DARK       0xA514u
#define COLOR_GRAY       0x8410u

typedef enum {
    fps_state_idle,
    fps_state_testing,
    fps_state_result,
} Fps_state;

static St7789* g_lcd = NULL;
static Fps_state g_state = fps_state_idle;
static uint32_t g_frame_count = 0;
static uint32_t g_start_time = 0;
static uint32_t g_last_fps = 0;
static uint8_t g_color_index = 0;

static const uint16_t g_palette[] = {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_CYAN,
    COLOR_YELLOW,
    COLOR_MAGENTA,
    COLOR_ORANGE,
    COLOR_WHITE,
};

#define PALETTE_COUNT (sizeof(g_palette) / sizeof(g_palette[0]))

/* ── Draw top status bar ── */
static void draw_top_bar(void) {
    /* "TESTING..."=60px max → x=173 (5px margin) */
    Game_Graphics_Fill_Rect(g_lcd, 173, 4, 65, 10, GAME_BAR_COLOR_BG);
    if (g_state == fps_state_idle) {
        /* "--- FPS"=42px → x=196 */
        Game_Graphics_Draw_Text(g_lcd, 196, 6, "---", 1, COLOR_GRAY);
        Game_Graphics_Draw_Text(g_lcd, 220, 6, "FPS", 1, COLOR_GRAY);
    } else if (g_state == fps_state_testing) {
        Game_Graphics_Draw_Text(g_lcd, 178, 6, "TESTING...", 1, COLOR_YELLOW);
    } else {
        Game_Graphics_Draw_U32(g_lcd, 196, 6, g_last_fps, 3, 1, COLOR_GREEN);
        Game_Graphics_Draw_Text(g_lcd, 220, 6, "FPS", 1, COLOR_GREEN);
    }
}

/* Bottom bar is drawn by the console (hints + FPS) */

/* ── Fill middle area with current color ── */
static void fill_middle(void) {
    uint16_t color;
    if (g_state == fps_state_testing) {
        color = g_palette[g_color_index];
    } else if (g_state == fps_state_result) {
        color = COLOR_BLUE;
    } else {
        color = COLOR_BLACK;
    }
    Game_Graphics_Fill_Rect(g_lcd, 0, MIDDLE_Y, SCREEN_WIDTH, MIDDLE_H, color);
}

/* Full-screen render replaced by per-component drawing in Init/Update */

void Fps_Test_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    g_state = fps_state_idle;
    g_frame_count = 0;
    g_last_fps = 0;
    g_color_index = 0;
    Game_Graphics_Clear_Game_Area(g_lcd);
    draw_top_bar();
}

Game_result Fps_Test_Update(const Game_input* input) {
    if (input->back_requested) { return game_result_exit; }

    const uint32_t now = Game_Runtime_Get_Tick_Ms();

    if (g_state == fps_state_idle) {
        if (input->confirm_pressed) {
            /* Start test */
            g_state = fps_state_testing;
            g_frame_count = 0;
            g_color_index = 0;
            g_start_time = now;
            fill_middle();
            draw_top_bar();
        }
    } else if (g_state == fps_state_testing) {
        g_frame_count++;
        g_color_index = (uint8_t)((g_color_index + 1) % PALETTE_COUNT);
        fill_middle();

        if (now - g_start_time >= TEST_DURATION_MS) {
            /* Test complete */
            g_last_fps = g_frame_count;
            g_state = fps_state_result;
            draw_top_bar();
        }
    } else {
        /* result */
        if (input->confirm_pressed || input->direction_pressed) {
            /* Restart */
            g_state = fps_state_testing;
            g_frame_count = 0;
            g_color_index = 0;
            g_start_time = now;
            fill_middle();
            draw_top_bar();
        }
    }

    return game_result_running;
}

uint32_t Fps_Test_Get_Score(void) { return g_last_fps; }

uint8_t Fps_Test_Is_Finished(void) { return 0; }
