#pragma once
#include <stdint.h>

#define WS2812_MAX_LEDS 42

typedef struct {
    uint8_t rz_idx;
    uint8_t led_count;
} Ws2812_config;

typedef struct {
    Ws2812_config config;
    uint8_t pixel_data[WS2812_MAX_LEDS * 3];
} Ws2812;

void Ws2812_Init(void);

Ws2812* Ws2812_Create(const Ws2812_config* config);
void Ws2812_Set_All(Ws2812* obj, uint8_t r, uint8_t g, uint8_t b);
void Ws2812_Set_Pixel(Ws2812* obj, uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void Ws2812_Set_Serial(Ws2812* obj, const uint8_t* color_arr, const uint8_t* idx_arr, uint8_t num);
void Ws2812_Update(Ws2812* obj);
