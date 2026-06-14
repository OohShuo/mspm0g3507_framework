#include "ws2812.h"

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_rz.h"
#include "freertos_alloc.h"
#include "task.h"

static const Bsp_rz_config ws2812_timing = {
    .period_ns = 1250,
    .one_code = {.high_ns = 700, .low_ns = 550},
    .zero_code = {.high_ns = 350, .low_ns = 900},
    .reset_required_us = 100,
};

void Ws2812_Init(void) {
    Bsp_rz_config cfg = ws2812_timing;
    Bsp_Rz_Set_Config(RZ_WS2812_IDX, &cfg);
}

Ws2812* Ws2812_Create(const Ws2812_config* config) {
    if (config == NULL) return NULL;
    if (config->led_count > WS2812_MAX_LEDS) return NULL;

    Ws2812* obj = (Ws2812*)pvPortMalloc(sizeof(Ws2812));
    if (obj == NULL) return NULL;

    memset(obj, 0, sizeof(Ws2812));
    obj->config = *config;
    return obj;
}

void Ws2812_Set_All(Ws2812* obj, uint8_t r, uint8_t g, uint8_t b) {
    if (obj == NULL) return;

    for (uint8_t i = 0; i < obj->config.led_count; i++) {
        obj->pixel_data[i * 3 + 0] = g;
        obj->pixel_data[i * 3 + 1] = r;
        obj->pixel_data[i * 3 + 2] = b;
    }
}

void Ws2812_Set_Pixel(Ws2812* obj, uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (obj == NULL) return;
    if (index >= obj->config.led_count) return;

    obj->pixel_data[index * 3 + 0] = g;
    obj->pixel_data[index * 3 + 1] = r;
    obj->pixel_data[index * 3 + 2] = b;
}

void Ws2812_Set_Serial(Ws2812* obj, const uint8_t* color_arr, const uint8_t* idx_arr, uint8_t num) {
    if (obj == NULL || color_arr == NULL || idx_arr == NULL) return;

    for (uint8_t i = 0; i < num; i++) {
        uint8_t index = idx_arr[i];
        if (index >= obj->config.led_count) continue;

        obj->pixel_data[index * 3 + 0] = color_arr[i * 3 + 0];
        obj->pixel_data[index * 3 + 1] = color_arr[i * 3 + 1];
        obj->pixel_data[index * 3 + 2] = color_arr[i * 3 + 2];
    }
}

void Ws2812_Update(Ws2812* obj) {
    if (obj == NULL) return;

    if (Bsp_Rz_Is_Busy(obj->config.rz_idx)) { return; }

    Bsp_Rz_Start(obj->config.rz_idx, obj->pixel_data, (uint32_t)obj->config.led_count * 3);
}
