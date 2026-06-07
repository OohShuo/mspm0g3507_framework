#include "lcd_test.h"

#include "board_config.h"
#include "bsp_time.h"
#include "st7789.h"

#define LCD_HOR_RES 128
#define LCD_VER_RES 128

static St7789* g_lcd = NULL;
static volatile uint8_t g_flush_done = 0;
// One RGB565 line. St7789_Send_Color does the in-place byte-swap (LE → BE).
static uint16_t g_line_buf[LCD_HOR_RES];

static void on_lcd_flush_done(void* arg) {
    (void)arg;
    g_flush_done = 1;
}

static void lcd_fill_line_pixels(uint16_t color) {
    for (int i = 0; i < LCD_HOR_RES; i++) g_line_buf[i] = color;
}

static void lcd_flush_line(int y, uint16_t color) {
    lcd_fill_line_pixels(color);
    g_flush_done = 0;
    St7789_Flush(g_lcd, 0, y, LCD_HOR_RES - 1, y, (uint8_t*)g_line_buf, LCD_HOR_RES * 2);
    while (!g_flush_done) {
        // wait for bsp_tx_done_cb -> on_lcd_flush_done
    }
}

static void lcd_fill_screen(uint16_t color) {
    for (int y = 0; y < LCD_VER_RES; y++) {
        lcd_flush_line(y, color);
    }
}

void App_Lcd_Test_Init(void) {
    // Pin mapping matches the working 地猛星 reference (lcd_init.h):
    //   RST = PA15  -> GPIO_2
    //   DC  = PA16  -> GPIO_1
    //   BLK = PA14  -> GPIO_3
    // If your board wires the panel differently, swap the indices below.
    const St7789_config lcd_cfg = {
        .spi_idx      = SPI_LCD_IDX,
        .cs_gpio_idx  = (uint32_t)-1,  // CS hardwired on this board
        .dc_gpio_idx  = GPIO_TFT_DC_IDX,
        .rst_gpio_idx = GPIO_TFT_RST_IDX,
        .bkl_gpio_idx = GPIO_TFT_BLK_IDX,
        .hor_res      = LCD_HOR_RES,
        .ver_res      = LCD_VER_RES,
        .flags        = { .mirror_y = 0, .color_use_bgr = 1 },
    };
    g_lcd = St7789_Create(&lcd_cfg);
    St7789_Register_Flush_Done_Cb(g_lcd, on_lcd_flush_done, NULL);
}

void App_Lcd_Test_Loop(void) {
    if (g_lcd == NULL) { return; }

    // Send the init sequence once (idempotent — only does anything the
    // first time after power-on / reset).
    static uint8_t init_done = 0;
    if (!init_done) {
        init_done = 1;
        St7789_Send_Init_Seq(g_lcd);
    }

    // The per-frame flush is currently disabled to avoid hammering the
    // LCD while the W25Q32 task is also exercising bsp_spi. To re-enable,
    // uncomment the block below and add a task in main.c that calls this
    // function.
    //
    // static uint32_t last_change = 0;
    // static uint8_t color_idx = 0;
    // static const uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
    // const uint32_t now = Bsp_Get_Tick_Ms();
    // if (now - last_change < 1000) { return; }
    // last_change = now;
    // lcd_fill_screen(colors[color_idx]);
    // color_idx = (color_idx + 1) % (sizeof(colors) / sizeof(colors[0]));
}
