#pragma once

#include <stdint.h>

#include "devices/msp/peripherals/hw_uart.h"

typedef void (*Bsp_uart_tx_dma_done_cb_t)(void* arg);
typedef void (*Bsp_uart_tx_done_cb_t)(void* arg);
typedef void (*Bsp_uart_rx_dma_done_cb_t)(void* arg);

void Bsp_Uart_Init(void);

void Bsp_Uart_Wait_For_Complete(uint32_t idx);

// read / write
void Bsp_Uart_Write(uint32_t idx, const uint8_t* data, uint32_t len);
void Bsp_Uart_Read(uint32_t idx, uint8_t* data, uint32_t len);
void Bsp_Uart_Write_Blocking(uint32_t idx, const uint8_t* data, uint32_t len);
void Bsp_Uart_Read_Blocking(uint32_t idx, uint8_t* data, uint32_t len);

// Continuous RX: after a DMA-DONE-RX fires, the ISR automatically
// re-arms the next reception with the same (data, len). The user
// callback fires on every completion, so the caller can consume the
// buffer between events. Calling Bsp_Uart_Read (or Read_Blocking)
// while continuous RX is active implicitly stops it — a one-shot
// read after Start_Continuous_Rx won't get silently re-armed.
// Bsp_Uart_Stop_Continuous_Rx makes the current in-flight DMA the
// last one; the ISR won't re-arm.
void Bsp_Uart_Start_Continuous_Rx(uint32_t idx, uint8_t* data, uint32_t len);
void Bsp_Uart_Stop_Continuous_Rx(uint32_t idx);

// callback registration
void Bsp_Uart_Register_Tx_Dma_Done_Cb(uint32_t idx, Bsp_uart_tx_dma_done_cb_t cb, void* cb_arg);
void Bsp_Uart_Register_Tx_Done_Cb(uint32_t idx, Bsp_uart_tx_done_cb_t cb, void* cb_arg);
void Bsp_Uart_Register_Rx_Dma_Done_Cb(uint32_t idx, Bsp_uart_rx_dma_done_cb_t cb, void* cb_arg);

void Bsp_Uart_Irq_Handler(UART_Regs* uart_inst);
