#include "bsp.h"

#include "bsp_gpio.h"
#include "bsp_pwm.h"

void Bsp_Init(void) {
    Bsp_Gpio_Init();
    Bsp_Pwm_Init();
}