#include "bsp_adc.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "ti_msp_dl_config.h"

void ADC12_0_INST_IRQHandler(void) { Bsp_Adc_Irq_Handler(ADC0); }