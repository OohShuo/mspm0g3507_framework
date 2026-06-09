#include "st7789.h"

#include <stddef.h>

#include "bsp_gpio.h"
#include "bsp_spi.h"

#define Bsp_Spi_Write Bsp_Soft_Spi_Write

// ~32 cycles/µs at the default 32 MHz CPUCLK (see ti_msp_dl_config.h).
// The init sequence's longest delay is 300 ms; this is a one-time cost
// at boot, so a busy-wait is acceptable (matches w25q32.c style).
static void busy_wait_ms(uint32_t ms) {
    volatile uint32_t cycles = ms * 32U * 1000U;
    while (cycles--) { (void)cycles; }
}

// === Module-private state ===

struct St7789_t {
    St7789_config config;
    St7789_flush_done_cb flush_done_cb;
    void* flush_done_cb_arg;
};

// === Init sequence (from 地猛星 1.3" reference) ===
//
// The power/voltage/gamma block (B2, B7, BB, C0, C2, C3, C4, C6, D0,
// E0, E1) is required or the panel stays blank. INVON (0x21) is also
// required for this particular 1.3" panel.

typedef struct {
    uint8_t cmd;
    uint16_t delay_ms;
    uint8_t arg_count;
    uint8_t args[16];
} St7789_init_cmd;

static St7789_init_cmd st7789_default_init_seq[] = {
    // The hardware reset pulse in St7789_Init is sufficient — the
    // reference (地猛星) goes straight to SLPOUT, no SLPIN/SWRESET.
    {ST7789_SLPOUT, 120, 0, {0}},  // sleep out (datasheet: wait > 120 ms)

    {ST7789_MADCTL, 0, 1, {0x00}},  // overwritten per config.flags
    {ST7789_COLMOD, 0, 1, {ST7789_COLMOD_16BPP}},

    {ST7789_PORCTRL, 0, 5, {0x0C, 0x0C, 0x00, 0x33, 0x33}},
    {ST7789_GCTRL, 0, 1, {0x35}},
    {ST7789_VCOMS, 0, 1, {0x37}},
    {ST7789_LCMCTRL, 0, 1, {0x2C}},
    {ST7789_VDVVRHEN, 0, 1, {0x01}},
    {ST7789_VRHS, 0, 1, {0x12}},
    {ST7789_VDVS, 0, 1, {0x20}},
    {ST7789_FRCTRL2, 0, 1, {0x0F}},
    {ST7789_PWCTRL1, 0, 2, {0xA4, 0xA1}},
    {ST7789_PVGAMCTRL, 0, 14,
        {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}},
    {ST7789_NVGAMCTRL, 0, 14,
        {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}},

    {ST7789_INVON, 0, 0, {0}},     // display inversion on (required for 1.3" panel)
    {ST7789_DISPON, 200, 0, {0}},  // display on
};

#define ST7789_INIT_SEQ_LEN (sizeof(st7789_default_init_seq) / sizeof(st7789_default_init_seq[0]))

// === Low-level helpers ===

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

// ST7789 convention: DC low = command, DC high = data.
static void dc_cmd(St7789* obj) { Bsp_Gpio_Write(obj->config.dc_gpio_idx, bsp_gpio_state_reset); }

static void dc_data(St7789* obj) { Bsp_Gpio_Write(obj->config.dc_gpio_idx, bsp_gpio_state_set); }

// One CS-low/high cycle: command byte, then optional params.
static void send_cmd(
    St7789* obj, const uint8_t* cmd, uint32_t cmd_len, const uint8_t* params, uint32_t params_len) {
    cs_low(obj);
    dc_cmd(obj);
    Bsp_Hard_Spi_Write(obj->config.spi_idx, cmd, cmd_len);
    if (params_len > 0) {
        dc_data(obj);
        Bsp_Hard_Spi_Write(obj->config.spi_idx, params, params_len);
    }
    cs_high(obj);
}

// In-place byte-swap of an array of 16-bit pixels (LE → BE for SPI MSB-first).
static void bswap16_inplace(uint8_t* data, uint32_t byte_count) {
    uint16_t* px = (uint16_t*)data;
    const uint32_t n = byte_count / 2;
    for (uint32_t i = 0; i < n; i++) { px[i] = (uint16_t)((px[i] << 8) | (px[i] >> 8)); }
}

// === Public API ===

