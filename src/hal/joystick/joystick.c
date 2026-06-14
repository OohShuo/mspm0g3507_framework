#include "joystick.h"

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bsp_adc.h"
#include "freertos_alloc.h"
#include "task.h"
#include "vector.h"

static Vector* joystick_instances = NULL;

static void joystick_adc_cb(void* arg);
static float normalize_axis(float voltage, float min_voltage, float center_voltage, float max_voltage);
static float apply_dead_zone(float value, float dead_zone);

void Joystick_Init(void) {
    if (joystick_instances == NULL) { joystick_instances = Vector_Init(sizeof(Joystick*), 4); }
}

Joystick* Joystick_Create(const Joystick_config* config) {
    if (config == NULL || joystick_instances == NULL) return NULL;

    Joystick* obj = (Joystick*)pvPortMalloc(sizeof(Joystick));
    if (obj == NULL) return NULL;

    memset(obj, 0, sizeof(Joystick));

    obj->config = *config;
    obj->x_center_voltage = (config->x_min_voltage + config->x_max_voltage) * 0.5f;
    obj->y_center_voltage = (config->y_min_voltage + config->y_max_voltage) * 0.5f;
    obj->x_value = 0;
    obj->y_value = 0;

    Vector_Push_Back(joystick_instances, (void*)&obj);

    Bsp_Adc_Register_Cb_Dma_Done(config->adc_idx, joystick_adc_cb, obj);

    Bsp_Adc_Start(config->adc_idx);

    return obj;
}

void Joystick_Calibrate_Center(Joystick* obj, uint32_t sample_count, uint32_t sample_interval_ms) {
    if (obj == NULL || sample_count == 0) { return; }

    float x_sum = 0.0f;
    float y_sum = 0.0f;

    for (uint32_t i = 0; i < sample_count; i++) {
        if (sample_interval_ms > 0) { vTaskDelay(pdMS_TO_TICKS(sample_interval_ms)); }
        x_sum += Bsp_Adc_Read_Voltage(obj->config.adc_idx, obj->config.adc_channel_x);
        y_sum += Bsp_Adc_Read_Voltage(obj->config.adc_idx, obj->config.adc_channel_y);
    }

    const float x_center = x_sum / sample_count;
    const float y_center = y_sum / sample_count;

    if (x_center > obj->config.x_min_voltage && x_center < obj->config.x_max_voltage) {
        obj->x_center_voltage = x_center;
    }
    if (y_center > obj->config.y_min_voltage && y_center < obj->config.y_max_voltage) {
        obj->y_center_voltage = y_center;
    }
}

static float normalize_axis(float voltage, float min_voltage, float center_voltage, float max_voltage) {
    if (min_voltage >= center_voltage || center_voltage >= max_voltage) {
        return voltage / ADC_REF_VOLTAGE * 2.0f - 1.0f;
    }

    float value;
    if (voltage >= center_voltage) {
        value = (voltage - center_voltage) / (max_voltage - center_voltage);
    } else {
        value = (voltage - center_voltage) / (center_voltage - min_voltage);
    }

    if (value < -1.0f) { value = -1.0f; }
    if (value > 1.0f) { value = 1.0f; }
    return value;
}

static float apply_dead_zone(float value, float dead_zone) {
    if (dead_zone <= 0.0f) { return value; }
    if (dead_zone >= 1.0f) { return 0.0f; }

    const float magnitude = value < 0.0f ? -value : value;
    if (magnitude <= dead_zone) { return 0.0f; }

    float scaled = (magnitude - dead_zone) / (1.0f - dead_zone);
    if (scaled > 1.0f) { scaled = 1.0f; }
    return value < 0.0f ? -scaled : scaled;
}

static void joystick_adc_cb(void* arg) {
    if (arg == NULL) return;

    Joystick* obj = (Joystick*)arg;

    uint32_t adc_idx = obj->config.adc_idx;
    uint32_t channel_x = obj->config.adc_channel_x;
    uint32_t channel_y = obj->config.adc_channel_y;

    obj->x_value = normalize_axis(Bsp_Adc_Read_Voltage(adc_idx, channel_x), obj->config.x_min_voltage,
        obj->x_center_voltage, obj->config.x_max_voltage);
    obj->y_value = normalize_axis(Bsp_Adc_Read_Voltage(adc_idx, channel_y), obj->config.y_min_voltage,
        obj->y_center_voltage, obj->config.y_max_voltage);

    obj->x_value -= obj->config.x_offset;
    obj->y_value -= obj->config.y_offset;

    if (obj->config.x_reverse) { obj->x_value *= -1; }
    if (obj->config.y_reverse) { obj->y_value *= -1; }

    obj->x_value = apply_dead_zone(obj->x_value, obj->config.x_dead_zone);
    obj->y_value = apply_dead_zone(obj->y_value, obj->config.y_dead_zone);
}
