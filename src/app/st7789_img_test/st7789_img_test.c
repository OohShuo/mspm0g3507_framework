#include "st7789_img_test.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_time.h"
#include "st7789.h"
#include "task.h"

// The asset .c file is LVGL's image-converter output: it wants <lvgl.h> and
// uses LV_ATTRIBUTE_* macros for section/alignment hints. We don't link
// LVGL in this test, so we point the include at our local stub and let
// the asset's static raw RGB565 array drop into our translation unit.
// #define LV_LVGL_H_INCLUDE_SIMPLE
// #include "lvgl.h"
// #include "../../../assets/MizunoAkane220x240.c"

// === Panel geometry (matches the wiring used by lcd_test) ===
#define LCD_HOR_RES     240
#define LCD_VER_RES     240
#define IMG_W           220
#define IMG_H           240
#define IMG_X_OFFSET    ((LCD_HOR_RES - IMG_W) / 2)  // 10 — center the 220-wide image on the 240-wide panel
#define IMG_ROW_BYTES   (IMG_W * 2)  // RGB565, little-endian on the wire after St7789_Send_Color's bswap
#define LINE_BUF_SIZE   IMG_ROW_BYTES
#define REFRESH_HOLD_MS 2000

// === Module-private state ===
static St7789* g_lcd = NULL;
static St7789_img_test_status g_status = {0};
static uint8_t g_line_buf[LINE_BUF_SIZE];
static uint32_t g_last_flush_done_ms = 0;
static uint32_t g_flush_cnt = 0;

static void flush_done_cb(void* arg) {
    (void)arg;
    g_flush_cnt++;
}

// Copy one image row into the scratch buffer (bswap16_inplace inside
// St7789_Send_Color needs a writable buffer — the const image data
// itself is in flash) and send it as a 1-row window.
extern const uint8_t MizunoAkane220x240_map[];
static void flush_image_row(int32_t y) {
    const uint8_t* src = &MizunoAkane220x240_map[y * IMG_ROW_BYTES];
    memcpy(g_line_buf, src, IMG_ROW_BYTES);
    St7789_Flush(g_lcd, IMG_X_OFFSET, y, (int32_t)(IMG_X_OFFSET + IMG_W - 1), y, g_line_buf, IMG_ROW_BYTES);
}

// Walk the image row by row. The full transfer takes ~hundreds of ms
// over soft SPI, but the caller drives the loop on a periodic tick —
// re-entry is safe because St7789_Flush is reentrant on its own bus.
static void flush_full_image(void) {
    for (uint32_t y = 0; y < IMG_H; y++) { flush_image_row((int32_t)y); }
    g_status.flush_count++;
}

// === Public API ===

void App_St7789_Img_Test_Init(void) {
    // Same pin map as lcd_test — the two tasks share the same panel, so
    // only one of them should be enabled at a time. CS is hardwired.
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

    g_status.image_name = "MizunoAkane220x240";
    g_status.init_done = 1;
}

void App_St7789_Img_Test_Loop(void) {
    if (g_lcd == NULL || !g_status.init_done) { return; }

    const uint32_t now = Bsp_Get_Tick_Ms();
    if (g_last_flush_done_ms == 0 || (now - g_last_flush_done_ms) >= REFRESH_HOLD_MS) {
        g_last_flush_done_ms = now;
        flush_full_image();
    }
}

const St7789_img_test_status* App_St7789_Img_Test_Get_Status(void) { return &g_status; }
