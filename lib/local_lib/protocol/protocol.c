#include "protocol.h"

#include <string.h>

#include "freertos_alloc.h"

/* ------------------------------------------------------------------ */
/* Ops tables — one per protocol type, each defined in its own .c file */
/* ------------------------------------------------------------------ */

extern const Protocol_ops g_protocol_none_ops;
extern const Protocol_ops g_protocol_7d7e_ops;
extern const Protocol_ops g_protocol_binary_ops;

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

Protocol* Protocol_Create(Protocol_type type, uint16_t max_payload,
                          Protocol_on_chunk_t on_chunk, void* arg) {
    Protocol* p = (Protocol*)pvPortMalloc(sizeof(Protocol));
    if (p == NULL) { return NULL; }
    memset(p, 0, sizeof(Protocol));

    switch (type) {
        case protocol_none:
            p->ops         = &g_protocol_none_ops;
            p->tx_buf_size = max_payload;
            p->tx_buf      = (uint8_t*)pvPortMalloc(p->tx_buf_size);
            if (p->tx_buf == NULL) { vPortFree(p); return NULL; }
            break;

        case protocol_7d7e:
            p->ops         = &g_protocol_7d7e_ops;
            p->tx_buf_size = 2 * max_payload + 2;  /* worst-case SLIP expansion */
            p->tx_buf      = (uint8_t*)pvPortMalloc(p->tx_buf_size);
            if (p->tx_buf == NULL) { vPortFree(p); return NULL; }
            p->rx_buf_size = max_payload;
            p->rx_buf      = (uint8_t*)pvPortMalloc(p->rx_buf_size);
            if (p->rx_buf == NULL) { vPortFree(p->tx_buf); vPortFree(p); return NULL; }
            break;

        case protocol_binary_frame:
            p->ops         = &g_protocol_binary_ops;
            p->tx_buf_size = (uint16_t)(max_payload + 6u);  /* SYNC(2)+LEN(2)+CRC(2) */
            p->tx_buf      = (uint8_t*)pvPortMalloc(p->tx_buf_size);
            if (p->tx_buf == NULL) { vPortFree(p); return NULL; }
            p->rx_buf_size = max_payload;
            p->rx_buf      = (uint8_t*)pvPortMalloc(p->rx_buf_size);
            if (p->rx_buf == NULL) { vPortFree(p->tx_buf); vPortFree(p); return NULL; }
            break;

        default:
            vPortFree(p);
            return NULL;
    }

    p->on_chunk     = on_chunk;
    p->on_chunk_arg = arg;
    return p;
}

void Protocol_Destroy(Protocol* p) {
    if (p == NULL) { return; }
    if (p->ops != NULL && p->ops->destroy != NULL) { p->ops->destroy(p); }
}
