#include "w25q32.h"

#include <stddef.h>
#include <stdlib.h>

#include "bsp_gpio.h"
#include "bsp_spi.h"

// W25Q32 protocol: every command-then-data transaction is strictly
// sequential. The non-blocking Bsp_Spi_Write/Read would let the next call
// re-arm the DMA before the previous transfer finished, corrupting both
// transfers. Alias to the blocking variants so every Bsp_Spi_* call below
// is implicitly serialized.
#define Bsp_Spi_Write  Bsp_Spi_Write_Blocking
#define Bsp_Spi_Read   Bsp_Spi_Read_Blocking

// CPU busy-wait in microseconds. Used for the post-BUSY settle so the
// chip has time to wrap up internal housekeeping. At 32MHz, 1us ≈ 32
// loop iterations. The volatile read of the counter also prevents the
// compiler from optimizing the loop away.
//
// This is preferred over vTaskDelay because vTaskDelay is unsafe to call
// before the FreeRTOS scheduler starts, and W25q32_Init runs pre-scheduler.
static void busy_wait_us(uint32_t us) {
    volatile uint32_t cycles = us * 32;
    while (cycles--) {
        (void)cycles;
    }
}

// === Low-level helpers ===

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

// Send a single command byte. No address, no data.
static void send_cmd(W25q32* obj, uint8_t cmd) {
    cs_low(obj);
    Bsp_Spi_Write(obj->config.spi_idx, &cmd, 1);
    cs_high(obj);
}

// Send cmd + 3-byte big-endian address. Leaves CS low so the caller can
// immediately follow with Bsp_Spi_Read or Bsp_Spi_Write for the data phase.
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

// === Public API ===

W25q32* W25q32_Create(const W25q32_config* config) {
    W25q32* obj = (W25q32*)malloc(sizeof(W25q32));
    if (obj == NULL) { return NULL; }
    obj->config = *config;
    obj->manufacturer_id = 0;
    obj->memory_type = 0;
    obj->capacity = 0;
    return obj;
}

bool W25q32_Init(W25q32* obj) {
    // If the chip is in power-down (e.g. from a previous run on a board
    // with no power-cycle between boots), wake it up first.
    W25q32_Release_Power_Down(obj);

    // Software reset makes the state deterministic regardless of what
    // the chip was doing before.
    W25q32_Reset(obj);

    // The reset takes tRST (~30us) to converge. Without this wait, the
    // first SR1 read in the test loop can see BUSY=1 (initially flaky).
    W25q32_Wait_Busy(obj);

    // Force-clear block protect bits in SR1. Some factory-fresh or
    // pre-owned flash chips have BP0/BP1/BP2 set, which silently
    // disables erase/program on the protected region (top of array).
    W25q32_Write_Status_Reg_1(obj, 0x00);
    W25q32_Wait_Busy(obj);

    uint8_t id3[3] = {0, 0, 0};
    W25q32_Read_Jedec_Id(obj, id3);
    obj->manufacturer_id = id3[0];
    obj->memory_type = id3[1];
    obj->capacity = id3[2];
    return (id3[0] == W25Q32_JEDEC_MFR_WINBOND);
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

void W25q32_Write_Enable(W25q32* obj) {
    send_cmd(obj, W25Q32_CMD_WRITE_ENABLE);
}

void W25q32_Write_Status_Reg_1(W25q32* obj, uint8_t sr1_value) {
    W25q32_Write_Enable(obj);
    cs_low(obj);
    const uint8_t buf[2] = {W25Q32_CMD_WRITE_STATUS_REG_1, sr1_value};
    Bsp_Spi_Write(obj->config.spi_idx, buf, sizeof(buf));
    cs_high(obj);
    W25q32_Wait_Busy(obj);
}

void W25q32_Wait_Busy(W25q32* obj) {
    // Typical sector erase: 30-200 ms. Page program: 0.5-3 ms. Chip erase: 15-60 s.
    // Polling at this rate (~10 kHz of SR reads) is fine for these timescales.
    while (W25q32_Read_Status_Reg_1(obj) & W25Q32_SR1_BUSY) {
        // spin
    }
    // ~1 ms settle: even after BUSY clears, the chip needs a beat for
    // internal housekeeping. Skipping this and starting the next op
    // immediately can cause the next op to silently fail.
    // Use a CPU busy-wait (not vTaskDelay) so this is safe to call from
    // W25q32_Init before the FreeRTOS scheduler starts.
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

void W25q32_Power_Down(W25q32* obj) {
    send_cmd(obj, W25Q32_CMD_POWER_DOWN);
}

void W25q32_Release_Power_Down(W25q32* obj) {
    send_cmd(obj, W25Q32_CMD_RELEASE_POWER_DOWN);
}

void W25q32_Reset(W25q32* obj) {
    send_cmd(obj, W25Q32_CMD_ENABLE_RESET);
    send_cmd(obj, W25Q32_CMD_RESET);
}
