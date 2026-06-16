#include "info.h"

#include <string.h>

#include "game_graphics.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_BLUE    0x053fu
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0x4208u

static St7789* g_lcd = NULL;

static void render_info_screen(void) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);

    /* ── Top status bar ── */
    Game_Graphics_Draw_Text(g_lcd, 10, 5, "INFO", 1, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, 10, 22, SCREEN_WIDTH - 20, 1, COLOR_DARK);

    /* ── Credits (scale=2 white text) ── */
    Game_Graphics_Draw_Text(g_lcd, 48, 48, "DESIGNED BY:", 2, COLOR_WHITE);

    Game_Graphics_Draw_Text(g_lcd, 96, 78, "SHUO", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 60, 108, "MORROWHOME", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 78, 138, "POLARIS", 2, COLOR_WHITE);

    /* HITSZ — blue, centered, scale=4 */
    Game_Graphics_Draw_Text(g_lcd, 60, 195, "HITSZ", 4, COLOR_BLUE);

    /* ── Bottom hint ── */
    Game_Graphics_Draw_Text(g_lcd, 64, 290, "HOLD TO BACK", 1, COLOR_GRAY);
}

void Info_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    render_info_screen();
}

Game_result Info_Update(const Game_input* input) {
    if (input->back_requested) { return game_result_exit; }
    return game_result_running;
}

uint32_t Info_Get_Score(void) { return 0; }

uint8_t Info_Is_Finished(void) { return 0; }
