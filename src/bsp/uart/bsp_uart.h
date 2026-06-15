#pragma once

#include <stdint.h>

#include "devices/msp/peripherals/hw_uart.h"

#define BSP_UART_CONTINUOUS_RX_BUF_SIZE 64

typedef void (*Bsp_uart_tx_dma_done_cb_t)(void* arg);
typedef void (*Bsp_uart_tx_done_cb_t)(void* arg);
typedef void (*Bsp_uart_rx_dma_done_cb_t)(void* arg);
typedef void (*Bsp_uart_rx_idle_cb_t)(uint32_t idx, uint8_t* data, uint32_t len, void* arg);

void Bsp_Uart_Init(void);

void Bsp_Uart_Wait_For_Complete(uint32_t idx);

// read / write
void Bsp_Uart_Write(uint32_t idx, const uint8_t* data, uint32_t len);
void Bsp_Uart_Read(uint32_t idx, uint8_t* data, uint32_t len);
void Bsp_Uart_Write_Blocking(uint32_t idx, const uint8_t* data, uint32_t len);
void Bsp_Uart_Read_Blocking(uint32_t idx, uint8_t* data, uint32_t len);
void Bsp_Uart_Start_Continuous_Rx(uint32_t idx, uint32_t idle_timeout_ms, uint8_t* buf, uint32_t max_len);
void Bsp_Uart_Stop_Continuous_Rx(uint32_t idx);

// callback registration
void Bsp_Uart_Register_Tx_Dma_Done_Cb(uint32_t idx, Bsp_uart_tx_dma_done_cb_t cb, void* cb_arg);
void Bsp_Uart_Register_Tx_Done_Cb(uint32_t idx, Bsp_uart_tx_done_cb_t cb, void* cb_arg);
void Bsp_Uart_Register_Rx_Dma_Done_Cb(uint32_t idx, Bsp_uart_rx_dma_done_cb_t cb, void* cb_arg);
void Bsp_Uart_Register_Rx_Idle_Cb(uint32_t idx, Bsp_uart_rx_idle_cb_t cb, void* cb_arg);

void Bsp_Uart_Irq_Handler(UART_Regs* uart_inst);
void Bsp_Uart_Idle_Irq_Handler(uint32_t idx);
