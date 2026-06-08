#include <src/tick/lv_tick.h>

#include "FreeRTOS.h"
#include "SEGGER_RTT.h"
#include "bsp_adc.h"
#include "bsp_spi.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "task.h"
#include "ti_msp_dl_config.h"

void ADC12_0_INST_IRQHandler(void) { Bsp_Adc_Irq_Handler(ADC0); }

void SPI1_IRQHandler(void) { Bsp_Spi_Irq_Handler(SPI1); }

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    SEGGER_RTT_printf(0, "STACK OVERFLOW: %s\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;);  // halt so debugger catches it
}

void vApplicationMallocFailedHook(void) {
    SEGGER_RTT_printf(0, "MALLOC FAILED: heap exhausted\n");
    taskDISABLE_INTERRUPTS();
    for (;;);
}
