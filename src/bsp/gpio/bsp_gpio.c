#include "bsp_gpio.h"

#include "board_config.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "devices/msp/peripherals/hw_gpio.h"
#include "dl_gpio.h"

#if GPIO_NUM

struct Bsp_gpio_instances_t {
    GPIO_Regs* port;
    uint32_t pin;
    enum {
        bsp_gpio_mode_input,
        bsp_gpio_mode_output,
        bsp_gpio_mode_input_pullup,
        bsp_gpio_mode_input_pulldown
    } mode;
};

struct Bsp_gpio_instances_t bsp_gpio_instances[GPIO_NUM] = {0};

void Bsp_Gpio_Init(void) {
    for (int i = 0; i < GPIO_NUM; i++) {
        bsp_gpio_instances[i].port = ((GPIO_Regs*[])GPIO_PORTS)[i];
        bsp_gpio_instances[i].pin = ((uint32_t[])GPIO_PINS)[i];
        bsp_gpio_instances[i].mode = ((int[])GPIO_MODES)[i];
    }
}

void Bsp_Gpio_Write(uint32_t idx, Bsp_gpio_state state) {
    if (idx >= GPIO_NUM) return;

    if (bsp_gpio_instances[idx].mode != bsp_gpio_mode_output) { return; }

    if (state == bsp_gpio_state_set) {
        DL_GPIO_setPins(bsp_gpio_instances[idx].port, bsp_gpio_instances[idx].pin);
    } else {
        DL_GPIO_clearPins(bsp_gpio_instances[idx].port, bsp_gpio_instances[idx].pin);
    }
}

void Bsp_Gpio_Toggle(uint32_t idx) {
    if (idx >= GPIO_NUM) return;

    if (bsp_gpio_instances[idx].mode != bsp_gpio_mode_output) { return; }

    DL_GPIO_togglePins(bsp_gpio_instances[idx].port, bsp_gpio_instances[idx].pin);
}

Bsp_gpio_state Bsp_Gpio_Read(uint32_t idx) {
    if (idx >= GPIO_NUM) return bsp_gpio_state_reset;

    if (bsp_gpio_instances[idx].mode == bsp_gpio_mode_output) { return bsp_gpio_state_err; }

    uint32_t res = DL_GPIO_readPins(bsp_gpio_instances[idx].port, bsp_gpio_instances[idx].pin);
    return (res != 0) ? bsp_gpio_state_set : bsp_gpio_state_reset;
}

#else

void Bsp_Gpio_Init(void) {}

void Bsp_Gpio_Write(uint32_t idx, Bsp_gpio_state state) {
    (void)idx;
    (void)state;
}

void Bsp_Gpio_Toggle(uint32_t idx) { (void)idx; }

Bsp_gpio_state Bsp_Gpio_Read(uint32_t idx) {
    (void)idx;
    return bsp_gpio_state_reset;
}

#endif