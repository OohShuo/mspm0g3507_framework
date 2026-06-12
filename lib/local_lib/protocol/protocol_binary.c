#include "protocol.h"

#include <string.h>

#include "freertos_alloc.h"
#include "soft_crc.h"

/* ------------------------------------------------------------------ */
/* Binary frame format                                                  */
/*                                                                     */
/*   SYNC0(0xAA) SYNC1(0x55) LEN_H LEN_L [data:0-512B] CRCH CRCL      */
/*                                                                     */
/* CRC16 covers LEN_H + LEN_L + data (= 2 + data_len bytes).           */
/* CRC polynomial: 0x1021, init: 0xFFFF (via soft_crc).                */
/*                                                                     */
/* send_pack(data, len): wraps data with SYNC + LEN + CRC.             */
/* recv_feed: state machine parses wire bytes, validates CRC,          */
/*            delivers decoded data to on_chunk callback.              */
/* ------------------------------------------------------------------ */

#define BIN_SYNC0 0xAAu
#define BIN_SYNC1 0x55u

/* ---- send --------------------------------------------------------- */

static void bin_send_pack(Protocol* p, const uint8_t* data, uint16_t len) {
    if (p == NULL || data == NULL) { return; }
    /* Check worst-case size: 2(sync) + 2(len) + len(data) + 2(crc) */
    if ((uint32_t)len + 6u > p->tx_buf_size) { return; }

    uint8_t* w   = p->tx_buf;
    *w++ = BIN_SYNC0;
    *w++ = BIN_SYNC1;
    *w++ = (uint8_t)(len >> 8);       /* LEN_H */
    *w++ = (uint8_t)(len);            /* LEN_L */
    memcpy(w, data, len);
    w += len;

    /* CRC over LEN(2) + data(len) — copy to a small local buffer
     * then feed to Soft_Crc16_Calc in one call.  The widest span
     * here is len ≤ 515 B, but rather than stack-allocate, use the
     * tail of tx_buf (already sized for 6+len and the next 6 bytes
     * are available after w). */
    /* Build crc_input = LEN_H LEN_L + data in a contiguous region.
     * p->tx_buf already contains [SYNC0][SYNC1][LEN_H][LEN_L][data...]
     * so LEN+data starts at p->tx_buf + 2. */
    uint16_t crc = Soft_Crc16_Calc(soft_crc16_default,
                                   p->tx_buf + 2,
                                   (uint32_t)(2u + len));
    *w++ = (uint8_t)(crc >> 8);
    *w++ = (uint8_t)(crc);

    p->tx_len = (uint16_t)(w - p->tx_buf);
}

static const uint8_t* bin_send_get_buf(const Protocol* p) {
    return (p != NULL) ? p->tx_buf : NULL;
}

static uint16_t bin_send_get_len(const Protocol* p) {
    return (p != NULL) ? p->tx_len : 0;
}

/* ---- receive ------------------------------------------------------ */

static void bin_flush(Protocol* p) {
    if (p->on_chunk == NULL) { return; }
    p->on_chunk(p->rx_buf, p->rx_len,
                PROTOCOL_CHUNK_FIRST | PROTOCOL_CHUNK_LAST,
                p->on_chunk_arg);
    p->rx_len = 0;
}

static void bin_recv_reset(Protocol* p) {
    p->bin_state    = proto_bin_rx_sync0;
    p->bin_len_pos  = 0;
    p->bin_data_pos = 0;
    p->bin_crc_pos  = 0;
    p->rx_len       = 0;
}

