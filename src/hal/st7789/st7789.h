#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "st7789_def.h"

typedef struct {
    uint32_t spi_idx;
    uint32_t cs_gpio_idx;  // (uint32_t)-1 if hardwired on the board
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
    } flags;
} St7789_config;

typedef struct St7789_t St7789;

// Fires from task context at the end of St7789_Flush, after CS has
// gone high and the pixel data has been clocked out. Use it to start
// preparing the next frame's buffer, signal a waiter, etc.
typedef void (*St7789_flush_done_cb)(void* arg);

St7789* St7789_Create(const St7789_config* config);

// Hardware reset pulse on RST (low 100ms, high 100ms). Use when you
// want to give the panel a clean boot without re-running the full init
// sequence (e.g. before handing control to a driver that runs its own
// init, like lv_st7789).
void St7789_Reset(St7789* obj);

// Run the ST7789 startup sequence (SLPOUT / MADCTL / COLMOD / PORCTRL /
// GCTRL / VCOMS / LCMCTRL / VDVVRHEN / VRHS / VDVS / FRCTRL2 / PWCTRL1 /
// PVGAMCTRL / NVGAMCTRL / INVON / DISPON). Blocks for ~720 ms total
// (busy-wait, no FreeRTOS involvement). Does NOT pulse the RST line —
// call St7789_Reset() first if the panel is in an unknown state.
void St7789_Run_Init_Sequence(St7789* obj);

// Reset + run the ST7789 startup sequence. Convenience wrapper for
// apps that drive the panel directly (lcd_test). Blocks for ~720 ms
// (busy-wait, no FreeRTOS involvement). Backlight is not touched.
void St7789_Init(St7789* obj);

void St7789_Set_Backlight(St7789* obj, uint8_t on);

// One CS-low/high cycle: command byte, then optional params.
void St7789_Send_Cmd(
    St7789* obj, const uint8_t* cmd, uint32_t cmd_len, const uint8_t* params, uint32_t params_len);

// One CS-low/high cycle: command byte, then pixel payload. `pixels` is
// byte-swapped in place (LE → BE for RGB565 MSB-first SPI), so the
// buffer must be writable. Caller retains ownership.
void St7789_Send_Color(
    St7789* obj, const uint8_t* cmd, uint32_t cmd_len, uint8_t* pixels, uint32_t pixels_len);

// Set the drawing window (CASET + RASET) and write `px_size` bytes of
// pixel data. Equivalent to Send_Cmd(CASET) + Send_Cmd(RASET) +
// Send_Color(RAMWR, px_map), with CS held across all three.
void St7789_Flush(
    St7789* obj, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t* px_map, uint32_t px_size);

// Register a callback that fires at the end of each St7789_Flush. Pass
// NULL to unregister. The callback runs in task context; do not block
// or call any St7789_* API from it.
void St7789_Register_Flush_Done_Cb(St7789* obj, St7789_flush_done_cb cb, void* arg);
