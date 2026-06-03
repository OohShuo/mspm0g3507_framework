#pragma once

#include "bsp_gpio.h"

typedef enum Led_simple_state_e { led_simple_state_off, led_simple_state_on } Led_simple_state;

typedef struct Led_simple_config_t {
    uint32_t gpio_idx;

    uint8_t use_as_indicator;
    uint8_t
        blink_freq_hz;  // 当 use_as_indicator 为 1 时有效，表示闪烁频率，单位 Hz，0 表示常亮，最大为 10 Hz

    Bsp_gpio_state gpio_state_when_on;
} Led_simple_config;

typedef struct Led_simple_t {
    Led_simple_config config;

    uint32_t last_toggle_time_ms;
    uint8_t blink_freq_hz;

    enum Led_simple_state_e state;
    Bsp_gpio_state gpio_states_map[2];
} Led_simple;

void Led_Simple_Init(void);

Led_simple* Led_Simple_Create(const Led_simple_config* config);
void Led_Simple_Set_State(Led_simple* obj, enum Led_simple_state_e state);
void Led_Simple_Toggle(Led_simple* obj);
void Led_Simple_Set_Blink_Freq(Led_simple* obj, uint8_t freq_hz);
void Led_Simple_Update_All(void);