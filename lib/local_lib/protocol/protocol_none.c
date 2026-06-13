#include <string.h>

#include "freertos_alloc.h"
#include "protocol.h"

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

const Protocol_ops g_protocol_none_ops = {
    .send_pack = none_send_pack,
    .send_get_buf = none_send_get_buf,
    .send_get_len = none_send_get_len,
    .recv_feed = none_recv_feed,
    .destroy = none_destroy,
};
