#pragma once

#include <stdint.h>

void Bsp_Soft_Spi_Init(void);

void Bsp_Soft_Spi_Write(uint32_t idx, const uint8_t* data, uint32_t len);
void Bsp_Soft_Spi_Write_Swapped16(uint32_t idx, const uint8_t* data, uint32_t len);
