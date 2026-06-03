#include "app.h"

#include <stdint.h>
#include <stdlib.h>

#include "bsp_time.h"
#include "led_simple.h"

Led_simple* led_indicator;

void App_Init(void) {
    Led_simple_config led_cfg = {
        .gpio_idx = 0, .use_as_indicator = 1, .blink_freq_hz = 2, .gpio_state_when_on = bsp_gpio_state_set};
    led_indicator = Led_Simple_Create(&led_cfg);
}

void App_Loop(void) {
    static uint32_t last_time_ms = 0;
    static uint8_t flag = 0;
    if (led_indicator != NULL) {
        uint32_t current_time_ms = Bsp_Get_Tick_Ms();
        if (current_time_ms - last_time_ms >= 5000) {
            last_time_ms = current_time_ms;

            if (flag) {
                Led_Simple_Set_Blink_Freq(led_indicator, 2);  // 2 Hz
            } else {
                Led_Simple_Set_Blink_Freq(led_indicator, 10);  // 10 Hz
            }

            flag = !flag;
        }
    }
}