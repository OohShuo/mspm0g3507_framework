#pragma once

#include <stdint.h>

#include "devices/msp/peripherals/hw_spi.h"

typedef void (*Bsp_spi_tx_dma_done_cb_t)(void* arg);
typedef void (*Bsp_spi_tx_done_cb_t)(void* arg);
typedef void (*Bsp_spi_rx_dma_done_cb_t)(void* arg);
typedef void (*Bsp_spi_idle_cb_t)(void* arg);

void Bsp_Spi_Init(void);

void Bsp_Spi_Write(uint32_t idx, const uint8_t* data, uint32_t len);
void Bsp_Spi_Read(uint32_t idx, uint8_t* data, uint32_t len);

// Blocking variants: start the DMA and busy-wait on the SPI peripheral
// until the transfer is fully done. Pure CPU spin — no FreeRTOS dependency,
// safe to call from pre-scheduler Init.
//
// Use these for back-to-back SPI transactions (e.g. W25Q32) where a new
// DMA setup would otherwise clobber the in-flight one.
void Bsp_Spi_Wait_For_Complete(uint32_t idx);
void Bsp_Spi_Write_Blocking(uint32_t idx, const uint8_t* data, uint32_t len);
void Bsp_Spi_Read_Blocking(uint32_t idx, uint8_t* data, uint32_t len);
void Bsp_Spi_Register_Tx_Dma_Done_Cb(uint32_t idx, Bsp_spi_tx_dma_done_cb_t cb, void* cb_arg);
void Bsp_Spi_Register_Tx_Done_Cb(uint32_t idx, Bsp_spi_tx_done_cb_t cb, void* cb_arg);
void Bsp_Spi_Register_Rx_Dma_Done_Cb(uint32_t idx, Bsp_spi_rx_dma_done_cb_t cb, void* cb_arg);
void Bsp_Spi_Register_Idle_Cb(uint32_t idx, Bsp_spi_idle_cb_t cb, void* cb_arg);

void Bsp_Spi_Irq_Handler(SPI_Regs* spi_inst);