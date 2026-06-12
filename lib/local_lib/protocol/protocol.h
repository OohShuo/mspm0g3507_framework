#pragma once

#include <stdint.h>

#define PROTOCOL_CHUNK_FIRST 0x01
#define PROTOCOL_CHUNK_LAST  0x02

typedef enum {
    protocol_none = 0,
    protocol_7d7e,
    protocol_binary_frame,
} Protocol_type;

typedef void (*Protocol_on_chunk_t)(const uint8_t* data, uint16_t len, uint8_t flags, void* arg);

typedef enum {
    proto_7d7e_rx_state_wait_start,
    proto_7d7e_rx_state_data,
    proto_7d7e_rx_state_escape,
} Proto_7d7e_rx_state;

typedef enum {
    proto_bin_rx_sync0,
    proto_bin_rx_sync1,
    proto_bin_rx_len,
    proto_bin_rx_data,
    proto_bin_rx_crc,
} Proto_bin_rx_state;

// ── Ops table (vtable) ─────────────────────────────────────────────

typedef struct Protocol_t Protocol;

typedef struct {
    void (*send_pack)(Protocol* p, const uint8_t* data, uint16_t len);
    const uint8_t* (*send_get_buf)(const Protocol* p);
    uint16_t (*send_get_len)(const Protocol* p);
    void (*recv_feed)(Protocol* p, const uint8_t* data, uint16_t len);
    void (*destroy)(Protocol* p);
} Protocol_ops;

// ── Unified Protocol struct ────────────────────────────────────────

struct Protocol_t {
    const Protocol_ops* ops;

    // Tx buffer
    uint8_t* tx_buf;
    uint16_t tx_buf_size;
    uint16_t tx_len;

    // Rx buffer (used by 7d7e and binary decoders)
    uint8_t* rx_buf;
    uint16_t rx_buf_size;
    uint16_t rx_len;

    // ── 7d7e Rx state machine ──────────────────────────────────

    Proto_7d7e_rx_state rx_state;
    uint8_t saw_start;
    uint8_t in_frame;

    // ── Binary frame Rx state machine ──────────────────────────

    Proto_bin_rx_state bin_state;
    uint8_t bin_len_buf[2];  // accumulated LEN bytes
    uint8_t bin_len_pos;
    uint16_t bin_data_len;  // parsed LEN value
    uint16_t bin_data_pos;
    uint8_t bin_crc_buf[2];
    uint8_t bin_crc_pos;

    // ── Callback ───────────────────────────────────────────────

    Protocol_on_chunk_t on_chunk;
    void* on_chunk_arg;
};

Protocol* Protocol_Create(Protocol_type type, uint16_t max_payload, Protocol_on_chunk_t on_chunk, void* arg);
void Protocol_Destroy(Protocol* p);
