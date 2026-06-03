#pragma once

#include "bsp_gpio.h"

typedef enum Led_simple_state_e { led_simple_state_off, led_simple_state_on } Led_simple_state;

typedef struct Led_simple_config_t {
    uint32_t gpio_idx;
    uint8_t use_as_indicator;
    Bsp_gpio_state gpio_state_when_on;
} Led_simple_config;

typedef struct Led_simple_t {
    Led_simple_config config;
    enum Led_simple_state_e state;
    Bsp_gpio_state gpio_states_map[2];
} Led_simple;

void Led_Simple_Init(void);

Led_simple* Led_Simple_Create(const Led_simple_config* config);
void Led_Simple_Set_State(Led_simple* obj, enum Led_simple_state_e state);
void Led_Simple_Toggle(Led_simple* obj);
void Led_Simple_Update_All(void);