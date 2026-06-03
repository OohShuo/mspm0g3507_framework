#pragma once

#include <stdint.h>

#include "bsp_gpio.h"

typedef enum { button_state_up, button_state_down } Button_state;

typedef struct {
    uint32_t gpio_idx;
    Bsp_gpio_state gpio_state_when_pressed;
} Button_config;

typedef struct {
    Button_config config;

    Button_state state;
    Button_state last_state;

    uint32_t last_state_change_time;
    uint8_t debouncing;
} Button;

void Button_Init(void);

Button* Button_Create(const Button_config* config);
Button_state Button_Get_State(Button* obj);
void Button_Update_All(void);
