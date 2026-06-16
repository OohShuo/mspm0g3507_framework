#include <stdlib.h>
#include <string.h>

#include "button.h"
#include "input_vm.h"

static Button* g_btn = NULL;

void Button_Init(void) {}

Button* Button_Create(const Button_config* c) {
    if (!c || g_btn) return NULL;
    g_btn = calloc(1, sizeof(Button));
    if (!g_btn) return NULL;
    g_btn->config = *c;
    g_btn->state = button_state_up;
    g_btn->last_state = button_state_up;
    return g_btn;
}

Button_state Button_Get_State(Button* o) {
    if (!o) return button_state_up;
    return o->state;
}

void Button_Update_All(void) {
    if (!g_btn) return;
    uint8_t pressed = Vm_Input_Get_Button();
    Button_state raw = pressed ? button_state_down : button_state_up;

    // Simple debounce: state changes immediately (VM doesn't need real debounce)
    if (raw != g_btn->last_state) { g_btn->state = raw; }
    g_btn->last_state = raw;
}
