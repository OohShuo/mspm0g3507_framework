#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w25q32_def.h"

typedef struct {
    uint32_t spi_idx;
    uint32_t cs_gpio_idx;  // -1 if hardwired on the board
} W25q32_config;

typedef struct W25q32_t W25q32;

struct W25q32_t {
    W25q32_config config;
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity;
};

W25q32* W25q32_Create(const W25q32_config* config);

bool W25q32_Init(W25q32* obj);

// identification
void W25q32_Read_Jedec_Id(W25q32* obj, uint8_t* out_id3);
uint8_t W25q32_Read_Status_Reg_1(W25q32* obj);

// Write-protect / busy-wait
void W25q32_Write_Enable(W25q32* obj);
void W25q32_Write_Status_Reg_1(W25q32* obj, uint8_t sr1_value);
void W25q32_Wait_Busy(W25q32* obj);

// Read / write
void W25q32_Read(W25q32* obj, uint32_t addr, uint8_t* data, uint32_t len);
void W25q32_Page_Program(W25q32* obj, uint32_t addr, const uint8_t* data, uint32_t len);

// Erase
void W25q32_Sector_Erase(W25q32* obj, uint32_t addr);
void W25q32_Block_Erase_32K(W25q32* obj, uint32_t addr);
void W25q32_Block_Erase_64K(W25q32* obj, uint32_t addr);
void W25q32_Chip_Erase(W25q32* obj);

// Power
void W25q32_Power_Down(W25q32* obj);
void W25q32_Release_Power_Down(W25q32* obj);
void W25q32_Reset(W25q32* obj);
