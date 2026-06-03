#include "app.h"

#include <stdint.h>
#include <stdlib.h>

#include "bsp_time.h"
#include "button.h"
#include "led_breath.h"
#include "led_simple.h"

Led_simple* led_indicator;
Led_breath* led_breath;
Button* button1;

void App_Init(void) {
    Led_simple_config led_cfg = {
        .gpio_idx = 0, .use_as_indicator = 1, .blink_freq_hz = 2, .gpio_state_when_on = bsp_gpio_state_set};
    led_indicator = Led_Simple_Create(&led_cfg);

    Led_breath_config led_breath_cfg = {.pwm_idx = 0, .max_brightness = 100, .breath_freq_hz = 1.0f};
    led_breath = Led_Breath_Create(&led_breath_cfg);

    Button_config btn_cfg = {.gpio_idx = 1, .gpio_state_when_pressed = bsp_gpio_state_set};
    button1 = Button_Create(&btn_cfg);
}

void App_Loop(void) {
    if (Button_Get_State(button1) == button_state_down) {
        Led_Breath_Set_Freq(led_breath, 2.5f);
    } else {
        Led_Breath_Set_Freq(led_breath, 0.5f);
    }
}