#include <stdlib.h>
#include <string.h>

#include "button.h"
#include "input_vm.h"

#define VM_BUTTON_COUNT 10u

static Button* g_buttons[VM_BUTTON_COUNT];
static uint8_t g_button_count;

void Button_Init(void) {}

Button* Button_Create(const Button_config* c) {
    if (!c || g_button_count >= VM_BUTTON_COUNT) return NULL;
    Button* button = calloc(1, sizeof(Button));
    if (!button) return NULL;
    button->config = *c;
    button->state = button_state_up;
    button->last_state = button_state_up;
    g_buttons[g_button_count++] = button;
    return button;
}

Button_state Button_Get_State(Button* o) {
    if (!o) return button_state_up;
    return o->state;
}

void Button_Update_All(void) {
    for (uint8_t i = 0; i < g_button_count; i++) {
        Button* button = g_buttons[i];
        const Bsp_gpio_state gpio = Bsp_Gpio_Read(button->config.gpio_idx);
        const Button_state raw =
            gpio == button->config.gpio_state_when_pressed ? button_state_down : button_state_up;
        button->state = raw;
        button->last_state = raw;
    }
}
