#if FRAMEWORK_USE_LVGL

// clang-format off

#include "lvgl_ball.h"

#include <stdbool.h>
#include <stddef.h>

#include "board_config.h"
#include "bsp.h"
#include "bsp_time.h"
#include "lvgl.h"
#include "src/drivers/display/st7789/lv_st7789.h"
#include "st7789.h"

// === Panel geometry — matches lvgl_hello / st7789 test ===

#define LCD_HOR_RES   240
#define LCD_VER_RES   240
#define LCD_BUF_LINES (LCD_VER_RES / 10 / 4)  // LVGL docs recommendation, 1/10 screen

// === Play field ===

// 5 px from each edge gives a 230x230 inset rectangle as the "table"
// the ball bounces inside.
#define BORDER_INSET  5
#define BORDER_WIDTH  2

// Ball geometry: 10x10 px, drawn as a circle via radius=LV_RADIUS_CIRCLE.
#define BALL_SIZE     25

// Play field inner bounds for the ball's top-left corner. The ball
// travels inside the inset border, so the rightmost top-left x is
// (HOR_RES - INSET - BALL_SIZE), not the panel edge.
#define FIELD_X_MIN   BORDER_INSET
#define FIELD_Y_MIN   BORDER_INSET
#define FIELD_X_MAX   (LCD_HOR_RES - BORDER_INSET - BALL_SIZE)
#define FIELD_Y_MAX   (LCD_VER_RES - BORDER_INSET - BALL_SIZE)

// Per-frame velocity in px. dx != dy avoids the boring perfectly-45°
// diagonal that always returns to the same corner.
#define BALL_DX       3
#define BALL_DY       5

// === Module state ===

static St7789* g_lcd = NULL;
static lv_display_t* g_disp = NULL;
static lv_obj_t* g_ball = NULL;

// LVGL render buffer: 1/10 screen * 2 bytes/px (RGB565), in static RAM.
static uint8_t g_render_buf[LCD_HOR_RES * LCD_BUF_LINES * 2];

// Ball position (top-left corner) and velocity in px/frame.
static int32_t g_ball_x = (LCD_HOR_RES - BALL_SIZE) / 2;
static int32_t g_ball_y = (LCD_VER_RES - BALL_SIZE) / 2;
static int32_t g_vx = BALL_DX;
static int32_t g_vy = BALL_DY;

// === LVGL ↔ st7789 bridge ===
//
// Identical contract to lvgl_hello: lv_st7789 hands us send_cmd_cb for
// register writes and send_color_cb for RAMWR pixel payloads. Both block
// on software SPI, so returning means the bus is idle and the px_map is
// safe to hand back to LVGL via lv_display_flush_ready().

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

static uint32_t lvgl_get_tick(void) { return Bsp_Get_Tick_Ms(); }

// === Init / Loop ===

void App_Lvgl_Ball_Init(void) {
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

    lv_lcd_generic_mipi_set_invert(g_disp, true);

    // Dark background — gives the white border and red ball strong
    // contrast. NOTE: we do NOT call lv_obj_invalidate on the screen
    // here; on this lvgl v9 build that full-screen re-render seems
    // to swallow the ball's later invalidate. The first lv_timer_handler
    // pass after init will still paint the bg because the screen was
    // never drawn before, and we still get a coherent first frame.
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000020), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

    // Border: 230x230 rectangle inset 5 px from the panel edges,
    // no fill, just a 2-px white outline. Same construction as the
    // ball's parent (a base obj on the screen) — and unlike the ball,
    // we DO want radius=0 here, which is the default after
    // remove_style_all. The border has no bg, so it can never satisfy
    // the screen's bg_opa=COVER cover check and "win" against the
    // ball; it just contributes a 2-px outline.
    lv_obj_t* border = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(border);
    lv_obj_set_size(border, LCD_HOR_RES - 2 * BORDER_INSET, LCD_VER_RES - 2 * BORDER_INSET);
    lv_obj_set_pos(border, BORDER_INSET, BORDER_INSET);
    lv_obj_set_style_border_color(border, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_border_width(border, BORDER_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_border_opa(border, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_invalidate(border);

    // Ball: lv_label with bg_color = red and a single-space text. We
    // went through lv_obj + bg_color + radius and the bg never made it
    // to the framebuffer; the label's draw event paints the bg via
    // the same style machinery that the screen bg uses (which works),
    // so this is the reliable path. The text is " " so the label has
    // a non-zero bbox without drawing a glyph.
    //
    // NOTE: we intentionally do NOT set a radius here. The label's
    // LVGL_COVER_CHECK optimization rejects an obj whose bbox corners
    // fall outside its rounded shape — for a 10x10 label with
    // radius=5 (full circle), the four corner pixels are
    // sqrt(50)≈7.07 px from the center, outside the 5-px radius, so
    // the cover check returns NOT_COVER. With the screen bg at
    // bg_opa=COVER, the screen then wins the "top obj" race and the
    // renderer skips the ball entirely. Without radius, the bbox IS
    // the visible area, the cover check passes, and the ball renders
    // on top of the screen bg. We get a 10x10 red square instead of
    // a circle — same bouncing-ball demo, just square.
    g_ball = lv_label_create(lv_screen_active());
    lv_obj_set_size(g_ball, BALL_SIZE, BALL_SIZE);
    lv_obj_set_pos(g_ball, g_ball_x, g_ball_y);
    lv_label_set_text(g_ball, " ");
    lv_obj_set_style_bg_color(g_ball, lv_color_hex(0xff4040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_ball, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_invalidate(g_ball);
}

void App_Lvgl_Ball_Loop(void) {
    if (g_ball == NULL) { return; }

    // Predict the next position, then check each wall independently.
    // Bouncing on a wall reverses the corresponding velocity component
    // AND clamps the position so the ball never crosses past the
    // border (which would let it leave the play field on a fast frame).
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
    // lv_obj_set_pos routes through the local-style system, which in
    // some lvgl v9 builds does not always enqueue a redraw for the new
    // area. Force one so the ball never visually "freezes" at a stale
    // position even though the state vars are advancing.
    lv_obj_invalidate(g_ball);

    lv_timer_handler();
}

#else

void App_Lvgl_Ball_Init(void) {}

void App_Lvgl_Ball_Loop(void) {}

// clang-format on

#endif