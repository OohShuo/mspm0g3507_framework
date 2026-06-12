#include "test_lcd.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_time.h"
#include "st7789.h"
#include "task.h"

#define LCD_HOR_RES       240
#define LCD_VER_RES       320
#define LCD_LINE_BUF_SIZE (240 * 2)
#define PATTERN_HOLD_MS   2000

typedef struct {
    const char* pattern_name;
    uint8_t pattern_idx;
    uint8_t init_done;
} Lcd_test_status;

static St7789* g_lcd = NULL;
static Lcd_test_status g_status = {0};
static uint8_t g_line_buf[LCD_LINE_BUF_SIZE];
static uint32_t g_last_pattern_change_ms = 0;

#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED   0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE  0x001F

static void flush_row(int32_t y, uint16_t color) {
    uint16_t* lb = (uint16_t*)g_line_buf;
    for (uint32_t i = 0; i < LCD_HOR_RES; i++) { lb[i] = color; }
    St7789_Flush(g_lcd, 0, y, (int32_t)(LCD_HOR_RES - 1), y, g_line_buf, LCD_HOR_RES * 2);
}

static void pattern_solid(uint16_t color) {
    for (uint32_t y = 0; y < LCD_VER_RES; y++) { flush_row((int32_t)y, color); }
}

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
static void flush_done_cb(void* arg) {
    (void)arg;
    flush_cnt++;
}

static void lcd_init(void) {
    const St7789_config lcd_cfg = {
        .spi_idx = SOFT_SPI_LCD_IDX,
        .cs_gpio_idx = (uint32_t)-1,
        .dc_gpio_idx = GPIO_TFT_DC_IDX,
        .rst_gpio_idx = GPIO_TFT_RST_IDX,
        .bkl_gpio_idx = GPIO_TFT_BLK_IDX,
        .hor_res = LCD_HOR_RES,
        .ver_res = LCD_VER_RES,
        .flags = {.mirror_y = 1, .color_use_bgr = 1},
    };
    g_lcd = St7789_Create(&lcd_cfg);
    if (g_lcd == NULL) { return; }

    St7789_Init(g_lcd);
    St7789_Set_Backlight(g_lcd, 1);
    St7789_Register_Flush_Done_Cb(g_lcd, flush_done_cb, NULL);

    g_status.init_done = 1;
}

static void lcd_loop(void) {
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

static void lcd_test_task(void* arg) {
    (void)arg;
    lcd_init();
    while (1) {
        lcd_loop();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void Test_Lcd_Task_Def(void) { xTaskCreate(lcd_test_task, "LCD_Test", 256, NULL, 1, NULL); }
