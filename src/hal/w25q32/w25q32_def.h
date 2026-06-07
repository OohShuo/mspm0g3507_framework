#pragma once

// === W25Q32 standard command set ===

#define W25Q32_CMD_WRITE_ENABLE       0x06
#define W25Q32_CMD_WRITE_DISABLE      0x04
#define W25Q32_CMD_READ_STATUS_REG_1  0x05
#define W25Q32_CMD_READ_STATUS_REG_2  0x35
#define W25Q32_CMD_READ_STATUS_REG_3  0x15
#define W25Q32_CMD_WRITE_STATUS_REG_1 0x01
#define W25Q32_CMD_PAGE_PROGRAM       0x02
#define W25Q32_CMD_READ_DATA          0x03
#define W25Q32_CMD_READ_STATUS_REG    0x05
#define W25Q32_CMD_READ_JEDEC_ID      0x9F
#define W25Q32_CMD_SECTOR_ERASE_4K    0x20
#define W25Q32_CMD_BLOCK_ERASE_32K    0x52
#define W25Q32_CMD_BLOCK_ERASE_64K    0xD8
#define W25Q32_CMD_CHIP_ERASE         0xC7
#define W25Q32_CMD_POWER_DOWN         0xB9
#define W25Q32_CMD_RELEASE_POWER_DOWN 0xAB
#define W25Q32_CMD_READ_UNIQUE_ID     0x4B
#define W25Q32_CMD_READ_SFDP          0x5A
#define W25Q32_CMD_ENABLE_RESET       0x66
#define W25Q32_CMD_RESET              0x99

// === Status Register 1 bits ===

#define W25Q32_SR1_BUSY               0x01
#define W25Q32_SR1_WEL                0x02
#define W25Q32_SR1_BP0                0x04
#define W25Q32_SR1_BP1                0x08
#define W25Q32_SR1_BP2                0x10
#define W25Q32_SR1_TB                 0x20
#define W25Q32_SR1_SEC                0x40
#define W25Q32_SR1_SRP                0x80

// === Expected JEDEC ID values ===

#define W25Q32_JEDEC_MFR_WINBOND      0xEF
#define W25Q32_JEDEC_TYPE_W25Q32      0x40  // W25Q32 family
#define W25Q32_JEDEC_CAP_4MBIT        0x15  // 2^21 bytes / 2^? — Winbond convention
