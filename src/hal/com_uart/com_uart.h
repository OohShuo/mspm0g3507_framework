#pragma once

#include <stdint.h>

#include "protocol.h"

typedef struct Com_uart_t Com_uart;
typedef void (*Com_uart_on_rx_t)(Com_uart* obj, const uint8_t* data, uint32_t len, uint8_t flags, void* arg);

typedef struct {
    uint32_t uart_idx;

    uint32_t idle_timeout_ms;
    uint32_t rx_max_len;
    uint32_t tx_max_len;

    Protocol_type protocol_type;
    uint16_t protocol_max_payload;

    Com_uart_on_rx_t on_rx;
    void* on_rx_arg;
} Com_uart_config;

struct Com_uart_t {
    Com_uart_config config;
    uint8_t* rx_buf;

    Protocol* proto;
};

void Com_Uart_Init(void);

Com_uart* Com_Uart_Create(const Com_uart_config* config);
void Com_Uart_Send(Com_uart* obj, const uint8_t* data, uint32_t len);
