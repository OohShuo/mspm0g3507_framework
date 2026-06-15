#include "FreeRTOS.h"
#include "bsp_adc.h"
#include "bsp_spi.h"
#include "bsp_uart.h"
#include "rtt_log.h"
#include "task.h"
#include "ti_msp_dl_config.h"

void ADC12_0_INST_IRQHandler(void) { Bsp_Adc_Irq_Handler(ADC0); }

void SPI1_IRQHandler(void) { Bsp_Hard_Spi_Irq_Handler(SPI1); }

void UART0_IRQHandler(void) {
#if FRAMEWORK_USE_UART
    Bsp_Uart_Irq_Handler(UART0);
#endif
}

void TIMA1_IRQHandler(void) {
#if FRAMEWORK_USE_UART
    switch (DL_TimerA_getPendingInterrupt(TIMA1)) {
        case DL_TIMERA_INTERRUPT_ZERO_EVENT:
            Bsp_Uart_Idle_Irq_Handler(0);
            break;
        default:
            break;
    }
#endif
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    printf("STACK OVERFLOW: %s\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationMallocFailedHook(void) {
    printf("MALLOC FAILED: heap exhausted\n");
    taskDISABLE_INTERRUPTS();
    for (;;);
}
