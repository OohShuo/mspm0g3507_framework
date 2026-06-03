#include "button.h"

#include <stdlib.h>
#include <string.h>

#include "bsp_time.h"
#include "vector.h"

#define DEBOUNCE_TIME_MS 20

static Vector* button_instances = NULL;

void Button_Init(void) {
    if (button_instances == NULL) { button_instances = Vector_Init(sizeof(Button*), 4); }
}

Button* Button_Create(const Button_config* config) {
    if (config == NULL || button_instances == NULL) return NULL;

    Button* obj = malloc(sizeof(Button));
    if (obj == NULL) return NULL;

    memset(obj, 0, sizeof(Button));

    obj->config = *config;

    obj->state = button_state_up;
    obj->last_state = button_state_up;
    obj->last_state_change_time = 0;

    Vector_Push_Back(button_instances, (void*)&obj);

    return obj;
}

Button_state Button_Get_State(Button* obj) {
    if (obj == NULL) return button_state_up;

    return obj->state;
}

void Button_Update_All(void) {
    if (button_instances == NULL) return;

    for (uint32_t i = 0; i < Vector_Get_Size(button_instances); i++) {
        Button* obj = *(Button**)Vector_Get_At(button_instances, i);

        if (obj == NULL) continue;

        Bsp_gpio_state gpio_state = Bsp_Gpio_Read(obj->config.gpio_idx);
        Button_state new_state =
            (gpio_state == obj->config.gpio_state_when_pressed) ? button_state_down : button_state_up;

        if (new_state != obj->last_state && !obj->debouncing) {
            obj->debouncing = 1;
            obj->last_state_change_time = Bsp_Get_Tick_Ms();
        }
        if (obj->debouncing) {
            if (Bsp_Get_Tick_Ms() - obj->last_state_change_time >= DEBOUNCE_TIME_MS) { obj->debouncing = 0; }
        }
        if (!obj->debouncing) { obj->state = new_state; }
        obj->last_state = new_state;
    }
}
