#include "led_breath.h"

#include <stdint.h>
#include <string.h>

#include "bsp_time.h"
#include "freertos_alloc.h"
#include "vector.h"

static Vector* led_breath_instances = NULL;

void Led_Breath_Init(void) {
    if (led_breath_instances == NULL) { led_breath_instances = Vector_Init(sizeof(Led_breath*), 4); }
}

Led_breath* Led_Breath_Create(const Led_breath_config* config) {
    if (config == NULL || led_breath_instances == NULL) return NULL;

    Led_breath* obj = (Led_breath*)pvPortMalloc(sizeof(Led_breath));
    if (obj == NULL) return NULL;

    memset(obj, 0, sizeof(Led_breath));

    obj->config = *config;
    obj->current_brightness = 0;
    obj->direction = 1;
    obj->last_update_time_ms = 0;
    Led_Breath_Set_Freq(obj, config->breath_freq_hz);

    Bsp_Pwm_Set_Freq(obj->config.pwm_idx, 1000);
    Bsp_Pwm_Set_Duty(obj->config.pwm_idx, 0);
    Bsp_Pwm_Start(obj->config.pwm_idx);

    Vector_Push_Back(led_breath_instances, (void*)&obj);

    return obj;
}

void Led_Breath_Set_Freq(Led_breath* obj, float freq_hz) {
    if (obj == NULL) return;
    if (freq_hz < 0)
        obj->breath_freq_hz = 0;
    else if (freq_hz > 5)
        obj->breath_freq_hz = 5;
    else
        obj->breath_freq_hz = freq_hz;
}

void Led_Breath_Update_All(void) {
    if (led_breath_instances == NULL) return;

    uint32_t now_ms = Bsp_Get_Tick_Ms();

    for (uint32_t i = 0; i < Vector_Get_Size(led_breath_instances); i++) {
        Led_breath* obj = *(Led_breath**)Vector_Get_At(led_breath_instances, i);
        if (obj == NULL) continue;

        if (now_ms - obj->last_update_time_ms < 10) { continue; }
        obj->last_update_time_ms = now_ms;

        uint32_t step = (uint32_t)((obj->breath_freq_hz * 2 * obj->config.max_brightness) / 1000 * 10);
        if (step == 0 && obj->breath_freq_hz > 0) step = 1;

        if (obj->direction == 1) {
            if (obj->current_brightness + step >= obj->config.max_brightness) {
                obj->current_brightness = obj->config.max_brightness;
                obj->direction = -1;
            } else {
                obj->current_brightness += step;
            }
        } else {
            if (obj->current_brightness <= step) {
                obj->current_brightness = 0;
                obj->direction = 1;
            } else {
                obj->current_brightness -= step;
            }
        }
        Bsp_Pwm_Set_Duty(obj->config.pwm_idx, obj->current_brightness / 100.0f);
    }
}
