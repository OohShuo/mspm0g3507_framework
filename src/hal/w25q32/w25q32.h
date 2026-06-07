#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t spi_idx;
    uint32_t cs_gpio_idx;  // -1 if hardwired on the board
} W25q32_config;

// === W25Q32 standard command set ===

#define W25Q32_CMD_WRITE_ENABLE          0x06
#define W25Q32_CMD_WRITE_DISABLE         0x04
#define W25Q32_CMD_READ_STATUS_REG_1     0x05
#define W25Q32_CMD_READ_STATUS_REG_2     0x35
#define W25Q32_CMD_READ_STATUS_REG_3     0x15
#define W25Q32_CMD_WRITE_STATUS_REG_1    0x01
#define W25Q32_CMD_PAGE_PROGRAM          0x02
#define W25Q32_CMD_READ_DATA             0x03
#define W25Q32_CMD_READ_STATUS_REG       0x05
#define W25Q32_CMD_READ_JEDEC_ID         0x9F
#define W25Q32_CMD_SECTOR_ERASE_4K      0x20
#define W25Q32_CMD_BLOCK_ERASE_32K      0x52
#define W25Q32_CMD_BLOCK_ERASE_64K      0xD8
#define W25Q32_CMD_CHIP_ERASE           0xC7
#define W25Q32_CMD_POWER_DOWN            0xB9
#define W25Q32_CMD_RELEASE_POWER_DOWN   0xAB
#define W25Q32_CMD_READ_UNIQUE_ID       0x4B
#define W25Q32_CMD_READ_SFDP            0x5A
#define W25Q32_CMD_ENABLE_RESET         0x66
#define W25Q32_CMD_RESET                0x99

// === Status Register 1 bits ===

#define W25Q32_SR1_BUSY  0x01
#define W25Q32_SR1_WEL   0x02
#define W25Q32_SR1_BP0   0x04
#define W25Q32_SR1_BP1   0x08
#define W25Q32_SR1_BP2   0x10
#define W25Q32_SR1_TB    0x20
#define W25Q32_SR1_SEC   0x40
#define W25Q32_SR1_SRP   0x80

// === Expected JEDEC ID values ===

#define W25Q32_JEDEC_MFR_WINBOND   0xEF
#define W25Q32_JEDEC_TYPE_W25Q32   0x40  // W25Q32 family
#define W25Q32_JEDEC_CAP_4MBIT     0x15  // 2^21 bytes / 2^? — Winbond convention

typedef struct W25q32_t W25q32;

struct W25q32_t {
    W25q32_config config;
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity;
};

// Allocate the W25Q32 instance. No hardware interaction yet.
W25q32* W25q32_Create(const W25q32_config* config);

// Release power-down and read JEDEC ID. Cached in the struct.
// Returns true if a Winbond flash responded (manufacturer_id == 0xEF).
bool W25q32_Init(W25q32* obj);

// === Identification ===

// Read JEDEC ID on demand. out_id3[0]=manufacturer, [1]=mem type, [2]=capacity.
void W25q32_Read_Jedec_Id(W25q32* obj, uint8_t* out_id3);

// Read Status Register 1 (SR1). BUSY bit (0x01) and WEL bit (0x02) live here.
uint8_t W25q32_Read_Status_Reg_1(W25q32* obj);

// === Write-protect / busy-wait ===

// Set the Write Enable Latch. Must be called before every Page Program /
// Sector Erase / Block Erase / Chip Erase / Write Status Register.
void W25q32_Write_Enable(W25q32* obj);

// Write Status Register 1. Affects BP0/BP1/BP2 (block protect), SEC, TB, SRP.
// Write Enable must be set first (handled internally).
void W25q32_Write_Status_Reg_1(W25q32* obj, uint8_t sr1_value);

// Block until the BUSY bit in SR1 clears. Use after any erase or program op.
void W25q32_Wait_Busy(W25q32* obj);

// === Read / write ===

// Read `len` bytes from `addr` into `data`. Length may span page boundaries.
void W25q32_Read(W25q32* obj, uint32_t addr, uint8_t* data, uint32_t len);

// Page Program: write 1..256 bytes. The flash must be erased (0xFF) first.
// If len > 256 or (addr + len) crosses a 256-byte page boundary, the wrap-around
// bytes are written at the start of the same page. Caller is responsible for
// splitting the write at page boundaries.
void W25q32_Page_Program(W25q32* obj, uint32_t addr, const uint8_t* data, uint32_t len);

// === Erase ===

// 4KB sector erase. `addr` should be 4KB-aligned.
void W25q32_Sector_Erase(W25q32* obj, uint32_t addr);

// 32KB block erase. `addr` should be 32KB-aligned.
void W25q32_Block_Erase_32K(W25q32* obj, uint32_t addr);

// 64KB block erase. `addr` should be 64KB-aligned.
void W25q32_Block_Erase_64K(W25q32* obj, uint32_t addr);

// Erase the entire chip. Takes seconds.
void W25q32_Chip_Erase(W25q32* obj);

// === Power ===

// Enter low-power mode. CS pulse wakes the device.
void W25q32_Power_Down(W25q32* obj);

// Exit power-down. After this, normal commands work again.
void W25q32_Release_Power_Down(W25q32* obj);

// Software reset (after which the device takes ~30us to be ready).
void W25q32_Reset(W25q32* obj);
