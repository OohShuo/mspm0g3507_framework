#include "bsp_gpio.h"

#include "board_config.h"
#include "input_vm.h"
void Bsp_Gpio_Init(void) {}
void Bsp_Gpio_Write(uint32_t i, Bsp_gpio_state s) {
    (void)i;
    (void)s;
}
void Bsp_Gpio_Toggle(uint32_t i) { (void)i; }
Bsp_gpio_state Bsp_Gpio_Read(uint32_t i) {
    uint8_t pressed = 0;
    if (i == GPIO_SW_BTN_IDX) {
        pressed = Vm_Input_Get_Button();
    } else if (i == GPIO_BNT_DOWN_IDX) {
        pressed = Vm_Input_Get_A();
    } else if (i == GPIO_BNT_RIGHT_IDX) {
        pressed = Vm_Input_Get_B();
    } else if (i == GPIO_BNT_UP_IDX) {
        pressed = Vm_Input_Get_X_Button();
    } else if (i == GPIO_BNT_LEFT_IDX) {
        pressed = Vm_Input_Get_Y_Button();
    }
    return pressed ? bsp_gpio_state_reset : bsp_gpio_state_set;
}
