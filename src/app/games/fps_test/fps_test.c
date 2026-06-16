#include "fps_test.h"

#include <stddef.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH     240
#define SCREEN_HEIGHT    320

#define TOP_BAR_Y        0
#define TOP_BAR_H        24
#define BOTTOM_BAR_Y     290
#define BOTTOM_BAR_H     30
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
#define COLOR_DARK       0x4208u
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
    /* Clear top bar */
    Game_Graphics_Fill_Rect(g_lcd, 0, TOP_BAR_Y, SCREEN_WIDTH, TOP_BAR_H, COLOR_BLACK);

    /* Title */
    Game_Graphics_Draw_Text(g_lcd, 10, 5, "FPS TEST", 2, COLOR_CYAN);

    /* FPS value on right side */
    if (g_state == fps_state_idle) {
        Game_Graphics_Draw_Text(g_lcd, 174, 6, "---", 1, COLOR_GRAY);
        Game_Graphics_Draw_Text(g_lcd, 198, 6, "FPS", 1, COLOR_GRAY);
    } else if (g_state == fps_state_testing) {
        Game_Graphics_Draw_Text(g_lcd, 162, 6, "TESTING...", 1, COLOR_YELLOW);
    } else {
        /* RESULT — show actual FPS */
        Game_Graphics_Draw_U32(g_lcd, 174, 6, g_last_fps, 3, 1, COLOR_GREEN);
        Game_Graphics_Draw_Text(g_lcd, 198, 6, "FPS", 1, COLOR_GREEN);
    }

    /* Separator */
    Game_Graphics_Fill_Rect(g_lcd, 10, TOP_BAR_H - 2, SCREEN_WIDTH - 20, 1, COLOR_DARK);
}

/* ── Draw bottom bar ── */
static void draw_bottom_bar(void) {
    Game_Graphics_Fill_Rect(g_lcd, 10, BOTTOM_BAR_Y, SCREEN_WIDTH - 20, 1, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, 10, BOTTOM_BAR_Y + 1, SCREEN_WIDTH - 20, BOTTOM_BAR_H, COLOR_BLACK);

    if (g_state == fps_state_idle) {
        Game_Graphics_Draw_Text(g_lcd, 120, BOTTOM_BAR_Y + 10, "PRESS TO START", 1, COLOR_WHITE);
        Game_Graphics_Draw_Text(g_lcd, 25, BOTTOM_BAR_Y + 10, "HOLD TO BACK", 1, COLOR_GRAY);
    } else if (g_state == fps_state_testing) {
        Game_Graphics_Draw_Text(g_lcd, 72, BOTTOM_BAR_Y + 10, "TESTING...", 1, COLOR_YELLOW);
    } else {
        Game_Graphics_Draw_Text(g_lcd, 120, BOTTOM_BAR_Y + 10, "PRESS TO RETEST", 1, COLOR_WHITE);
        Game_Graphics_Draw_Text(g_lcd, 25, BOTTOM_BAR_Y + 10, "HOLD TO BACK", 1, COLOR_GRAY);
    }
}

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

/* ── Full screen render ── */
static void render_all(void) {
    fill_middle();
    draw_top_bar();
    draw_bottom_bar();
}

void Fps_Test_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    g_state = fps_state_idle;
    g_frame_count = 0;
    g_last_fps = 0;
    g_color_index = 0;
    render_all();
}

Game_result Fps_Test_Update(const Game_input* input) {
    if (input->back_requested) { return game_result_exit; }

    const uint32_t now = Bsp_Get_Tick_Ms();

    if (g_state == fps_state_idle) {
        if (input->confirm_pressed) {
            /* Start test */
            g_state = fps_state_testing;
            g_frame_count = 0;
            g_color_index = 0;
            g_start_time = now;
            fill_middle();
            draw_top_bar();
            draw_bottom_bar();
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
            draw_bottom_bar();
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
            draw_bottom_bar();
        }
    }

    return game_result_running;
}

uint32_t Fps_Test_Get_Score(void) { return g_last_fps; }

uint8_t Fps_Test_Is_Finished(void) { return 0; }
