#include "test_lvgl_ball.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "joystick.h"
#include "st7789.h"
#include "task.h"

#define LCD_HOR_RES 240
#define LCD_VER_RES 320

#define BORDER_INSET 5
#define BORDER_WIDTH 2
#define BALL_SIZE    25

#define FIELD_X_MIN (BORDER_INSET + BORDER_WIDTH)
#define FIELD_Y_MIN (BORDER_INSET + BORDER_WIDTH)
#define FIELD_X_MAX (LCD_HOR_RES - BORDER_INSET - BORDER_WIDTH - BALL_SIZE)
#define FIELD_Y_MAX (LCD_VER_RES - BORDER_INSET - BORDER_WIDTH - BALL_SIZE)

#define JOYSTICK_DEAD_ZONE 0.18f
#define JOYSTICK_MAX_SPEED 5.0f
#define JOYSTICK_CALIBRATION_SAMPLES 32u
#define JOYSTICK_CALIBRATION_DELAY_MS 5u

#define COLOR_BACKGROUND 0x0004u
#define COLOR_BORDER     0xffffu
#define COLOR_BALL       0xfa08u

static St7789* g_lcd = NULL;
static Joystick* g_joystick = NULL;

static uint16_t g_line_buf[LCD_HOR_RES];
static uint16_t g_ball_buf[BALL_SIZE * BALL_SIZE];

static float g_ball_x = (LCD_HOR_RES - BALL_SIZE) / 2.0f;
static float g_ball_y = (LCD_VER_RES - BALL_SIZE) / 2.0f;
static int32_t g_drawn_ball_x = (LCD_HOR_RES - BALL_SIZE) / 2;
static int32_t g_drawn_ball_y = (LCD_VER_RES - BALL_SIZE) / 2;

static float clampf(float value, float min_value, float max_value) {
    if (value < min_value) { return min_value; }
    if (value > max_value) { return max_value; }
    return value;
}

static float apply_dead_zone(float value) {
    const float magnitude = value < 0.0f ? -value : value;
    if (magnitude <= JOYSTICK_DEAD_ZONE) { return 0.0f; }

    const float scaled = (magnitude - JOYSTICK_DEAD_ZONE) / (1.0f - JOYSTICK_DEAD_ZONE);
    return value < 0.0f ? -scaled : scaled;
}

static void draw_field(void) {
    for (int32_t y = 0; y < LCD_VER_RES; y++) {
        for (int32_t x = 0; x < LCD_HOR_RES; x++) {
            const uint8_t inside_outer =
                x >= BORDER_INSET && x < LCD_HOR_RES - BORDER_INSET && y >= BORDER_INSET &&
                y < LCD_VER_RES - BORDER_INSET;
            const uint8_t inside_inner =
                x >= BORDER_INSET + BORDER_WIDTH && x < LCD_HOR_RES - BORDER_INSET - BORDER_WIDTH &&
                y >= BORDER_INSET + BORDER_WIDTH && y < LCD_VER_RES - BORDER_INSET - BORDER_WIDTH;

            g_line_buf[x] = inside_outer && !inside_inner ? COLOR_BORDER : COLOR_BACKGROUND;
        }

        St7789_Flush(g_lcd, 0, y, LCD_HOR_RES - 1, y, (uint8_t*)g_line_buf, sizeof(g_line_buf));
    }
}

static void draw_ball_area(int32_t x, int32_t y, uint8_t show_ball) {
    const int32_t radius = BALL_SIZE / 2;

    for (int32_t row = 0; row < BALL_SIZE; row++) {
        for (int32_t col = 0; col < BALL_SIZE; col++) {
            const int32_t dx = col - radius;
            const int32_t dy = row - radius;
            const uint8_t inside_ball = dx * dx + dy * dy <= radius * radius;
            g_ball_buf[row * BALL_SIZE + col] =
                show_ball && inside_ball ? COLOR_BALL : COLOR_BACKGROUND;
        }
    }

    St7789_Flush(
        g_lcd, x, y, x + BALL_SIZE - 1, y + BALL_SIZE - 1, (uint8_t*)g_ball_buf, sizeof(g_ball_buf));
}

static void ball_demo_init(void) {
    const Joystick_config joystick_cfg = {
        .adc_idx = ADC_JOYSTICK_IDX,
        .adc_channel_x = ADC_JOYSTICK_X_CHANNEL,
        .adc_channel_y = ADC_JOYSTICK_Y_CHANNEL,
        .x_min_voltage = JOYSTICK_X_MIN_VOLTAGE,
        .x_max_voltage = JOYSTICK_X_MAX_VOLTAGE,
        .y_min_voltage = JOYSTICK_Y_MIN_VOLTAGE,
        .y_max_voltage = JOYSTICK_Y_MAX_VOLTAGE,
        .x_reverse = JOYSTICK_X_REVERSE,
        .y_reverse = JOYSTICK_Y_REVERSE,
    };
    g_joystick = Joystick_Create(&joystick_cfg);
    Joystick_Calibrate_Center(
        g_joystick, JOYSTICK_CALIBRATION_SAMPLES, JOYSTICK_CALIBRATION_DELAY_MS);

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

    draw_field();
    draw_ball_area(g_drawn_ball_x, g_drawn_ball_y, 1);
}

static void ball_demo_loop(void) {
    if (g_lcd == NULL || g_joystick == NULL) { return; }

    const float axis_x = apply_dead_zone(g_joystick->x_value);
    const float axis_y = apply_dead_zone(g_joystick->y_value);

    g_ball_x += axis_x * JOYSTICK_MAX_SPEED;
    g_ball_y -= axis_y * JOYSTICK_MAX_SPEED;

    g_ball_x = clampf(g_ball_x, FIELD_X_MIN, FIELD_X_MAX);
    g_ball_y = clampf(g_ball_y, FIELD_Y_MIN, FIELD_Y_MAX);

    const int32_t next_x = (int32_t)(g_ball_x + 0.5f);
    const int32_t next_y = (int32_t)(g_ball_y + 0.5f);
    if (next_x == g_drawn_ball_x && next_y == g_drawn_ball_y) { return; }

    draw_ball_area(g_drawn_ball_x, g_drawn_ball_y, 0);
    draw_ball_area(next_x, next_y, 1);
    g_drawn_ball_x = next_x;
    g_drawn_ball_y = next_y;
}

static void ball_demo_task(void* arg) {
    (void)arg;
    ball_demo_init();

    uint32_t tick = xTaskGetTickCount();
    while (1) {
        ball_demo_loop();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(20));
    }
}

void Test_Lvgl_Ball_Task_Def(void) { xTaskCreate(ball_demo_task, "Ball", 256, NULL, 1, NULL); }
