#include "lcd_test.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_time.h"
#include "st7789.h"
#include "task.h"

// === Panel geometry ===

#define LCD_HOR_RES       240
#define LCD_VER_RES       240
#define LCD_LINE_BUF_SIZE (240 * 2)
#define PATTERN_HOLD_MS   2000

// === Module-private state ===

static St7789* g_lcd = NULL;
static Lcd_test_status g_status = {0};
static uint8_t g_line_buf[LCD_LINE_BUF_SIZE];
static uint32_t g_last_pattern_change_ms = 0;

// === Color helpers ===

// RGB565 packs R5G6B5, MSB first on the wire (after the bswap in
// St7789_Send_Color). The values here are the canonical "named" colors.
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED   0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE  0x001F

// === Patterns ===

// Fill one row of the line buffer with `color` and flush it as a
// 1-row window. The repeated call builds up a full-screen fill.
static void flush_row(int32_t y, uint16_t color) {
    uint16_t* lb = (uint16_t*)g_line_buf;
    for (uint32_t i = 0; i < LCD_HOR_RES; i++) { lb[i] = color; }
    St7789_Flush(g_lcd, 0, y, (int32_t)(LCD_HOR_RES - 1), y, g_line_buf, LCD_HOR_RES * 2);
}

// Fill the entire screen with one color, one row at a time.
static void pattern_solid(uint16_t color) {
    for (uint32_t y = 0; y < LCD_VER_RES; y++) { flush_row((int32_t)y, color); }
}

// === Pattern table ===

static void p_solid_red(void) { pattern_solid(COLOR_RED); }
static void p_solid_green(void) { pattern_solid(COLOR_GREEN); }
static void p_solid_blue(void) { pattern_solid(COLOR_BLUE); }
static void p_solid_white(void) { pattern_solid(COLOR_WHITE); }
static void p_solid_black(void) { pattern_solid(COLOR_BLACK); }

typedef void (*Pattern_fn)(void);
typedef struct {
    const char* name;
    Pattern_fn fn;
} Pattern;

static const Pattern g_patterns[] = {
    {"Solid Red", p_solid_red},
    {"Solid Green", p_solid_green},
    {"Solid Blue", p_solid_blue},
    {"Solid White", p_solid_white},
    {"Solid Black", p_solid_black},
};
#define PATTERN_COUNT (sizeof(g_patterns) / sizeof(g_patterns[0]))

static uint32_t flush_cnt = 0;
void flush_done_cb(void* arg) {
    (void)arg;
    flush_cnt++;
}

// === Public API ===

void App_Lcd_Test_Init(void) {
    // Pin map matches the working 地猛星 reference (lcd_init.h):
    //   RST = PA15  -> GPIO_1
    //   DC  = PA16  -> GPIO_2
    //   BLK = PA14  -> GPIO_3
    // CS is hardwired on the panel; bsp_spi doesn't see it.
    // The SDA pin is MOSI-only — no MISO, so we don't try to read back
    // from the panel.
    const St7789_config lcd_cfg = {
        .spi_idx = SPI_LCD_IDX,
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
    St7789_Register_Flush_Done_Cb(g_lcd, flush_done_cb, NULL);

    g_status.init_done = true;
}

void App_Lcd_Test_Loop(void) {
    if (g_lcd == NULL || !g_status.init_done) { return; }

    const uint32_t now = Bsp_Get_Tick_Ms();
    if (g_last_pattern_change_ms == 0 || (now - g_last_pattern_change_ms) >= PATTERN_HOLD_MS) {
        g_last_pattern_change_ms = now;
        const uint8_t idx = g_status.pattern_idx;
        g_patterns[idx].fn();
        g_status.pattern_name = g_patterns[idx].name;
        g_status.pattern_idx = (uint8_t)((idx + 1) % PATTERN_COUNT);
    }
}

const Lcd_test_status* App_Lcd_Test_Get_Status(void) { return &g_status; }
