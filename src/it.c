#include <src/tick/lv_tick.h>

#include "FreeRTOS.h"
#include "bsp_adc.h"
#include "bsp_log.h"
#include "bsp_spi.h"
#include "task.h"
#include "ti_msp_dl_config.h"

void ADC12_0_INST_IRQHandler(void) { Bsp_Adc_Irq_Handler(ADC0); }

void SPI1_IRQHandler(void) { Bsp_Spi_Irq_Handler(SPI1); }

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    bsp_printf("STACK OVERFLOW: %s\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationMallocFailedHook(void) {
    bsp_printf("MALLOC FAILED: heap exhausted\n");
    taskDISABLE_INTERRUPTS();
    for (;;);
}
