#include "protocol.h"

#include <string.h>

#include "freertos_alloc.h"

#define SLIP_START     0x7F
#define SLIP_END       0x7E
#define SLIP_ESC       0x7D
#define SLIP_ESC_START 0x02
#define SLIP_ESC_END   0x01
#define SLIP_ESC_ESC   0x00

Protocol_send* Protocol_Send_Create(uint16_t max_payload) {
    Protocol_send* s = (Protocol_send*)pvPortMalloc(sizeof(Protocol_send));
    if (s == NULL) { return NULL; }

    s->buf_size = 2 * max_payload + 2;
    s->txbuf = (uint8_t*)pvPortMalloc(s->buf_size);
    if (s->txbuf == NULL) {
        vPortFree(s);
        return NULL;
    }
    s->tx_len = 0;
    return s;
}

void Protocol_Send_Delete(Protocol_send* s) {
    if (s == NULL) { return; }
    vPortFree(s->txbuf);
    vPortFree(s);
}

void Protocol_Send_Pack(Protocol_send* s, const uint8_t* data, uint16_t len) {
    if (s == NULL || data == NULL) { return; }

    uint8_t* p = s->txbuf;

    // START marker
    *p++ = SLIP_START;

    // Escape payload
    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        if (b == SLIP_START) {
            *p++ = SLIP_ESC;
            *p++ = SLIP_ESC_START;
        } else if (b == SLIP_END) {
            *p++ = SLIP_ESC;
            *p++ = SLIP_ESC_END;
        } else if (b == SLIP_ESC) {
            *p++ = SLIP_ESC;
            *p++ = SLIP_ESC_ESC;
        } else {
            *p++ = b;
        }
    }

    *p++ = SLIP_END;

    s->tx_len = (uint16_t)(p - s->txbuf);
}

const uint8_t* Protocol_Send_Get_Buf(const Protocol_send* s) { return (s != NULL) ? s->txbuf : NULL; }

uint16_t Protocol_Send_Get_Len(const Protocol_send* s) { return (s != NULL) ? s->tx_len : 0; }

// ── Receive side ────────────────────────────────────────────────────
Protocol_recv* Protocol_Recv_Create(uint16_t max_payload, Protocol_on_chunk_t on_chunk, void* arg) {
    Protocol_recv* r = (Protocol_recv*)pvPortMalloc(sizeof(Protocol_recv));
    if (r == NULL) { return NULL; }

    r->buf_size = max_payload;
    r->decode_buf = (uint8_t*)pvPortMalloc(max_payload);
    if (r->decode_buf == NULL) {
        vPortFree(r);
        return NULL;
    }

    r->decoded_len = 0;
    r->state = proto_7d7e_rx_state_wait_start;
    r->saw_start = 0;
    r->in_frame = 0;
    r->on_chunk = on_chunk;
    r->on_chunk_arg = arg;
    return r;
}

void Protocol_Recv_Delete(Protocol_recv* r) {
    if (r == NULL) { return; }
    vPortFree(r->decode_buf);
    vPortFree(r);
}

static void protocol_flush(Protocol_recv* r, uint8_t is_last) {
    if (r->on_chunk == NULL) { return; }

    uint8_t flags = 0;
    if (r->saw_start) { flags |= PROTOCOL_CHUNK_FIRST; }
    if (is_last) { flags |= PROTOCOL_CHUNK_LAST; }

    r->on_chunk(r->decode_buf, r->decoded_len, flags, r->on_chunk_arg);

    r->decoded_len = 0;
    r->saw_start = 0;
}

void Protocol_Recv_Feed(Protocol_recv* r, const uint8_t* data, uint16_t len) {
    if (r == NULL || data == NULL) { return; }

    uint8_t flushed = 0;

    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = data[i];

        switch (r->state) {
            case proto_7d7e_rx_state_wait_start:
                if (b == SLIP_START) {
                    r->state = proto_7d7e_rx_state_data;
                    r->saw_start = 1;
                    r->in_frame = 1;
                }
                break;

            case proto_7d7e_rx_state_data:
                if (b == SLIP_END) {
                    // Frame ends — flush with LAST flag.
                    r->state = proto_7d7e_rx_state_wait_start;
                    r->in_frame = 0;
                    protocol_flush(r, 1);
                    flushed = 1;
                } else if (b == SLIP_ESC) {
                    r->state = proto_7d7e_rx_state_escape;
                } else {
                    if (r->decoded_len < r->buf_size) { r->decode_buf[r->decoded_len++] = b; }
                }
                break;

            case proto_7d7e_rx_state_escape:
                if (r->decoded_len < r->buf_size) {
                    if (b == SLIP_ESC_END) {
                        r->decode_buf[r->decoded_len++] = SLIP_END;
                    } else if (b == SLIP_ESC_ESC) {
                        r->decode_buf[r->decoded_len++] = SLIP_ESC;
                    } else if (b == SLIP_ESC_START) {
                        r->decode_buf[r->decoded_len++] = SLIP_START;
                    } else {
                        r->decode_buf[r->decoded_len++] = b;
                    }
                }
                r->state = proto_7d7e_rx_state_data;
                break;
        }
    }

    if (!flushed && r->in_frame && (r->decoded_len > 0 || r->saw_start)) { protocol_flush(r, 0); }
}
