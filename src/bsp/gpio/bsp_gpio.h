#pragma once

#include <stdint.h>

typedef enum Bsp_gpio_state_e { bsp_gpio_state_reset, bsp_gpio_state_set } Bsp_gpio_state;

void Bsp_Gpio_Init(void);

void Bsp_Gpio_Write(uint32_t idx, Bsp_gpio_state state);
void Bsp_Gpio_Toggle(uint32_t idx);
void Bsp_Gpio_Read(uint32_t idx, Bsp_gpio_state* state);
