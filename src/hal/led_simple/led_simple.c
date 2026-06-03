#include "led_simple.h"

#include <stdlib.h>
#include <string.h>

#include "bsp_time.h"
#include "vector.h"

static Vector* led_simple_instances = NULL;

void Led_Simple_Init(void) {
    if (led_simple_instances == NULL) { led_simple_instances = Vector_Init(sizeof(Led_simple*), 4); }
}

Led_simple* Led_Simple_Create(const Led_simple_config* config) {
    if (config == NULL || led_simple_instances == NULL) return NULL;

    Led_simple* obj = malloc(sizeof(Led_simple));
    if (obj == NULL) return NULL;

    memset(obj, 0, sizeof(Led_simple));

    obj->config = *config;

    obj->state = led_simple_state_off;
    if (obj->config.gpio_state_when_on == bsp_gpio_state_set) {
        obj->gpio_states_map[led_simple_state_off] = bsp_gpio_state_reset;
        obj->gpio_states_map[led_simple_state_on] = bsp_gpio_state_set;
    } else {
        obj->gpio_states_map[led_simple_state_off] = bsp_gpio_state_set;
        obj->gpio_states_map[led_simple_state_on] = bsp_gpio_state_reset;
    }

    Vector_Push_Back(led_simple_instances, (void*)&obj);

    return obj;
}

void Led_Simple_Set_State(Led_simple* obj, enum Led_simple_state_e state) {
    if (obj == NULL) return;

    obj->state = state;

    Bsp_Gpio_Write(obj->config.gpio_idx, obj->gpio_states_map[obj->state]);
}

void Led_Simple_Toggle(Led_simple* obj) {
    if (obj == NULL) return;

    obj->state = (obj->state == led_simple_state_off) ? led_simple_state_on : led_simple_state_off;

    Bsp_Gpio_Write(obj->config.gpio_idx, obj->gpio_states_map[obj->state]);
}

void Led_Simple_Update_All(void) {
    if (led_simple_instances == NULL) return;

    for (uint32_t i = 0; i < Vector_Get_Size(led_simple_instances); i++) {
        Led_simple* obj = *(Led_simple**)Vector_Get_At(led_simple_instances, i);
        if (obj == NULL) continue;

        if (obj->config.use_as_indicator) { Led_Simple_Toggle(obj); }
    }
}