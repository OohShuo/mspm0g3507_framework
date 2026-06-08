#pragma once

#include <stdint.h>

void Bsp_Soft_Spi_Init(void);

// Bit-bang MSB-first SPI write. Blocking. Caller is responsible for
// driving CS / DC (see the st7789 driver for the typical pattern).
void Bsp_Soft_Spi_Write(uint32_t idx, const uint8_t* data, uint32_t len);
