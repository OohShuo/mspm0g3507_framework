#include "video_player.h"

#include <stdint.h>

#include "FreeRTOS.h"
#include "app_config.h"

#if VIDEO_PLAYER_ENABLE

#include "board_config.h"
#include "button.h"
#include "game_graphics.h"
#include "st7789.h"
#include "storage.h"
#include "task.h"
#include "video_asset.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_RED     0xf800u
#define COLOR_GREEN   0x07e0u
#define COLOR_CYAN    0x07ffu
#define COLOR_YELLOW  0xffe0u

static St7789* g_lcd = NULL;
static Button* g_pause_button = NULL;
static Video_asset g_video;
static uint16_t g_line_buffer[SCREEN_WIDTH];

static void draw_centered(const char* text, int32_t y, uint8_t scale, uint16_t color) {
    int32_t length = 0;
    while (text[length] != '\0') { length++; }
    const int32_t width = length * 6 * scale;
    Game_Graphics_Draw_Text(g_lcd, (SCREEN_WIDTH - width) / 2, y, text, scale, color);
}

static void draw_status(const char* line1, const char* line2, uint16_t color) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_centered("VIDEO PLAYER", 24, 2, COLOR_CYAN);
    draw_centered(line1, 118, 2, color);
    if (line2 != NULL) { draw_centered(line2, 154, 1, COLOR_WHITE); }
}

static void video_player_init_hardware(void) {
    const Button_config pause_button_config = {
        .gpio_idx = GPIO_SW_BTN_IDX,
        .gpio_state_when_pressed = bsp_gpio_state_reset,
    };
    g_pause_button = Button_Create(&pause_button_config);
    configASSERT(g_pause_button != NULL);

    const St7789_config lcd_config = {
        .spi_idx = SOFT_SPI_LCD_IDX,
        .cs_gpio_idx = (uint32_t)-1,
        .dc_gpio_idx = GPIO_TFT_DC_IDX,
        .rst_gpio_idx = GPIO_TFT_RST_IDX,
        .bkl_gpio_idx = GPIO_TFT_BLK_IDX,
        .hor_res = SCREEN_WIDTH,
        .ver_res = SCREEN_HEIGHT,
        .flags =
            {
                .mirror_x = LCD_MIRROR_X,
                .mirror_y = LCD_MIRROR_Y,
                .color_use_bgr = LCD_COLOR_USE_BGR,
            },
    };
    g_lcd = St7789_Create(&lcd_config);
    configASSERT(g_lcd != NULL);
    St7789_Init(g_lcd);
    St7789_Set_Backlight(g_lcd, 1);
}

static uint8_t draw_video_frame(uint32_t frame_index) {
    if (g_video.width > SCREEN_WIDTH || g_video.height > SCREEN_HEIGHT) { return 0; }

    const int32_t x = (SCREEN_WIDTH - (int32_t)g_video.width) / 2;
    const int32_t y = (SCREEN_HEIGHT - (int32_t)g_video.height) / 2;

    St7789_Begin_Write(g_lcd, x, y, x + g_video.width - 1, y + g_video.height - 1);
    for (uint16_t row = 0; row < g_video.height; row++) {
        if (!Video_Asset_Read_Frame_Row(&g_video, frame_index, row, g_line_buffer)) {
            St7789_End_Write(g_lcd);
            return 0;
        }
        St7789_Write_Pixels(g_lcd, (uint8_t*)g_line_buffer, (uint32_t)g_video.width * sizeof(uint16_t));
    }
    St7789_End_Write(g_lcd);
    return 1;
}

static uint8_t open_video(void) {
    if (!Storage_Is_Available()) { return 0; }
    if (!Video_Asset_Open(&g_video, VIDEO_PLAYER_PATH)) { return 0; }
    if (g_video.width <= SCREEN_WIDTH && g_video.height <= SCREEN_HEIGHT) { return 1; }
    Video_Asset_Close(&g_video);
    return 0;
}

static void video_player_task(void* arg) {
    (void)arg;
    video_player_init_hardware();

    uint8_t ready = open_video();
    if (!ready) {
        draw_status("VIDEO MISS", "UPLOAD V565", COLOR_RED);
    } else {
        Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
        draw_status("VIDEO OK", "PRESS PAUSE", COLOR_GREEN);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    uint32_t frame_index = 0;
    uint8_t paused = 0;
    Button_state last_button_state = button_state_up;
    uint32_t tick = xTaskGetTickCount();

    while (1) {
        if (!ready) {
            if (open_video()) {
                ready = 1;
                frame_index = 0;
                Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
                tick = xTaskGetTickCount();
            } else {
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            continue;
        }

        const Button_state button_state = Button_Get_State(g_pause_button);
        if (button_state == button_state_up && last_button_state == button_state_down) {
            paused = !paused;
            if (paused) { draw_status("PAUSED", "PRESS RESUME", COLOR_YELLOW); }
            tick = xTaskGetTickCount();
        }
        last_button_state = button_state;

        if (!paused) {
            if (!draw_video_frame(frame_index)) {
                ready = 0;
                Video_Asset_Close(&g_video);
                draw_status("READ ERROR", "CHECK FILE", COLOR_RED);
                continue;
            }

            frame_index++;
            if (frame_index >= g_video.frame_count) {
#if VIDEO_PLAYER_LOOP
                frame_index = 0;
#else
                paused = 1;
                frame_index = g_video.frame_count - 1u;
                draw_status("FINISHED", "PRESS REPLAY", COLOR_GREEN);
#endif
            }
        }

        uint32_t frame_ms = 1000u / g_video.fps;
        if (frame_ms == 0) { frame_ms = 1; }
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(frame_ms));
    }
}

void Video_Player_Task_Def(void) {
    const BaseType_t result = xTaskCreate(video_player_task, "Video", 768, NULL, 1, NULL);
    configASSERT(result == pdPASS);
}

#else

void Video_Player_Task_Def(void) { }

#endif
