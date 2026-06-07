#include "st7789.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <stddef.h>
#include <task.h>

#include "bsp_gpio.h"
#include "bsp_spi.h"
#include "bsp_time.h"

// === ST7789 command set (internal) ===

#define ST7789_NOP      0x00
#define ST7789_SWRESET  0x01
#define ST7789_SLPIN    0x10
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVOFF   0x20
#define ST7789_INVON    0x21
#define ST7789_DISPOFF  0x28
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_MADCTL   0x36
#define ST7789_COLMOD   0x3A
#define ST7789_PORCTRL  0xB2  // Front/back porch
#define ST7789_GCTRL    0xB7  // Gate timing
#define ST7789_VCOMS    0xBB  // VCOM voltage
#define ST7789_LCMCTRL  0xC0  // LCM control
#define ST7789_VDVVRHEN 0xC2  // VDV/VRH enable
#define ST7789_VRHS     0xC3  // VRH set
#define ST7789_VDVS     0xC4  // VDV set
#define ST7789_FRCTRL2  0xC6  // Frame rate ctrl 2
#define ST7789_PWCTRL1  0xD0  // Power ctrl 1
#define ST7789_PVGAMCTRL 0xE0 // Positive gamma
#define ST7789_NVGAMCTRL 0xE1 // Negative gamma

// === Init sequence ===
// Ported from the 地猛星 1.3" 240x240 reference project. The ST7789 needs
// the power/voltage/gamma settings (B2, B7, BB, C0, C2, C3, C4, C6, D0,
// E0, E1) or the panel stays blank. INVON (0x21) is also required for
// this particular panel.

typedef struct {
    uint8_t cmd;
    uint16_t delay_ms;
    uint8_t arg_count;
    uint8_t args[16];
} St7789_init_cmd;

static St7789_init_cmd st7789_default_init_seq[] = {
    {ST7789_SLPIN,   10,  0, {0}},  // sleep in (prep for SWRESET)
    {ST7789_SWRESET, 200, 0, {0}},  // software reset
    {ST7789_SLPOUT,  300, 0, {0}},  // sleep out

    {ST7789_MADCTL,  0,   1, {0x00}},  // overwritten per config.flags
    {ST7789_COLMOD,  10,  1, {0x05}},  // 16 bit/pixel (RGB565)

    {ST7789_PORCTRL, 0,   5, {0x0C, 0x0C, 0x00, 0x33, 0x33}},
    {ST7789_GCTRL,   0,   1, {0x35}},
    {ST7789_VCOMS,   0,   1, {0x37}},
    {ST7789_LCMCTRL, 0,   1, {0x2C}},
    {ST7789_VDVVRHEN, 0,  1, {0x01}},
    {ST7789_VRHS,    0,   1, {0x12}},
    {ST7789_VDVS,    0,   1, {0x20}},
    {ST7789_FRCTRL2, 0,   1, {0x0F}},
    {ST7789_PWCTRL1, 0,   2, {0xA4, 0xA1}},
    {ST7789_PVGAMCTRL, 0, 14, {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}},
    {ST7789_NVGAMCTRL, 0, 14, {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}},

    {ST7789_INVON,   0,   0, {0}},    // display inversion on (required for 1.3" panel)
    {ST7789_DISPON,  200, 0, {0}},    // display on
};

#define ST7789_INIT_SEQ_LEN (sizeof(st7789_default_init_seq) / sizeof(st7789_default_init_seq[0]))

// === Forward decls of default IO impls ===

static void io_set_cmd_mode(St7789* obj, uint8_t is_cmd);
static void io_rst_pulse(St7789* obj);
static void io_set_bkl(St7789* obj, uint8_t on);

// === Low-level helpers ===

static void delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static void cs_low(St7789* obj) {
    if (obj->config.cs_gpio_idx != (uint32_t)-1) {
        Bsp_Gpio_Write(obj->config.cs_gpio_idx, bsp_gpio_state_reset);
    }
}

