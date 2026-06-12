#include "protocol.h"

#include <string.h>

#include "freertos_alloc.h"

#define SLIP_START     0x7F
#define SLIP_END       0x7E
#define SLIP_ESC       0x7D
#define SLIP_ESC_START 0x02
#define SLIP_ESC_END   0x01
#define SLIP_ESC_ESC   0x00

// protocol_none  ops  (pass-through, DMA-safe copy)

static void none_send_pack(Protocol* p, const uint8_t* data, uint16_t len) {
    if (p == NULL || data == NULL) { return; }
    uint16_t n = (len <= p->tx_buf_size) ? len : p->tx_buf_size;
    memcpy(p->tx_buf, data, n);
    p->tx_len = n;
}

static const uint8_t* none_send_get_buf(const Protocol* p) { return (p != NULL) ? p->tx_buf : NULL; }
static uint16_t none_send_get_len(const Protocol* p) { return (p != NULL) ? p->tx_len : 0; }

static void none_recv_feed(Protocol* p, const uint8_t* data, uint16_t len) {
    if (p == NULL || data == NULL) { return; }
    if (p->on_chunk != NULL) {
        p->on_chunk(data, len, PROTOCOL_CHUNK_FIRST | PROTOCOL_CHUNK_LAST, p->on_chunk_arg);
    }
}

static void none_destroy(Protocol* p) {
    if (p == NULL) { return; }
    vPortFree(p->tx_buf);
    vPortFree(p);
}

static const Protocol_ops g_protocol_none_ops = {
    .send_pack = none_send_pack,
    .send_get_buf = none_send_get_buf,
    .send_get_len = none_send_get_len,
    .recv_feed = none_recv_feed,
    .destroy = none_destroy,
};

// protocol_7d7e  ops

static void slip_send_pack(Protocol* p, const uint8_t* data, uint16_t len) {
    if (p == NULL || data == NULL) { return; }

    uint8_t* w = p->tx_buf;
    uint8_t* end = p->tx_buf + p->tx_buf_size;

    // START marker
    if (w >= end) { return; }
    *w++ = SLIP_START;

    // Escape payload
    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        if (b == SLIP_START) {
            if (w + 1 >= end) { break; }
            *w++ = SLIP_ESC;
            *w++ = SLIP_ESC_START;
        } else if (b == SLIP_END) {
            if (w + 1 >= end) { break; }
            *w++ = SLIP_ESC;
            *w++ = SLIP_ESC_END;
        } else if (b == SLIP_ESC) {
            if (w + 1 >= end) { break; }
            *w++ = SLIP_ESC;
            *w++ = SLIP_ESC_ESC;
        } else {
            if (w >= end) { break; }
            *w++ = b;
        }
    }

    // END marker
    if (w >= end) { return; }
    *w++ = SLIP_END;

    p->tx_len = (uint16_t)(w - p->tx_buf);
}

static const uint8_t* slip_send_get_buf(const Protocol* p) { return (p != NULL) ? p->tx_buf : NULL; }
static uint16_t slip_send_get_len(const Protocol* p) { return (p != NULL) ? p->tx_len : 0; }

static void slip_flush(Protocol* p, uint8_t is_last) {
    if (p->on_chunk == NULL) { return; }

    uint8_t flags = 0;
    if (p->saw_start) { flags |= PROTOCOL_CHUNK_FIRST; }
    if (is_last) { flags |= PROTOCOL_CHUNK_LAST; }

    p->on_chunk(p->rx_buf, p->rx_len, flags, p->on_chunk_arg);

    p->rx_len = 0;
    p->saw_start = 0;
}

static void slip_recv_feed(Protocol* p, const uint8_t* data, uint16_t len) {
    if (p == NULL || data == NULL) { return; }

    uint8_t flushed = 0;

    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = data[i];

        switch (p->rx_state) {
            case proto_7d7e_rx_state_wait_start:
                if (b == SLIP_START) {
                    p->rx_state = proto_7d7e_rx_state_data;
                    p->saw_start = 1;
                    p->in_frame = 1;
                }
                break;

            case proto_7d7e_rx_state_data:
                if (b == SLIP_END) {
                    p->rx_state = proto_7d7e_rx_state_wait_start;
                    p->in_frame = 0;
                    slip_flush(p, 1);
                    flushed = 1;
                } else if (b == SLIP_ESC) {
                    p->rx_state = proto_7d7e_rx_state_escape;
                } else {
                    if (p->rx_len < p->rx_buf_size) { p->rx_buf[p->rx_len++] = b; }
                }
                break;

            case proto_7d7e_rx_state_escape:
                if (p->rx_len < p->rx_buf_size) {
                    if (b == SLIP_ESC_END) {
                        p->rx_buf[p->rx_len++] = SLIP_END;
                    } else if (b == SLIP_ESC_ESC) {
                        p->rx_buf[p->rx_len++] = SLIP_ESC;
                    } else if (b == SLIP_ESC_START) {
                        p->rx_buf[p->rx_len++] = SLIP_START;
                    } else {
                        p->rx_buf[p->rx_len++] = b;
                    }
                }
                p->rx_state = proto_7d7e_rx_state_data;
                break;
        }
    }

    if (!flushed && p->in_frame && (p->rx_len > 0 || p->saw_start)) { slip_flush(p, 0); }
}

static void slip_destroy(Protocol* p) {
    if (p == NULL) { return; }
    vPortFree(p->tx_buf);
    vPortFree(p->rx_buf);
    vPortFree(p);
}

static const Protocol_ops g_protocol_7d7e_ops = {
    .send_pack = slip_send_pack,
    .send_get_buf = slip_send_get_buf,
    .send_get_len = slip_send_get_len,
    .recv_feed = slip_recv_feed,
    .destroy = slip_destroy,
};

// Public API

Protocol* Protocol_Create(Protocol_type type, uint16_t max_payload, Protocol_on_chunk_t on_chunk, void* arg) {
    Protocol* p = (Protocol*)pvPortMalloc(sizeof(Protocol));
    if (p == NULL) { return NULL; }
    memset(p, 0, sizeof(Protocol));

    switch (type) {
        case protocol_none:
            p->ops = &g_protocol_none_ops;
            p->tx_buf_size = max_payload;
            p->tx_buf = (uint8_t*)pvPortMalloc(p->tx_buf_size);
            if (p->tx_buf == NULL) {
                vPortFree(p);
                return NULL;
            }
            break;

        case protocol_7d7e:
            p->ops = &g_protocol_7d7e_ops;
            // tx_buf sized for worst-case SLIP escaping
            p->tx_buf_size = 2 * max_payload + 2;
            p->tx_buf = (uint8_t*)pvPortMalloc(p->tx_buf_size);
            if (p->tx_buf == NULL) {
                vPortFree(p);
                return NULL;
            }
            p->rx_buf_size = max_payload;
            p->rx_buf = (uint8_t*)pvPortMalloc(p->rx_buf_size);
            if (p->rx_buf == NULL) {
                vPortFree(p->tx_buf);
                vPortFree(p);
                return NULL;
            }
            break;

        default:
            vPortFree(p);
            return NULL;
    }

    p->on_chunk = on_chunk;
    p->on_chunk_arg = arg;
    return p;
}

void Protocol_Destroy(Protocol* p) {
    if (p == NULL) { return; }
    if (p->ops != NULL && p->ops->destroy != NULL) { p->ops->destroy(p); }
}
