#include "low_knight.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_config.h"
#include "bsp_gpio.h"
#include "bsp_time.h"
#include "game_graphics.h"
#include "low_knight_resources.h"
#include "low_knight_runtime.h"
#include "storage.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_RED     0xf800u
#define COLOR_GREEN   0x07e0u
#define COLOR_YELLOW  0xffe0u
#define COLOR_CYAN    0x07ffu

#define LOW_KNIGHT_FRAME_MS 33u

static St7789* g_lcd = NULL;
static Low_Knight_Resources g_resources;
static uint8_t g_ready = 0;
static uint8_t g_last_jump_down = 0;
static uint8_t g_last_strike_down = 0;
static uint8_t g_last_up_down = 0;
static uint32_t g_next_frame_time = 0;

static void draw_centered(const char* text, int32_t y, uint8_t scale, uint16_t color) {
    int32_t length = 0;
    while (text[length] != '\0') { length++; }
    const int32_t width = length * 6 * scale;
    Game_Graphics_Draw_Text(g_lcd, (SCREEN_WIDTH - width) / 2, y, text, scale, color);
}

static void draw_boot_screen(uint8_t ready) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_centered("LOW KNIGHT", 18, 2, COLOR_CYAN);

    if (!ready) {
        draw_centered("RESOURCE MISS", 82, 2, COLOR_RED);
        Game_Graphics_Draw_Text(g_lcd, 20, 130, "UPLOAD", 1, COLOR_WHITE);
        Game_Graphics_Draw_Text(g_lcd, 20, 148, "LOW-KNIGHT-P8R", 1, COLOR_YELLOW);
        Game_Graphics_Draw_Text(g_lcd, 20, 174, "TO LITTLEFS", 1, COLOR_WHITE);
        Game_Graphics_Draw_Text(g_lcd, 20, 192, "PATH", 1, COLOR_WHITE);
        Game_Graphics_Draw_Text(g_lcd, 72, 192, "LOW-KNIGHT", 1, COLOR_CYAN);
        return;
    }

    draw_centered("RESOURCE OK", 64, 2, COLOR_GREEN);
    Game_Graphics_Draw_Text(g_lcd, 26, 104, "GFX", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, 88, 104, g_resources.gfx_size, 5, 1, COLOR_YELLOW);
    Game_Graphics_Draw_Text(g_lcd, 26, 124, "GFF", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, 88, 124, g_resources.gff_size, 5, 1, COLOR_YELLOW);
    Game_Graphics_Draw_Text(g_lcd, 26, 144, "MAP", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, 88, 144, g_resources.map_size, 5, 1, COLOR_YELLOW);
    Game_Graphics_Draw_Text(g_lcd, 26, 164, "CRC", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, 88, 164, g_resources.crc16, 5, 1, COLOR_YELLOW);
}

static uint8_t open_resources(void) {
    Low_Knight_Resources_Close(&g_resources);
    g_ready = Low_Knight_Resources_Open(&g_resources, LOW_KNIGHT_RESOURCE_PATH) &&
              Low_Knight_Runtime_Init(&g_resources);
    if (g_ready) {
        Low_Knight_Runtime_Draw(g_lcd);
    } else {
        draw_boot_screen(0);
    }
    g_last_jump_down = 0;
    g_last_strike_down = 0;
    g_last_up_down = 0;
    g_next_frame_time = Bsp_Get_Tick_Ms();
    return g_ready;
}

void Low_Knight_Init(const Game_hardware* hardware) {
    g_lcd = hardware != NULL ? hardware->lcd : NULL;
    memset(&g_resources, 0, sizeof(g_resources));
    g_ready = 0;
    if (g_lcd == NULL) { return; }
    (void)open_resources();
}

Game_result Low_Knight_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested && input->direction == game_direction_down) {
        Low_Knight_Resources_Close(&g_resources);
        g_ready = 0;
        return game_result_exit;
    }

    if (!g_ready) {
        if (Storage_Is_Available()) { (void)open_resources(); }
        return game_result_running;
    }

    const uint32_t now = Bsp_Get_Tick_Ms();
    if ((int32_t)(now - g_next_frame_time) < 0) { return game_result_running; }
    g_next_frame_time = now + LOW_KNIGHT_FRAME_MS;

    const uint8_t jump_down = input->confirm_down;
    const uint8_t strike_down = Bsp_Gpio_Read(GPIO_BNT_RIGHT_IDX) == bsp_gpio_state_reset;
    const uint8_t up_down = input->direction == game_direction_up;
    Low_Knight_Input low_input = {
        .move_x = 0,
        .move_y = 0,
        .jump_down = jump_down,
        .jump_pressed = jump_down && !g_last_jump_down,
        .jump_released = !jump_down && g_last_jump_down,
        .strike_down = strike_down,
        .strike_pressed = strike_down && !g_last_strike_down,
        .up_pressed = up_down && !g_last_up_down,
    };
    if (input->direction == game_direction_left) { low_input.move_x = -1; }
    if (input->direction == game_direction_right) { low_input.move_x = 1; }
    if (input->direction == game_direction_down) { low_input.move_y = -1; }
    if (input->direction == game_direction_up) { low_input.move_y = 1; }

    const Low_Knight_Step_Result step_result = Low_Knight_Runtime_Step(&low_input);
    if (step_result == low_knight_step_transition || step_result == low_knight_step_full) {
        Low_Knight_Runtime_Draw(g_lcd);
    } else if (step_result == low_knight_step_dirty) {
        Low_Knight_Runtime_Draw_Dirty(g_lcd);
    }

    g_last_jump_down = jump_down;
    g_last_strike_down = strike_down;
    g_last_up_down = up_down;
    return game_result_running;
}

uint32_t Low_Knight_Get_Score(void) { return 0; }

uint8_t Low_Knight_Is_Finished(void) { return 0; }