static void cs_high(St7789* obj) {
    if (obj->config.cs_gpio_idx != (uint32_t)-1) {
        Bsp_Gpio_Write(obj->config.cs_gpio_idx, bsp_gpio_state_set);
    }
}

// Drains any pending semaphore gives left over from a previous transaction.
// No-op in the current design — the cb only gives the semaphore when
// blocking_write is actively waiting, so the semaphore is always empty
// between transactions. Kept for defense in depth.
// static void drain_sem(St7789* obj) { (void)xSemaphoreTake(obj->tx_done_sem, 0); }

// Synchronous SPI write: starts a DMA, blocks (yields via the FreeRTOS semaphore)
// until the BSP's tx_done ISR signals completion. No busy-wait.
//
// Invariant: tx_state must be st7789_tx_state_idle when this is called,
// so the cb takes the idle branch and gives the semaphore.
static void blocking_write(St7789* obj, const uint8_t* data, uint32_t len) {
    Bsp_Spi_Write(obj->config.spi_idx, data, len);
    xSemaphoreTake(obj->tx_done_sem, portMAX_DELAY);
}

// In-place byte-swap of an array of 16-bit pixels (LE → BE for SPI MSB-first).
static void bswap16_inplace(uint8_t* data, size_t byte_count) {
    uint16_t* px = (uint16_t*)data;
    const size_t n = byte_count / 2;
    for (size_t i = 0; i < n; i++) {
        px[i] = (uint16_t)((px[i] << 8) | (px[i] >> 8));
    }
}

// === BSP tx_done callback (ISR context) ===
// `arg` is the St7789 instance (passed via Bsp_Spi_Register_Tx_Done_Cb).
//
// Two branches:
//   - IDLE: a blocking_write is waiting — give the semaphore, return.
//   - FLUSH: this DMA was the fire-and-forget pixel-data from
//            St7789_Send_Color — no one is waiting on the semaphore,
//            so we don't give. Just raise CS, clear the flag, fire the
//            user cb. This is the key invariant: the semaphore is only
//            given when there is a matching take in progress.

static void bsp_tx_done_cb(void* arg) {
    St7789* obj = arg;
    BaseType_t hpw = pdFALSE;

    if (obj->tx_state == st7789_tx_state_flush) {
        cs_high(obj);
        obj->tx_state = st7789_tx_state_idle;
        if (obj->flush_done_cb != NULL) { obj->flush_done_cb(obj->flush_done_cb_arg); }
        return;
    }

    xSemaphoreGiveFromISR(obj->tx_done_sem, &hpw);
    portYIELD_FROM_ISR(hpw);
}

// === Public API ===

St7789* St7789_Create(const St7789_config* config) {
    St7789* obj = pvPortMalloc(sizeof(St7789));
    if (obj == NULL) { return NULL; }

    obj->config = *config;
    obj->set_cmd_mode = io_set_cmd_mode;
    obj->rst_pulse = io_rst_pulse;
    obj->set_bkl = io_set_bkl;
    obj->flush_done_cb = NULL;
    obj->flush_done_cb_arg = NULL;
    obj->tx_state = st7789_tx_state_idle;

    obj->tx_done_sem = xSemaphoreCreateBinary();
    if (obj->tx_done_sem == NULL) {
        vPortFree(obj);
        return NULL;
    }

    Bsp_Spi_Register_Tx_Done_Cb(obj->config.spi_idx, bsp_tx_done_cb, obj);

    return obj;
}

