#include "joystick.h"

#include <stdlib.h>
#include <string.h>

#include "bsp_adc.h"
#include "vector.h"

static Vector* joystick_instances = NULL;

static void joystick_adc_cb(void* arg);

void Joystick_Init(void) {
    if (joystick_instances == NULL) { joystick_instances = Vector_Init(sizeof(Joystick*), 4); }
}

Joystick* Joystick_Create(const Joystick_config* config) {
    if (config == NULL || joystick_instances == NULL) return NULL;

    Joystick* obj = malloc(sizeof(Joystick));
    if (obj == NULL) return NULL;

    memset(obj, 0, sizeof(Joystick));

    obj->config = *config;
    obj->x_value = 0;
    obj->y_value = 0;

    Vector_Push_Back(joystick_instances, (void*)&obj);

    Bsp_Adc_Register_Cb_Dma_Done(config->adc_idx, joystick_adc_cb, obj);

    Bsp_Adc_Start(config->adc_idx);

    return obj;
}

static void joystick_adc_cb(void* arg) {
    if (arg == NULL) return;

    Joystick* obj = (Joystick*)arg;

    uint32_t adc_idx = obj->config.adc_idx;
    uint32_t channel_x = obj->config.adc_channel_x;
    uint32_t channel_y = obj->config.adc_channel_y;

    obj->x_value = Bsp_Adc_Read_Voltage(adc_idx, channel_x) / ADC_REF_VOLTAGE * 2 - 1;
    obj->y_value = Bsp_Adc_Read_Voltage(adc_idx, channel_y) / ADC_REF_VOLTAGE * 2 - 1;

    obj->x_value -= obj->config.x_offset;
    obj->y_value -= obj->config.y_offset;

    if (obj->config.x_reverse) { obj->x_value *= -1; }
    if (obj->config.y_reverse) { obj->y_value *= -1; }
}
