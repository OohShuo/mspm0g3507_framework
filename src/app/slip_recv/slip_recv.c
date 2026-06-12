#include "slip_recv.h"

#include <stdint.h>

#include "com_uart.h"
#include "rtt_log.h"

#define SLIP_UART_IDX        0
#define SLIP_IDLE_TIMEOUT_MS 5
#define SLIP_RX_DMA_BUF_LEN  512  // raw DMA buffer (one chunk)
#define SLIP_TX_BUF_LEN      1    // unused (RX-only)
#define SLIP_MAX_PAYLOAD     512  // decoded chunk buffer (matches DMA buf)

static Com_uart* g_slip_uart = NULL;

static void slip_on_chunk(Com_uart* obj, const uint8_t* data, uint32_t len, uint8_t flags, void* arg) {
    (void)obj;
    (void)arg;

    if (flags & PROTOCOL_CHUNK_FIRST) { printf("start\n"); }
    for (uint32_t i = 0; i < len; i++) {
        if ((data[i] >= 32 && data[i] <= 126) || data[i] == '\n') {
            printf("%c", (char)data[i]);
        } else if (data[i] == 0x7d) {
            printf("|");
        } else if (data[i] == 0x7e) {
            printf(">");
        } else if (data[i] == 0x7f) {
            printf("<");
        } else {
            printf(".");
        }
    }
    printf("\n");
    if (flags & PROTOCOL_CHUNK_LAST) { printf("end.\n"); }
}

void App_Slip_Recv_Init(void) {
    static const Com_uart_config cfg = {
        .uart_idx = SLIP_UART_IDX,
        .idle_timeout_ms = SLIP_IDLE_TIMEOUT_MS,
        .rx_max_len = SLIP_RX_DMA_BUF_LEN,
        .tx_max_len = SLIP_TX_BUF_LEN,
        .protocol_type = protocol_none,
        .protocol_max_payload = SLIP_MAX_PAYLOAD,
        .on_rx = slip_on_chunk,
        .on_rx_arg = NULL,
    };
    g_slip_uart = Com_Uart_Create(&cfg);
}

void App_Slip_Recv_Loop(void) {
    // All work is done in the on_rx callback (ISR context).
    // This task body just parks.
}
