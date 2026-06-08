#include "app.h"

#include <stddef.h>

#include "board_config.h"
#include "bsp.h"
#include "bsp_time.h"
#include "button.h"
#include "buzzer.h"
#include "joystick.h"
#include "lcd_test.h"
#include "led_breath.h"
#include "led_simple.h"
#include "w25q32_test.h"
#include "lcd.h"

static Led_simple* led_indicator = NULL;
static Led_breath* led_breath = NULL;
static Button* button1 = NULL;
// Button* button2 = NULL;
static Buzzer* buzzer = NULL;
static Joystick* joystick = NULL;

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

    /* LCD demo */
    LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);
    LCD_ShowString(30, 100, (uint8_t *)"Hello!", RED, WHITE, 32, 0);
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
