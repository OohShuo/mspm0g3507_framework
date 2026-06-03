#pragma once

#include <stdint.h>

void Bsp_Pwm_Init(void);

void Bsp_Pwm_Set_Duty(uint32_t idx, float duty);
void Bsp_Pwm_Set_Freq(uint32_t idx, uint32_t freq);
void Bsp_Pwm_Start(uint32_t idx);
void Bsp_Pwm_Stop(uint32_t idx);