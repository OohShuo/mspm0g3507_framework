#include "test_slip_recv.h"

#include <stdint.h>

#include "FreeRTOS.h"
#include "com_uart.h"
#include "rtt_log.h"
#include "task.h"

#define SLIP_UART_IDX        0
#define SLIP_IDLE_TIMEOUT_MS 5
#define SLIP_RX_DMA_BUF_LEN  512
#define SLIP_TX_BUF_LEN      1
#define SLIP_MAX_PAYLOAD     512

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
    if (flags & PROTOCOL_CHUNK_LAST) { printf("end.\n"); }
}

static void slip_recv_init(void) {
    static const Com_uart_config cfg = {
        .uart_idx = SLIP_UART_IDX,
        .idle_timeout_ms = SLIP_IDLE_TIMEOUT_MS,
        .rx_max_len = SLIP_RX_DMA_BUF_LEN,
        .tx_max_len = SLIP_TX_BUF_LEN,
        .protocol_type = protocol_7d7e,
        .protocol_max_payload = SLIP_MAX_PAYLOAD,
        .on_rx = slip_on_chunk,
        .on_rx_arg = NULL,
    };
    g_slip_uart = Com_Uart_Create(&cfg);
}

static void slip_recv_loop(void) {}

static void slip_recv_task(void* arg) {
    (void)arg;
    slip_recv_init();
    while (1) { slip_recv_loop(); }
}

void Test_Slip_Recv_Task_Def(void) { xTaskCreate(slip_recv_task, "SlipRecv", 256, NULL, 1, NULL); }
