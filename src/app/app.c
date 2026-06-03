#include "app.h"

#include "led_simple.h"

Led_simple* led_indicator;

void App_Init(void) {
    Led_simple_config led_cfg = {
        .gpio_idx = 0,
        .use_as_indicator = 1,
        .gpio_state_when_on = bsp_gpio_state_set
    };
    led_indicator = Led_Simple_Create(&led_cfg);
}