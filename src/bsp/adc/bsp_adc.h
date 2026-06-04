#pragma once

#include <stdint.h>

#include "devices/msp/peripherals/hw_adc12.h"

#define ADC_REF_VOLTAGE 3.3f
#define ADC_RESOLUTION  (1u << 12)  // 12-bit ADC

typedef void (*Bsp_adc_cb_t)(void* arg);

void Bsp_Adc_Init(void);
void Bsp_Adc_Start(uint32_t idx);
void Bsp_Adc_Stop(uint32_t idx);
float Bsp_Adc_Read_Voltage(uint32_t idx, uint32_t channel);
void Bsp_Adc_Register_Cb_Dma_Done(uint32_t idx, Bsp_adc_cb_t cb, void* cb_arg);

void Bsp_Adc_Irq_Handler(ADC12_Regs* adc_inst);
