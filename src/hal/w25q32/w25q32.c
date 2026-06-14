#include "w25q32.h"

#include <stddef.h>

#include "bsp_gpio.h"
#include "bsp_spi.h"

#define Bsp_Spi_Write Bsp_Hard_Spi_Write_Blocking
#define Bsp_Spi_Read  Bsp_Hard_Spi_Read_Blocking

#define W25Q32_INIT_BUSY_POLL_LIMIT 100000u

static void busy_wait_us(uint32_t us) {
    volatile uint32_t cycles = us * 80;
    while (cycles--) { (void)cycles; }
}

// Low-level helpers

static void cs_low(W25q32* obj) {
    if (obj->config.cs_gpio_idx != (uint32_t)-1) {
        Bsp_Gpio_Write(obj->config.cs_gpio_idx, bsp_gpio_state_reset);
    }
}

static void cs_high(W25q32* obj) {
    if (obj->config.cs_gpio_idx != (uint32_t)-1) {
        Bsp_Gpio_Write(obj->config.cs_gpio_idx, bsp_gpio_state_set);
    }
}

static void send_cmd(W25q32* obj, uint8_t cmd) {
    cs_low(obj);
    Bsp_Spi_Write(obj->config.spi_idx, &cmd, 1);
    cs_high(obj);
}

static void send_cmd_addr(W25q32* obj, uint8_t cmd, uint32_t addr) {
    const uint8_t buf[4] = {
        cmd,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr),
    };
    cs_low(obj);
    Bsp_Spi_Write(obj->config.spi_idx, buf, sizeof(buf));
}

// Public API

// One flash per build (w25q32_test / lfs_test each hold a single
// g_w25q32 / g_flash global, and the demo apps are mutually exclusive
// targets). Backing the handle with a static removes the heap
// dependency that used to return NULL on a fragmented/starved
// FreeRTOS heap — there is now no allocation to fail, and the
// caller-side `if (g_flash == NULL) { return; }` checks degrade to
// pure defensive guards against a NULL config (which none of the
// current call sites ever pass).
static W25q32 s_w25q32_storage;

static uint8_t wait_busy_during_init(W25q32* obj) {
    for (uint32_t i = 0; i < W25Q32_INIT_BUSY_POLL_LIMIT; i++) {
        if ((W25q32_Read_Status_Reg_1(obj) & W25Q32_SR1_BUSY) == 0) { return 1; }
        busy_wait_us(10);
    }
    return 0;
}

W25q32* W25q32_Create(const W25q32_config* config) {
    if (config == NULL) { return NULL; }
    s_w25q32_storage.config = *config;
    s_w25q32_storage.manufacturer_id = 0;
    s_w25q32_storage.memory_type = 0;
    s_w25q32_storage.capacity = 0;
    return &s_w25q32_storage;
}

uint8_t W25q32_Init(W25q32* obj) {
    W25q32_Release_Power_Down(obj);

    W25q32_Reset(obj);

    if (!wait_busy_during_init(obj)) { return 0; }

    uint8_t id3[3] = {0, 0, 0};
    W25q32_Read_Jedec_Id(obj, id3);
    obj->manufacturer_id = id3[0];
    obj->memory_type = id3[1];
    obj->capacity = id3[2];
    if (id3[0] != W25Q32_JEDEC_MFR_WINBOND) { return 0; }

    W25q32_Write_Status_Reg_1(obj, 0x00);
    W25q32_Wait_Busy(obj);
    return 1;
}

void W25q32_Read_Jedec_Id(W25q32* obj, uint8_t* out_id3) {
    const uint8_t cmd = W25Q32_CMD_READ_JEDEC_ID;
    cs_low(obj);
    Bsp_Spi_Write(obj->config.spi_idx, &cmd, 1);
    Bsp_Spi_Read(obj->config.spi_idx, out_id3, 3);
    cs_high(obj);
}

uint8_t W25q32_Read_Status_Reg_1(W25q32* obj) {
    const uint8_t cmd = W25Q32_CMD_READ_STATUS_REG_1;
    uint8_t sr = 0;
    cs_low(obj);
    Bsp_Spi_Write(obj->config.spi_idx, &cmd, 1);
    Bsp_Spi_Read(obj->config.spi_idx, &sr, 1);
    cs_high(obj);
    return sr;
}

void W25q32_Write_Enable(W25q32* obj) { send_cmd(obj, W25Q32_CMD_WRITE_ENABLE); }

void W25q32_Write_Status_Reg_1(W25q32* obj, uint8_t sr1_value) {
    W25q32_Write_Enable(obj);
    cs_low(obj);
    const uint8_t buf[2] = {W25Q32_CMD_WRITE_STATUS_REG_1, sr1_value};
    Bsp_Spi_Write(obj->config.spi_idx, buf, sizeof(buf));
    cs_high(obj);
    W25q32_Wait_Busy(obj);
}

void W25q32_Wait_Busy(W25q32* obj) {
    while (W25q32_Read_Status_Reg_1(obj) & W25Q32_SR1_BUSY) { ; }

    busy_wait_us(1000);
}

void W25q32_Read(W25q32* obj, uint32_t addr, uint8_t* data, uint32_t len) {
    send_cmd_addr(obj, W25Q32_CMD_READ_DATA, addr);
    Bsp_Spi_Read(obj->config.spi_idx, data, len);
    cs_high(obj);
}

void W25q32_Page_Program(W25q32* obj, uint32_t addr, const uint8_t* data, uint32_t len) {
    if (len == 0 || len > 256) { return; }

    W25q32_Write_Enable(obj);
    send_cmd_addr(obj, W25Q32_CMD_PAGE_PROGRAM, addr);
    Bsp_Spi_Write(obj->config.spi_idx, data, len);
    cs_high(obj);
    W25q32_Wait_Busy(obj);
}

void W25q32_Sector_Erase(W25q32* obj, uint32_t addr) {
    W25q32_Write_Enable(obj);
    send_cmd_addr(obj, W25Q32_CMD_SECTOR_ERASE_4K, addr);
    cs_high(obj);
    W25q32_Wait_Busy(obj);
}

void W25q32_Block_Erase_32K(W25q32* obj, uint32_t addr) {
    W25q32_Write_Enable(obj);
    send_cmd_addr(obj, W25Q32_CMD_BLOCK_ERASE_32K, addr);
    cs_high(obj);
    W25q32_Wait_Busy(obj);
}

void W25q32_Block_Erase_64K(W25q32* obj, uint32_t addr) {
    W25q32_Write_Enable(obj);
    send_cmd_addr(obj, W25Q32_CMD_BLOCK_ERASE_64K, addr);
    cs_high(obj);
    W25q32_Wait_Busy(obj);
}

void W25q32_Chip_Erase(W25q32* obj) {
    W25q32_Write_Enable(obj);
    send_cmd(obj, W25Q32_CMD_CHIP_ERASE);
    W25q32_Wait_Busy(obj);
}

void W25q32_Power_Down(W25q32* obj) { send_cmd(obj, W25Q32_CMD_POWER_DOWN); }

void W25q32_Release_Power_Down(W25q32* obj) { send_cmd(obj, W25Q32_CMD_RELEASE_POWER_DOWN); }

void W25q32_Reset(W25q32* obj) {
    send_cmd(obj, W25Q32_CMD_ENABLE_RESET);
    send_cmd(obj, W25Q32_CMD_RESET);
}
