#pragma once

#include <stddef.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <semphr.h>

typedef struct {
    uint32_t spi_idx;

    uint32_t cs_gpio_idx;  // -1 if not used
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

typedef void (*St7789_flush_done_cb)(void* arg);

typedef enum {
    st7789_tx_state_idle,
    st7789_tx_state_flush,
} St7789_tx_state;

typedef struct St7789_t St7789;

struct St7789_t {
    St7789_config config;

    void (*set_cmd_mode)(St7789* obj, uint8_t is_cmd);
    void (*rst_pulse)(St7789* obj);
    void (*set_bkl)(St7789* obj, uint8_t on);

    St7789_flush_done_cb flush_done_cb;
    void* flush_done_cb_arg;

    SemaphoreHandle_t tx_done_sem;
    St7789_tx_state tx_state;
};

// Allocate the LCD instance and wire the BSP's tx_done ISR to it.
St7789* St7789_Create(const St7789_config* config);

// Run the ST7789 startup sequence (resets, sleep out, color mode, display on).
// Must be called from a task (uses vTaskDelay / xSemaphoreTake).
void St7789_Send_Init_Seq(St7789* obj);

/* Send a short command to the LCD. This function shall wait until the
 * transaction finishes. CS and DC are managed automatically. */
void St7789_Send_Cmd(St7789* obj, const uint8_t* cmd, size_t cmd_size, const uint8_t* param, size_t param_size);

/* Send a large array of pixel data to the LCD. This function does the
 * in-place byte-swap of RGB565 pixels (LE → BE) and can do the transfer
 * in the background. Completion is signaled via the cb registered with
 * St7789_Register_Flush_Done_Cb. */
void St7789_Send_Color(St7789* obj, const uint8_t* cmd, size_t cmd_size, uint8_t* param, size_t param_size);

// Register the completion callback for St7789_Send_Color.
void St7789_Register_Flush_Done_Cb(St7789* obj, St7789_flush_done_cb cb, void* arg);

/* High-level flush for non-LVGL use. Sets the drawing window (CASET + RASET)
 * and writes `px_size` bytes of pixel data at it. Pixel bytes are byte-swapped
 * in place; `px_map` must be writable. The pixel DMA runs asynchronously;
 * completion is signaled via the cb registered with
 * St7789_Register_Flush_Done_Cb. */
void St7789_Flush(St7789* obj, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t* px_map, size_t px_size);
