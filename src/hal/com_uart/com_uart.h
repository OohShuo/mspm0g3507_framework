#pragma once

#include <stdint.h>

#include "general_data.h"

typedef struct Com_uart_t Com_uart;

// User RX notification: called from the bsp idle ISR with a pointer to the
// payload (the bytes after the length-prefix) and its length. The data
// pointer is to the bsp's internal buffer; copy out if you need to keep it.
// Expected not to block.
typedef void (*Com_uart_on_rx_t)(Com_uart* obj, const uint8_t* data, uint32_t len, void* arg);

typedef struct {
    uint32_t uart_idx;          // index into bsp_uart_instances[]

    uint32_t idle_timeout_ms;   // silence threshold for framing (passed to bsp_uart)

    uint32_t rx_max_len;        // max length of received payload (not counting length byte or CRC16); used to size bsp_uart continuous rx buffer
    uint32_t tx_max_len;        // max length of transmitted payload (not counting length byte or CRC16); used to size bsp_uart continuous tx buffer
    Com_uart_on_rx_t on_rx;     // may be NULL
    void* on_rx_arg;
} Com_uart_config;

struct Com_uart_t {
    Com_uart_config config;
    uint8_t* tx_buf;        // [len][data][crc16_lo][crc16_hi]; max payload = 253 B
    uint8_t* rx_buf;        // for continuous reception; same format as tx_buf
    General_data rx_data;          // parsed from rx_buf by on_rx; updated on each idle event
    uint8_t rx_update_flag;       // set to 1 by bsp idle ISR when new data is in rx_buf; user can clear
};

void Com_Uart_Init(void);

Com_uart* Com_Uart_Create(const Com_uart_config* config);

void Com_Uart_Send(Com_uart* obj, const uint8_t* data, uint32_t len);
