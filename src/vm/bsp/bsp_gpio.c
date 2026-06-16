#include "bsp_gpio.h"

#include "input_vm.h"
void Bsp_Gpio_Init(void) {}
void Bsp_Gpio_Write(uint32_t i, Bsp_gpio_state s) {
    (void)i;
    (void)s;
}
void Bsp_Gpio_Toggle(uint32_t i) { (void)i; }
Bsp_gpio_state Bsp_Gpio_Read(uint32_t i) {
    // GPIO_SW_BTN_IDX = 0: read from Space key
    if (i == 0) return Vm_Input_Get_Button() ? bsp_gpio_state_reset : bsp_gpio_state_set;
    return bsp_gpio_state_set;  // default: inactive high
}