void St7789_Send_Init_Seq(St7789* obj) {
    obj->rst_pulse(obj);

    // Turn backlight on early (matches the reference's flow) so the panel
    // is illuminated by the time display-on fires.
    obj->set_bkl(obj, 1);
    delay_ms(100);

    // Build MADCTL byte from config.flags
    if (obj->config.flags.mirror_x) st7789_default_init_seq[3].args[0] |= 0x40;       // MX
    if (obj->config.flags.mirror_y) st7789_default_init_seq[3].args[0] |= 0x80;       // MY
    if (obj->config.flags.color_use_bgr) st7789_default_init_seq[3].args[0] |= 0x08;  // BGR
    if (obj->config.flags.color_use_18bit)
        st7789_default_init_seq[4].args[0] = 0x06;  // COLMOD: 0x05 = 16-bit, 0x06 = 18-bit (lower 3 bits)

    for (size_t i = 0; i < ST7789_INIT_SEQ_LEN; i++) {
        const St7789_init_cmd* c = &st7789_default_init_seq[i];
        St7789_Send_Cmd(obj, &c->cmd, 1, c->args, c->arg_count);
        if (c->delay_ms > 0) { delay_ms(c->delay_ms); }
    }
}

void St7789_Send_Cmd(St7789* obj, const uint8_t* cmd, size_t cmd_size, const uint8_t* param, size_t param_size) {
    cs_low(obj);

    obj->set_cmd_mode(obj, 0);  // DC = cmd
    blocking_write(obj, cmd, cmd_size);

    if (param_size > 0) {
        obj->set_cmd_mode(obj, 1);  // DC = data
        blocking_write(obj, param, param_size);
    }

    cs_high(obj);
}

void St7789_Send_Color(St7789* obj, const uint8_t* cmd, size_t cmd_size, uint8_t* param, size_t param_size) {
    // In-place byte-swap of RGB565 pixels (LE → BE for SPI MSB-first).
    bswap16_inplace(param, param_size);

    cs_low(obj);

    // Send cmd synchronously (DC = cmd). tx_state is idle so the cb's
    // idle branch will give the semaphore, and blocking_write returns.
    obj->set_cmd_mode(obj, 0);
    blocking_write(obj, cmd, cmd_size);

    // Send pixel data asynchronously (DC = data). Setting tx_state BEFORE
    // starting the DMA ensures the cb's flush branch fires (no give).
    obj->set_cmd_mode(obj, 1);
    obj->tx_state = st7789_tx_state_flush;
    Bsp_Spi_Write(obj->config.spi_idx, param, param_size);
}

void St7789_Register_Flush_Done_Cb(St7789* obj, St7789_flush_done_cb cb, void* arg) {
    obj->flush_done_cb = cb;
    obj->flush_done_cb_arg = arg;
}

// High-level wrapper for non-LVGL use. Sets the drawing window and starts
// the pixel DMA. CS and DC transitions are handled by Send_Cmd / Send_Color.
void St7789_Flush(St7789* obj, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t* px_map, size_t px_size) {
    const uint8_t caset_cmd = 0x2A;
    const uint8_t caset_args[4] = {
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF),
        (uint8_t)(x2 >> 8), (uint8_t)(x2 & 0xFF),
    };
    St7789_Send_Cmd(obj, &caset_cmd, 1, caset_args, 4);

    const uint8_t raset_cmd = 0x2B;
    const uint8_t raset_args[4] = {
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF),
        (uint8_t)(y2 >> 8), (uint8_t)(y2 & 0xFF),
    };
    St7789_Send_Cmd(obj, &raset_cmd, 1, raset_args, 4);

    const uint8_t ramwr_cmd = 0x2C;
    St7789_Send_Color(obj, &ramwr_cmd, 1, px_map, px_size);
}

// === Default IO implementations (BSP-backed) ===

static void io_set_cmd_mode(St7789* obj, uint8_t is_cmd) {
    Bsp_Gpio_Write(obj->config.dc_gpio_idx, is_cmd ? bsp_gpio_state_reset : bsp_gpio_state_set);
}

static void io_rst_pulse(St7789* obj) {
    Bsp_Gpio_Write(obj->config.rst_gpio_idx, bsp_gpio_state_reset);
    delay_ms(100);
    Bsp_Gpio_Write(obj->config.rst_gpio_idx, bsp_gpio_state_set);
    delay_ms(100);
}

static void io_set_bkl(St7789* obj, uint8_t on) {
    Bsp_Gpio_Write(obj->config.bkl_gpio_idx, on ? bsp_gpio_state_set : bsp_gpio_state_reset);
}