static void bin_recv_feed(Protocol* p, const uint8_t* data, uint16_t len) {
    if (p == NULL || data == NULL) { return; }

    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = data[i];

        switch (p->bin_state) {

        case proto_bin_rx_sync0:
            if (b == BIN_SYNC0) {
                p->bin_state = proto_bin_rx_sync1;
            }
            break;

        case proto_bin_rx_sync1:
            if (b == BIN_SYNC1) {
                p->bin_state   = proto_bin_rx_len;
                p->bin_len_pos = 0;
            } else if (b != BIN_SYNC0) {
                p->bin_state = proto_bin_rx_sync0;
            }
            /* b == SYNC0: stay in sync1 (allows 0xAA 0xAA 0x55) */
            break;

        case proto_bin_rx_len:
            p->bin_len_buf[p->bin_len_pos++] = b;
            if (p->bin_len_pos >= 2) {
                p->bin_data_len = ((uint16_t)p->bin_len_buf[0] << 8)
                                |  p->bin_len_buf[1];
                if (p->bin_data_len > p->rx_buf_size) {
                    /* Invalid length — reset */
                    bin_recv_reset(p);
                } else if (p->bin_data_len == 0) {
                    /* Empty payload — go straight to CRC */
                    p->bin_state   = proto_bin_rx_crc;
                    p->bin_crc_pos = 0;
                } else {
                    p->bin_state    = proto_bin_rx_data;
                    p->bin_data_pos = 0;
                    p->rx_len       = 0;
                }
            }
            break;

        case proto_bin_rx_data:
            if (p->rx_len < p->rx_buf_size) {
                p->rx_buf[p->rx_len++] = b;
            }
            if (++p->bin_data_pos >= p->bin_data_len) {
                p->bin_state   = proto_bin_rx_crc;
                p->bin_crc_pos = 0;
            }
            break;

        case proto_bin_rx_crc:
            p->bin_crc_buf[p->bin_crc_pos++] = b;
            if (p->bin_crc_pos >= 2) {
                /* Frame complete — verify CRC */
                bin_recv_reset(p);  /* ready for next frame */

                uint16_t wire_crc = ((uint16_t)p->bin_crc_buf[0] << 8)
                                  |  p->bin_crc_buf[1];

                /* CRC input = LEN(2) + data(rx_len).
                 * Build in a local buffer — max 2+515=517 B.
                 * Use rx_buf+rx_len scratch area: shift rx_buf by 2
                 * (temporarily), restore after.  Or simpler: use a
                 * small stack buffer for LEN(2) + data.
                 *
                 * Actually the simplest: CRC_LEN + CRC_DATA can be
                 * computed by re-feeding to the algorithm.  But
                 * Soft_Crc16_Calc is stateless...
                 *
                 * We need a contiguous byte sequence of LEN(2)+DATA(N).
                 * rx_buf currently holds just DATA (rx_len bytes).
                 * There's no built-in scratch area.
                 *
                 * Safer: use a static buffer (BSS, not stack).
                 * The max is 2+515=517 bytes.  One static is fine. */
                static uint8_t crc_buf[520];  /* 2 + 515 + margin */
                crc_buf[0] = p->bin_len_buf[0];
                crc_buf[1] = p->bin_len_buf[1];
                if (p->rx_len > 0) {
                    memcpy(crc_buf + 2, p->rx_buf, p->rx_len);
                }
                uint16_t calc_crc = Soft_Crc16_Calc(
                    soft_crc16_default, crc_buf,
                    (uint32_t)(2u + p->rx_len));

                if (calc_crc == wire_crc && p->rx_len > 0) {
                    bin_flush(p);
                }
                /* On CRC mismatch or zero-length: silently drop */
            }
            break;
        } /* switch */
    }
}

/* ---- destroy ------------------------------------------------------ */

static void bin_destroy(Protocol* p) {
    if (p == NULL) { return; }
    vPortFree(p->tx_buf);
    vPortFree(p->rx_buf);
    vPortFree(p);
}

/* ---- ops table ---------------------------------------------------- */

const Protocol_ops g_protocol_binary_ops = {
    .send_pack    = bin_send_pack,
    .send_get_buf = bin_send_get_buf,
    .send_get_len = bin_send_get_len,
    .recv_feed    = bin_recv_feed,
    .destroy      = bin_destroy,
};
