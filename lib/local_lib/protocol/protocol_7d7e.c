#include "protocol.h"

#include <string.h>

#include "freertos_alloc.h"

/* ------------------------------------------------------------------ */
/* protocol_7d7e — SLIP framing (RFC 1055 variant)                     */
/*                                                                     */
/* Frame:  START(0x7F) [escaped payload] END(0x7E)                    */
/* Escape: 0x7F→0x7D 0x02  0x7E→0x7D 0x01  0x7D→0x7D 0x00           */
/*                                                                     */
/* tx_buf is sized for worst-case payload (2× expansion + 2 delims).  */
/* rx_buf accumulates decoded data; on_chunk fires per SLIP frame.    */
/* ------------------------------------------------------------------ */

#define SLIP_START     0x7F
#define SLIP_END       0x7E
#define SLIP_ESC       0x7D
#define SLIP_ESC_START 0x02
#define SLIP_ESC_END   0x01
#define SLIP_ESC_ESC   0x00

/* ---- send --------------------------------------------------------- */

static void slip_send_pack(Protocol* p, const uint8_t* data, uint16_t len) {
    if (p == NULL || data == NULL) { return; }

    uint8_t* w   = p->tx_buf;
    uint8_t* end = p->tx_buf + p->tx_buf_size;

    if (w >= end) { return; }
    *w++ = SLIP_START;

    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        if (b == SLIP_START) {
            if (w + 1 >= end) { break; }
            *w++ = SLIP_ESC; *w++ = SLIP_ESC_START;
        } else if (b == SLIP_END) {
            if (w + 1 >= end) { break; }
            *w++ = SLIP_ESC; *w++ = SLIP_ESC_END;
        } else if (b == SLIP_ESC) {
            if (w + 1 >= end) { break; }
            *w++ = SLIP_ESC; *w++ = SLIP_ESC_ESC;
        } else {
            if (w >= end) { break; }
            *w++ = b;
        }
    }

    if (w >= end) { return; }
    *w++ = SLIP_END;

    p->tx_len = (uint16_t)(w - p->tx_buf);
}

static const uint8_t* slip_send_get_buf(const Protocol* p) {
    return (p != NULL) ? p->tx_buf : NULL;
}

static uint16_t slip_send_get_len(const Protocol* p) {
    return (p != NULL) ? p->tx_len : 0;
}

/* ---- receive ------------------------------------------------------ */

static void slip_flush(Protocol* p, uint8_t is_last) {
    if (p->on_chunk == NULL) { return; }

    uint8_t flags = 0;
    if (p->saw_start) { flags |= PROTOCOL_CHUNK_FIRST; }
    if (is_last)    { flags |= PROTOCOL_CHUNK_LAST;  }

    p->on_chunk(p->rx_buf, p->rx_len, flags, p->on_chunk_arg);

    p->rx_len   = 0;
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
                p->rx_state  = proto_7d7e_rx_state_data;
                p->saw_start = 1;
                p->in_frame  = 1;
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
                if (p->rx_len < p->rx_buf_size) {
                    p->rx_buf[p->rx_len++] = b;
                }
            }
            break;

        case proto_7d7e_rx_state_escape:
            if (p->rx_len < p->rx_buf_size) {
                if (b == SLIP_ESC_END)        { p->rx_buf[p->rx_len++] = SLIP_END;  }
                else if (b == SLIP_ESC_ESC)   { p->rx_buf[p->rx_len++] = SLIP_ESC;  }
                else if (b == SLIP_ESC_START) { p->rx_buf[p->rx_len++] = SLIP_START; }
                else                          { p->rx_buf[p->rx_len++] = b;          }
            }
            p->rx_state = proto_7d7e_rx_state_data;
            break;
        }
    }

    /* Partial flush: data arrived but no END marker yet */
    if (!flushed && p->in_frame && (p->rx_len > 0 || p->saw_start)) {
        slip_flush(p, 0);
    }
}

/* ---- destroy ------------------------------------------------------ */

static void slip_destroy(Protocol* p) {
    if (p == NULL) { return; }
    vPortFree(p->tx_buf);
    vPortFree(p->rx_buf);
    vPortFree(p);
}

const Protocol_ops g_protocol_7d7e_ops = {
    .send_pack    = slip_send_pack,
    .send_get_buf = slip_send_get_buf,
    .send_get_len = slip_send_get_len,
    .recv_feed    = slip_recv_feed,
    .destroy      = slip_destroy,
};
