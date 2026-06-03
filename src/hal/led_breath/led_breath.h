#pragma once

#include <stdint.h>

#include "bsp_pwm.h"

typedef struct {
    uint32_t pwm_idx;

    uint8_t max_brightness;  // 0-100
    float breath_freq_hz;    // 呼吸频率，单位 Hz
} Led_breath_config;

typedef struct {
    Led_breath_config config;

    uint8_t current_brightness;
    int8_t direction;

    float breath_freq_hz;
    uint32_t last_update_time_ms;
} Led_breath;

void Led_Breath_Init(void);

Led_breath* Led_Breath_Create(const Led_breath_config* config);
void Led_Breath_Set_Freq(Led_breath* obj, float freq_hz);
void Led_Breath_Update_All(void);
