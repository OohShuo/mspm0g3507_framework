#pragma once

#include <stdint.h>

#include "general_data.h"

typedef struct Com_uart_t Com_uart;

typedef void (*Com_uart_on_rx_t)(Com_uart* obj, void* arg);

typedef struct {
    uint32_t uart_idx;
    uint8_t data_len;
    uint32_t idle_timeout_ms;
    Com_uart_on_rx_t on_rx;
    void* on_rx_arg;
} Com_uart_config;

struct Com_uart_t {
    Com_uart_config config;
    General_data data_rx;
    uint8_t* tx_buf;
};

void Com_Uart_Init(void);

Com_uart* Com_Uart_Create(const Com_uart_config* config);

void Com_Uart_Send(Com_uart* obj, const uint8_t* data);
