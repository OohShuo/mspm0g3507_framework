#include "st7789_img_test.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_time.h"
#include "st7789.h"
#include "task.h"

#define LCD_HOR_RES     240
#define LCD_VER_RES     320
#define IMG_W           100
#define IMG_H           100
#define IMG_X_OFFSET    ((LCD_HOR_RES - IMG_W) / 2)
#define IMG_Y_OFFSET    ((LCD_VER_RES - IMG_H) / 2)
#define IMG_ROW_BYTES   (IMG_W * 2)
#define LINE_BUF_SIZE   IMG_ROW_BYTES
#define REFRESH_HOLD_MS 2000

typedef struct {
    const char* image_name;
    uint32_t flush_count;
    uint8_t init_done;
} St7789_img_test_status;

static St7789* g_lcd = NULL;
static uint8_t g_line_buf[LINE_BUF_SIZE];
static uint32_t g_last_flush_done_ms = 0;
static uint32_t g_flush_cnt = 0;

static void flush_done_cb(void* arg) {
    (void)arg;
    g_flush_cnt++;
}

extern const uint8_t MizunoAkane100x100_map[];  // NOLINT (readability-identifier-naming)
static void flush_image_row(int32_t y) {
    const uint8_t* src = &MizunoAkane100x100_map[(y - IMG_Y_OFFSET) * IMG_ROW_BYTES];
    memcpy(g_line_buf, src, IMG_ROW_BYTES);
    St7789_Flush(g_lcd, IMG_X_OFFSET, y, (int32_t)(IMG_X_OFFSET + IMG_W - 1), y, g_line_buf, IMG_ROW_BYTES);
}

static void flush_full_image(void) {
    for (uint32_t y = IMG_Y_OFFSET; y < IMG_Y_OFFSET + IMG_H; y++) { flush_image_row((int32_t)y); }
}

void App_St7789_Img_Test_Init(void) {
    const St7789_config lcd_cfg = {
        .spi_idx = SOFT_SPI_LCD_IDX,
        .cs_gpio_idx = (uint32_t)-1,
        .dc_gpio_idx = GPIO_TFT_DC_IDX,
        .rst_gpio_idx = GPIO_TFT_RST_IDX,
        .bkl_gpio_idx = GPIO_TFT_BLK_IDX,
        .hor_res = LCD_HOR_RES,
        .ver_res = LCD_VER_RES,
        .flags = {.mirror_y = 0, .color_use_bgr = 0},
    };
    g_lcd = St7789_Create(&lcd_cfg);
    if (g_lcd == NULL) { return; }

    St7789_Init(g_lcd);
    St7789_Set_Backlight(g_lcd, 1);
    St7789_Run_Init_Sequence(g_lcd);
    St7789_Register_Flush_Done_Cb(g_lcd, flush_done_cb, NULL);
}

void App_St7789_Img_Test_Loop(void) {
    if (g_lcd == NULL) { return; }

    const uint32_t now = Bsp_Get_Tick_Ms();
    if (g_last_flush_done_ms == 0 || (now - g_last_flush_done_ms) >= REFRESH_HOLD_MS) {
        g_last_flush_done_ms = now;
        flush_full_image();
    }
}

static void st7789_img_test_task(void* arg) {
    (void)arg;
    App_St7789_Img_Test_Init();
    while (1) {
        App_St7789_Img_Test_Loop();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void St7789_Img_Test_Task_Def(void) { xTaskCreate(st7789_img_test_task, "ST7789_Img", 256, NULL, 1, NULL); }
