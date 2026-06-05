#include "app.h"

#include <stdint.h>
#include <stdlib.h>

#include "board_config.h"
#include "bsp_time.h"
#include "button.h"
#include "buzzer.h"
#include "joystick.h"
#include "led_breath.h"
#include "led_simple.h"

Led_simple* led_indicator = NULL;
Led_breath* led_breath = NULL;
Button* button1 = NULL;
// Button* button2 = NULL;
Buzzer* buzzer = NULL;
Joystick* joystick = NULL;

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
}

void App_Loop(void) {
    static Button_state last_button_state = button_state_up;

    if (Button_Get_State(button1) != last_button_state) {
        last_button_state = Button_Get_State(button1);
        if (last_button_state == button_state_down) { Buzzer_Play(buzzer, music1, music1_len, 60 * 10, 0); }
    }
}