#include "bad_apple.h"

#include <stddef.h>

#include "badapple_video.h"
#include "game_graphics.h"
#include "st7789.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define COLOR_BLACK   0x0000u
#define COLOR_CYAN    0x07FFu

static St7789* g_lcd = NULL;

void Bad_Apple_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;

    /* Play from SPI flash via LittleFS */
    if (!Badapple_Video_Init(g_lcd, badapple_video_source_lfs_file)) {
        /* Clear game area only, keep bars */
        Game_Graphics_Draw_Top_Bar(g_lcd, "?");
        Game_Graphics_Draw_Text(g_lcd, 30, 130, "NO BAD APPLE VIDEO", 2, COLOR_CYAN);
        return;
    }

    /* Redraw top bar — Init clears game area */
    Game_Graphics_Draw_Top_Bar(g_lcd, "?");
}

Game_result Bad_Apple_Update(const Game_input* input) {
    if (input->back_requested) {
        Badapple_Video_Stop();
        return game_result_exit;
    }

    if (Badapple_Video_Is_Active()) { Badapple_Video_Update(); }

    return game_result_running;
}

uint32_t Bad_Apple_Get_Score(void) { return 0; }

uint8_t Bad_Apple_Is_Finished(void) { return 0; }
