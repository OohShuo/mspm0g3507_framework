#include "lvgl_hello.h"

#include <stdbool.h>
#include <stddef.h>

#include "SEGGER_RTT.h"
#include "board_config.h"
#include "bsp.h"
#include "bsp_time.h"
#include "lvgl.h"
#include "src/drivers/display/st7789/lv_st7789.h"
#include "st7789.h"

// === Panel geometry — matches the st7789 test ===

#define LCD_HOR_RES   240
#define LCD_VER_RES   240
#define LCD_BUF_LINES (LCD_VER_RES / 10)  // 1/10 screen sized buffer (LVGL docs recommendation)

// === Module state ===

static St7789* g_lcd = NULL;
static lv_display_t* g_disp = NULL;
static lv_obj_t* g_label = NULL;

// LVGL render buffer: 1/10 screen * 2 bytes/pixel (RGB565). Placed in
// static RAM so the address stays valid forever.
static uint8_t g_render_buf[LCD_HOR_RES * LCD_BUF_LINES * 2];

// "Hello" / "World" toggle cadence.
#define LABEL_TOGGLE_MS 5000U
static uint32_t g_label_last_toggle_ms = 0;
static bool g_label_show_hello = true;

// === LVGL ↔ st7789 bridge ===
//
// lv_st7789_create hands us two callbacks that follow the
// lv_lcd_generic_mipi contract:
//
//   send_cmd_cb  — DC=0, command byte, DC=1, parameter bytes. The driver
//                  issues this for every register write (init sequence,
//                  CASET, RASET, MADCTL, COLMOD, INVON, DISPON, ...).
//
//   send_color_cb — DC=0, RAMWR (0x2C), DC=1, pixel payload. The driver
//                  calls this after CASET + RASET to push one chunk of
//                  RGB565 pixels; we must call lv_display_flush_ready()
//                  once the panel has accepted the data.
//
// St7789_Send_Cmd and St7789_Send_Color block on software SPI, so
// returning from them means the bus is idle and it is safe to hand the
// px_map back to LVGL immediately.

static void lvgl_send_cmd_cb(
    lv_display_t* disp, const uint8_t* cmd, size_t cmd_size, const uint8_t* param, size_t param_size) {
    (void)disp;
    St7789_Send_Cmd(g_lcd, cmd, (uint32_t)cmd_size, param, (uint32_t)param_size);
}

static void lvgl_send_color_cb(
    lv_display_t* disp, const uint8_t* cmd, size_t cmd_size, uint8_t* px_map, size_t px_size) {
    (void)disp;
    St7789_Send_Color(g_lcd, cmd, (uint32_t)cmd_size, px_map, (uint32_t)px_size);
    lv_display_flush_ready(disp);
}

// === LVGL tick ===

// LVGL calls this whenever it needs the elapsed-ms timestamp. We just
// forward to the BSP tick counter (incremented from SysTick).
static uint32_t lvgl_get_tick(void) { return Bsp_Get_Tick_Ms(); }

// === Init / Loop ===

void App_Lvgl_Hello_Init(void) {
    // Panel config — matches lcd_test (1.3" 地猛星 reference panel).
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

    // 1. Hard-reset the panel so lv_st7789's init_cmd_list starts from a
    //    clean state. St7789_Reset is the RST pulse only — no register
    //    writes, no gamma setup.
    St7789_Reset(g_lcd);

    // 2. LVGL core.
    lv_init();
    lv_tick_set_cb(lvgl_get_tick);

    // 3. Display: lv_st7789_create owns the lv_display_t, wires up the
    //    standard MIPI flush logic (CASET → RASET → RAMWR per dirty
    //    area), and runs its built-in LovyanGFX init_cmd_list
    //    (GCTRL/VCOMS/VRHS/PVGAMCTRL/NVGAMCTRL/...).
    g_disp =
        lv_st7789_create(LCD_HOR_RES, LCD_VER_RES, LV_LCD_FLAG_NONE, lvgl_send_cmd_cb, lvgl_send_color_cb);
    if (g_disp == NULL) { return; }
    lv_display_set_buffers(g_disp, g_render_buf, NULL, sizeof(g_render_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 4. Re-apply the 地猛星 1.3" reference init sequence on top of
    //    lv_st7789's defaults. Without this, the panel's gamma / porch
    //    / INVON registers end up at LovyanGFX stock values and the
    //    screen stays black on this particular panel. The sequence is
    //    idempotent for the registers lv_st7789 also writes (GCTRL /
    //    VCOMS / VRHS / PWCTRL1 / PVGAMCTRL / NVGAMCTRL) — we just win
    //    the last write.
    St7789_Run_Init_Sequence(g_lcd);

    // 5. Backlight on last so the panel does not flash the bootloader
    //    logo on a blanked-out framebuffer.
    St7789_Set_Backlight(g_lcd, 1);

    // 6. Widget: a centered label. The loop toggles its text between
    //    "Hello" and "World" every LABEL_TOGGLE_MS.
    g_label = lv_label_create(lv_screen_active());
    lv_label_set_text(g_label, "Hello");
    lv_obj_align(g_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0xf10000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x00001f), LV_PART_MAIN);
    g_label_last_toggle_ms = Bsp_Get_Tick_Ms();
}

void App_Lvgl_Hello_Loop(void) {
    SEGGER_RTT_printf(0, "loop in\n");
    lv_timer_handler();
    SEGGER_RTT_printf(0, "loop after lv_timer_handler\n");

    const uint32_t now_ms = Bsp_Get_Tick_Ms();
    if ((now_ms - g_label_last_toggle_ms) >= LABEL_TOGGLE_MS) {
        g_label_last_toggle_ms = now_ms;
        g_label_show_hello = !g_label_show_hello;
        SEGGER_RTT_printf(0, "label toggle: %s\n", g_label_show_hello ? "Hello" : "World");
        if (g_label) { lv_label_set_text(g_label, g_label_show_hello ? "Hello" : "World"); }
    }
}
