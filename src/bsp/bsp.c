#include "bsp.h"

#include "bsp_adc.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include "bsp_spi.h"
#include "bsp_uart.h"

void Bsp_Init(void) {
    Bsp_Gpio_Init();
    Bsp_Pwm_Init();
    Bsp_Adc_Init();
    Bsp_Hard_Spi_Init();
    Bsp_Soft_Spi_Init();
    Bsp_Uart_Init();
}