#include "app.h"

#include <stdint.h>
#include <stdlib.h>

#include "board_config.h"
// #include "bsp_spi.h"
#include "bsp_time.h"
#include "button.h"
#include "buzzer.h"
#include "joystick.h"
#include "led_breath.h"
#include "led_simple.h"
#include "st7789.h"

Led_simple* led_indicator = NULL;
Led_breath* led_breath = NULL;
Button* button1 = NULL;
// Button* button2 = NULL;
Buzzer* buzzer = NULL;
Joystick* joystick = NULL;

// === LCD test ===

#define LCD_HOR_RES 128
#define LCD_VER_RES 128

static St7789* g_lcd = NULL;
static volatile uint8_t g_flush_done = 0;
// One RGB565 line. St7789_Send_Color does the in-place byte-swap (LE → BE).
static uint16_t g_line_buf[LCD_HOR_RES];

static uint32_t tt_cnt = 0;
static void on_lcd_flush_done(void* arg) {
    (void)arg;
    g_flush_done = 1;
    tt_cnt++;
}

static void lcd_fill_line_pixels(uint16_t color) {
    for (int i = 0; i < LCD_HOR_RES; i++) g_line_buf[i] = color;
}

static void lcd_flush_line(int y, uint16_t color) {
    lcd_fill_line_pixels(color);
    g_flush_done = 0;
    // High-level wrapper: internally sends CASET + RASET (sync) then
    // RAMWR + pixel data (async). cb clears g_flush_done when DMA done.
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
    if (g_lcd == NULL) { return; }
    // St7789_Send_Init_Seq uses vTaskDelay / xSemaphoreTake, so it must be
    // called from a task after vTaskStartScheduler().
    St7789_Send_Init_Seq(g_lcd);
}

void App_Lcd_Test_Loop(void) {
    if (g_lcd == NULL) { return; }

    static uint32_t last_change = 0;
    static uint8_t color_idx = 0;
    static const uint16_t colors[] = {
        0xF800,  // red
        0x07E0,  // green
        0x001F,  // blue
        0xFFFF,  // white
        0x0000,  // black
    };

    const uint32_t now = Bsp_Get_Tick_Ms();
    if (now - last_change < 1000) { return; }
    last_change = now;

    lcd_fill_screen(colors[color_idx]);
    color_idx = (color_idx + 1) % (sizeof(colors) / sizeof(colors[0]));
}

void App_Init(void) {
    // Led_simple_config led_cfg = {
    //     .gpio_idx = 0, .use_as_indicator = 1, .blink_freq_hz = 2, .gpio_state_when_on =
    //     bsp_gpio_state_set};
    // led_indicator = Led_Simple_Create(&led_cfg);

    // Led_breath_config led_breath_cfg = {.pwm_idx = 0, .max_brightness = 100, .breath_freq_hz = 1.0f};
    // led_breath = Led_Breath_Create(&led_breath_cfg);

    Button_config btn_cfg = {.gpio_idx = GPIO_SW_BTN_IDX, .gpio_state_when_pressed = bsp_gpio_state_reset};
    button1 = Button_Create(&btn_cfg);

    Buzzer_config buzzer_cfg = {
        .pwm_idx = PWM_BUZZER_IDX,
    };
    buzzer = Buzzer_Create(&buzzer_cfg);

    Joystick_config joystick_cfg = {
        .adc_idx = ADC_JOYSTICK_IDX,
        .adc_channel_x = ADC_JOYSTICK_X_CHANNEL,
        .adc_channel_y = ADC_JOYSTICK_Y_CHANNEL,
        .x_offset = -0.0178018045,
        .y_offset = -0.0148155261,
    };
    joystick = Joystick_Create(&joystick_cfg);

    // === LCD ===
    // Pin mapping matches the working 地猛星 reference (lcd_init.h):
    //   RST = PA15  -> GPIO_2  (board_config.h has this as GPIO_TFT_DC_IDX, label is misleading)
    //   DC  = PA16  -> GPIO_1  (board_config.h has this as GPIO_TFT_RST_IDX)
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

void App_Loop(void) {
    static Button_state last_button_state = button_state_up;

    if (Button_Get_State(button1) != last_button_state) {
        last_button_state = Button_Get_State(button1);
        if (last_button_state == button_state_down) {
            Buzzer_Play(buzzer, &music_library[music_idx_mario], 0);
        }
    }
}
