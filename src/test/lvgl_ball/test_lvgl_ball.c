#include "test_lvgl_ball.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp.h"
#include "bsp_time.h"
#include "st7789.h"
#include "task.h"

#define LCD_HOR_RES   240
#define LCD_VER_RES   320
#define LCD_BUF_LINES (LCD_VER_RES / 10 / 4)

#define BORDER_INSET  5
#define BORDER_WIDTH  2

#define BALL_SIZE     25

#define FIELD_X_MIN   BORDER_INSET
#define FIELD_Y_MIN   BORDER_INSET
#define FIELD_X_MAX   (LCD_HOR_RES - BORDER_INSET - BALL_SIZE)
#define FIELD_Y_MAX   (LCD_VER_RES - BORDER_INSET - BALL_SIZE)

#define BALL_DX       3
#define BALL_DY       5

#if FRAMEWORK_USE_LVGL

// clang-format off

#include "lvgl.h"
#include "src/drivers/display/st7789/lv_st7789.h"

// clang-format on

static St7789* g_lcd = NULL;
static lv_display_t* g_disp = NULL;
static lv_obj_t* g_ball = NULL;

static uint8_t g_render_buf[LCD_HOR_RES * LCD_BUF_LINES * 2];

static int32_t g_ball_x = (LCD_HOR_RES - BALL_SIZE) / 2;
static int32_t g_ball_y = (LCD_VER_RES - BALL_SIZE) / 2;
static int32_t g_vx = BALL_DX;
static int32_t g_vy = BALL_DY;

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

static void lvgl_ball_init(void) {
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
    St7789_Set_Backlight(g_lcd, 1);

    lv_init();
    lv_tick_set_cb(lvgl_get_tick);

    g_disp =
        lv_st7789_create(LCD_HOR_RES, LCD_VER_RES, LV_LCD_FLAG_NONE, lvgl_send_cmd_cb, lvgl_send_color_cb);
    if (g_disp == NULL) { return; }
    lv_display_set_buffers(g_disp, g_render_buf, NULL, sizeof(g_render_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_lcd_generic_mipi_set_invert(g_disp, 0);

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000020), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* border = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(border);
    lv_obj_set_size(border, LCD_HOR_RES - 2 * BORDER_INSET, LCD_VER_RES - 2 * BORDER_INSET);
    lv_obj_set_pos(border, BORDER_INSET, BORDER_INSET);
    lv_obj_set_style_border_color(border, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_border_width(border, BORDER_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_border_opa(border, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_invalidate(border);

    g_ball = lv_label_create(lv_screen_active());
    lv_obj_set_size(g_ball, BALL_SIZE, BALL_SIZE);
    lv_obj_set_pos(g_ball, g_ball_x, g_ball_y);
    lv_label_set_text(g_ball, " ");
    lv_obj_set_style_bg_color(g_ball, lv_color_hex(0xff4040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_ball, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_invalidate(g_ball);
}

static void lvgl_ball_loop(void) {
    if (g_ball == NULL) { return; }

    int32_t next_x = g_ball_x + g_vx;
    int32_t next_y = g_ball_y + g_vy;

    if (next_x <= FIELD_X_MIN) {
        next_x = FIELD_X_MIN;
        g_vx = -g_vx;
    } else if (next_x >= FIELD_X_MAX) {
        next_x = FIELD_X_MAX;
        g_vx = -g_vx;
    }

    if (next_y <= FIELD_Y_MIN) {
        next_y = FIELD_Y_MIN;
        g_vy = -g_vy;
    } else if (next_y >= FIELD_Y_MAX) {
        next_y = FIELD_Y_MAX;
        g_vy = -g_vy;
    }

    g_ball_x = next_x;
    g_ball_y = next_y;
    lv_obj_set_pos(g_ball, g_ball_x, g_ball_y);
    lv_obj_invalidate(g_ball);

    lv_timer_handler();
}

#else

static void lvgl_ball_init(void) {}

static void lvgl_ball_loop(void) {}

#endif

static void lvgl_ball_task(void* arg) {
    (void)arg;
    lvgl_ball_init();
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        lvgl_ball_loop();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(20));
    }
}

void Test_Lvgl_Ball_Task_Def(void) { xTaskCreate(lvgl_ball_task, "LVGL_Ball", 1024, NULL, 1, NULL); }
