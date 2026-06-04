#pragma once

#include <stdint.h>

typedef struct {
    uint32_t adc_idx;
    uint32_t adc_channel_x;
    uint32_t adc_channel_y;
    float x_offset;
    float y_offset;
    uint8_t x_reverse;
    uint8_t y_reverse;
} Joystick_config;

typedef struct {
    Joystick_config config;

    // -1 to 1, where -1 is left/down and 1 is right/up
    float x_value;
    float y_value;
} Joystick;

void Joystick_Init(void);

Joystick* Joystick_Create(const Joystick_config* config);