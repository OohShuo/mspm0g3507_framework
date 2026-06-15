#pragma once

#include <stdint.h>

#include "st7789_def.h"

typedef void (*St7789_flush_done_cb)(void* arg);

typedef struct {
    uint32_t spi_idx;
    uint32_t cs_gpio_idx;
    uint32_t dc_gpio_idx;
    uint32_t rst_gpio_idx;
    uint32_t bkl_gpio_idx;
    uint32_t hor_res;
    uint32_t ver_res;
    struct {
        uint8_t mirror_x        : 1;
        uint8_t mirror_y        : 1;
        uint8_t color_use_bgr   : 1;
        uint8_t color_use_18bit : 1;
        uint8_t use_inv         : 1;  // send ST7789_INVON during init (panel-dependent)
    } flags;
} St7789_config;

typedef struct St7789_t {
    St7789_config config;
    St7789_flush_done_cb flush_done_cb;
    void* flush_done_cb_arg;
} St7789;

St7789* St7789_Create(const St7789_config* config);

void St7789_Reset(St7789* obj);
void St7789_Run_Init_Sequence(St7789* obj);
void St7789_Init(St7789* obj);
void St7789_Set_Backlight(St7789* obj, uint8_t on);

void St7789_Send_Cmd(
    St7789* obj, const uint8_t* cmd, uint32_t cmd_len, const uint8_t* params, uint32_t params_len);
void St7789_Send_Color(
    St7789* obj, const uint8_t* cmd, uint32_t cmd_len, uint8_t* pixels, uint32_t pixels_len);
void St7789_Begin_Write(St7789* obj, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void St7789_Write_Pixels(St7789* obj, uint8_t* pixels, uint32_t pixels_len);
void St7789_End_Write(St7789* obj);
void St7789_Flush(
    St7789* obj, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t* px_map, uint32_t px_size);

void St7789_Register_Flush_Done_Cb(St7789* obj, St7789_flush_done_cb cb, void* arg);
