#include "lvgl_hello.h"

#include <stddef.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp.h"
#include "bsp_time.h"
#include "st7789.h"
#include "task.h"

#define LCD_HOR_RES     240
#define LCD_VER_RES     320
#define LCD_BUF_LINES   (LCD_VER_RES / 10 / 4)
#define LABEL_TOGGLE_MS 1000U
#define BG_TOGGLE_MS    5000U
#define BG_COLOR_COUNT  (sizeof(BG_COLORS) / sizeof(BG_COLORS[0]))

#if FRAMEWORK_USE_LVGL

// clang-format off
#include "lvgl.h"
// clang-format on

static St7789* g_lcd = NULL;
static lv_display_t* g_disp = NULL;
static lv_obj_t* g_label = NULL;

static uint8_t g_render_buf[LCD_HOR_RES * LCD_BUF_LINES * 2];

static uint32_t g_label_last_toggle_ms = 0;
static uint8_t g_label_show_hello = 1;

static uint32_t g_bg_last_toggle_ms = 0;
static const uint32_t BG_COLORS[] = {0x0000ff, 0xff00ff};  // blue, magenta
static uint32_t g_bg_index = 0;

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

static uint32_t lvgl_get_tick(void) { return Bsp_Get_Tick_Ms(); }

void App_Lvgl_Hello_Init(void) {
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

    St7789_Reset(g_lcd);

    lv_init();
    lv_tick_set_cb(lvgl_get_tick);

    g_disp =
        lv_st7789_create(LCD_HOR_RES, LCD_VER_RES, LV_LCD_FLAG_NONE, lvgl_send_cmd_cb, lvgl_send_color_cb);
    if (g_disp == NULL) { return; }
    lv_display_set_buffers(g_disp, g_render_buf, NULL, sizeof(g_render_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    St7789_Set_Backlight(g_lcd, 1);

    g_label = lv_label_create(lv_screen_active());
    lv_label_set_text(g_label, "Hello");
    lv_obj_align(g_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0xff0000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0000ff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_invalidate(lv_screen_active());
    g_label_last_toggle_ms = Bsp_Get_Tick_Ms();
}

void App_Lvgl_Hello_Loop(void) {
    lv_timer_handler();

    const uint32_t now_ms = Bsp_Get_Tick_Ms();
    if ((now_ms - g_label_last_toggle_ms) >= LABEL_TOGGLE_MS) {
        g_label_last_toggle_ms = now_ms;
        g_label_show_hello = !g_label_show_hello;
        printf("label toggle: %s\n", g_label_show_hello ? "Hello" : "World");
        if (g_label) { lv_label_set_text(g_label, g_label_show_hello ? "Hello" : "World"); }
    }

    if ((now_ms - g_bg_last_toggle_ms) >= BG_TOGGLE_MS) {
        g_bg_last_toggle_ms = now_ms;
        g_bg_index = (g_bg_index + 1) % BG_COLOR_COUNT;
        const uint32_t c = BG_COLORS[g_bg_index];
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(c), LV_PART_MAIN);
        lv_obj_invalidate(lv_screen_active());
        printf("bg change: 0x%06lx\n", (unsigned long)c);
    }
}

#else

void App_Lvgl_Hello_Init(void) {}

void App_Lvgl_Hello_Loop(void) {}

#endif

static void lvgl_hello_task(void* arg) {
    (void)arg;
    App_Lvgl_Hello_Init();
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        App_Lvgl_Hello_Loop();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(20));
    }
}

void Lvgl_Hello_Test_Task_Def(void) { xTaskCreate(lvgl_hello_task, "LVGL_Hello", 1024, NULL, 1, NULL); }