// One LCD per build (each app links against its own g_lcd, and the
// demo apps are mutually exclusive targets). Backing the handle with
// a static removes the heap dependency that used to return NULL on a
// fragmented/starved FreeRTOS heap — there is now no allocation to
// fail, and the caller-side `if (g_lcd == NULL) { return; }` checks
// degrade to pure defensive guards against a NULL config (which
// none of the current call sites ever pass).
static St7789 s_lcd_storage;

St7789* St7789_Create(const St7789_config* config) {
    if (config == NULL) { return NULL; }
    s_lcd_storage.config = *config;
    s_lcd_storage.flush_done_cb = NULL;
    s_lcd_storage.flush_done_cb_arg = NULL;
    return &s_lcd_storage;
}

void St7789_Reset(St7789* obj) {
    // Hardware reset pulse.
    Bsp_Gpio_Write(obj->config.rst_gpio_idx, bsp_gpio_state_reset);
    busy_wait_ms(100);
    Bsp_Gpio_Write(obj->config.rst_gpio_idx, bsp_gpio_state_set);
    busy_wait_ms(100);
}

void St7789_Run_Init_Sequence(St7789* obj) {
    // Build MADCTL byte from config.flags.
    if (obj->config.flags.mirror_x) { st7789_default_init_seq[1].args[0] |= ST7789_MADCTL_MX; }
    if (obj->config.flags.mirror_y) { st7789_default_init_seq[1].args[0] |= ST7789_MADCTL_MY; }
    if (obj->config.flags.color_use_bgr) { st7789_default_init_seq[1].args[0] |= ST7789_MADCTL_BGR; }
    if (obj->config.flags.color_use_18bit) { st7789_default_init_seq[2].args[0] = ST7789_COLMOD_18BPP; }

    for (uint32_t i = 0; i < ST7789_INIT_SEQ_LEN; i++) {
        const St7789_init_cmd* c = &st7789_default_init_seq[i];
        send_cmd(obj, &c->cmd, 1, c->args, c->arg_count);
        if (c->delay_ms > 0) { busy_wait_ms(c->delay_ms); }
    }
}

void St7789_Init(St7789* obj) {
    St7789_Reset(obj);
    St7789_Run_Init_Sequence(obj);
}

void St7789_Set_Backlight(St7789* obj, uint8_t on) {
    Bsp_Gpio_Write(obj->config.bkl_gpio_idx, on ? bsp_gpio_state_set : bsp_gpio_state_reset);
}

void St7789_Send_Cmd(
    St7789* obj, const uint8_t* cmd, uint32_t cmd_len, const uint8_t* params, uint32_t params_len) {
    send_cmd(obj, cmd, cmd_len, params, params_len);
}

void St7789_Send_Color(
    St7789* obj, const uint8_t* cmd, uint32_t cmd_len, uint8_t* pixels, uint32_t pixels_len) {
    bswap16_inplace(pixels, pixels_len);
    cs_low(obj);
    dc_cmd(obj);
    Bsp_Hard_Spi_Write(obj->config.spi_idx, cmd, cmd_len);
    dc_data(obj);
    Bsp_Hard_Spi_Write(obj->config.spi_idx, pixels, pixels_len);
    cs_high(obj);
}

void St7789_Flush(
    St7789* obj, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t* px_map, uint32_t px_size) {
    const uint8_t caset_cmd = ST7789_CASET;
    const uint8_t caset_args[4] = {
        (uint8_t)(x1 >> 8),
        (uint8_t)(x1 & 0xFF),
        (uint8_t)(x2 >> 8),
        (uint8_t)(x2 & 0xFF),
    };
    send_cmd(obj, &caset_cmd, 1, caset_args, 4);

    const uint8_t raset_cmd = ST7789_RASET;
    const uint8_t raset_args[4] = {
        (uint8_t)(y1 >> 8),
        (uint8_t)(y1 & 0xFF),
        (uint8_t)(y2 >> 8),
        (uint8_t)(y2 & 0xFF),
    };
    send_cmd(obj, &raset_cmd, 1, raset_args, 4);

    const uint8_t ramwr_cmd = ST7789_RAMWR;
    St7789_Send_Color(obj, &ramwr_cmd, 1, px_map, px_size);

    // CS is high, SPI idle — safe to notify the caller.
    if (obj->flush_done_cb != NULL) { obj->flush_done_cb(obj->flush_done_cb_arg); }
}

void St7789_Register_Flush_Done_Cb(St7789* obj, St7789_flush_done_cb cb, void* arg) {
    obj->flush_done_cb = cb;
    obj->flush_done_cb_arg = arg;
}
