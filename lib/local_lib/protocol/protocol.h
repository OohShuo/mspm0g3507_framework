#pragma once

#include <stdint.h>

#define PROTOCOL_CHUNK_FIRST 0x01
#define PROTOCOL_CHUNK_LAST  0x02

typedef enum {
    protocol_none = 0,
    protocol_7d7e,
} Protocol_type;

// ── Tx side ────────────────────────────────────────────────────
typedef struct Protocol_send_t {
    uint8_t* txbuf;
    uint16_t buf_size;
    uint16_t tx_len;
} Protocol_send;

Protocol_send* Protocol_Send_Create(uint16_t max_payload);
void Protocol_Send_Delete(Protocol_send* s);
void Protocol_Send_Pack(Protocol_send* s, const uint8_t* data, uint16_t len);
const uint8_t* Protocol_Send_Get_Buf(const Protocol_send* s);
uint16_t Protocol_Send_Get_Len(const Protocol_send* s);

// ── Rx side ────────────────────────────────────────────────────
typedef void (*Protocol_on_chunk_t)(const uint8_t* data, uint16_t len, uint8_t flags, void* arg);

typedef enum {
    proto_7d7e_rx_state_wait_start,
    proto_7d7e_rx_state_data,
    proto_7d7e_rx_state_escape,
} Proto_7d7e_rx_state;

typedef struct Protocol_recv_t {
    uint8_t* decode_buf;
    uint16_t buf_size;
    uint16_t decoded_len;
    Proto_7d7e_rx_state state;
    uint8_t saw_start;
    uint8_t in_frame;

    Protocol_on_chunk_t on_chunk;
    void* on_chunk_arg;
} Protocol_recv;

Protocol_recv* Protocol_Recv_Create(uint16_t max_payload, Protocol_on_chunk_t on_chunk, void* arg);
void Protocol_Recv_Delete(Protocol_recv* r);
void Protocol_Recv_Feed(Protocol_recv* r, const uint8_t* data, uint16_t len);
