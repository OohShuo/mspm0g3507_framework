#include "slip_recv.h"

#include <stdint.h>
#include <string.h>

#include "com_uart.h"
#include "rtt_log.h"

#define SLIP_START 0x7F
#define SLIP_END   0x7E
#define SLIP_ESC   0x7D

#define SLIP_ESC_END   0x01
#define SLIP_ESC_ESC   0x00
#define SLIP_ESC_START 0x02

#define SLIP_UART_IDX        0
#define SLIP_RX_MAX_LEN      512
#define SLIP_TX_MAX_LEN      1
#define SLIP_IDLE_TIMEOUT_MS 5
#define SLIP_DECODED_MAX     512

static Com_uart* g_slip_uart = NULL;

static uint8_t  g_slip_decoded[SLIP_DECODED_MAX];
static uint32_t g_slip_decoded_len;
static uint8_t  g_slip_in_frame;
static uint8_t  g_slip_escape_next;

static void flush_decoded(void) {
    if (g_slip_decoded_len == 0) { return; }
    printf("%.*s", (int)g_slip_decoded_len, g_slip_decoded);
    g_slip_decoded_len = 0;
}

static void slip_on_rx(Com_uart* obj, const uint8_t* data, uint32_t len, void* arg) {
    (void)obj;
    (void)arg;

    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = data[i];

        if (byte == SLIP_END) {
            if (g_slip_in_frame) {
                flush_decoded();
                printf("end.\n");
                g_slip_in_frame    = 0;
                g_slip_escape_next = 0;
            }
        } else if (byte == SLIP_START) {
            g_slip_in_frame    = 1;
            g_slip_decoded_len = 0;
            g_slip_escape_next = 0;
            printf("start\n");
        } else if (g_slip_in_frame) {
            if (g_slip_escape_next) {
                if (byte == SLIP_ESC_END) {
                    g_slip_decoded[g_slip_decoded_len++] = SLIP_END;
                } else if (byte == SLIP_ESC_ESC) {
                    g_slip_decoded[g_slip_decoded_len++] = SLIP_ESC;
                } else if (byte == SLIP_ESC_START) {
                    g_slip_decoded[g_slip_decoded_len++] = SLIP_START;
                } else {
                    g_slip_decoded[g_slip_decoded_len++] = byte;
                }
                g_slip_escape_next = 0;

                if (g_slip_decoded_len >= SLIP_DECODED_MAX) {
                    g_slip_decoded_len = SLIP_DECODED_MAX - 1;
                }
            } else if (byte == SLIP_ESC) {
                g_slip_escape_next = 1;
            } else {
                g_slip_decoded[g_slip_decoded_len++] = byte;
                if (g_slip_decoded_len >= SLIP_DECODED_MAX) {
                    g_slip_decoded_len = SLIP_DECODED_MAX - 1;
                }
            }
        }
    }

    // Print decoded data chunk-by-chunk as each com_uart idle callback fires.
    flush_decoded();
}

void App_Slip_Recv_Init(void) {
    g_slip_in_frame    = 0;
    g_slip_decoded_len = 0;
    g_slip_escape_next = 0;

    static const Com_uart_config cfg = {
        .uart_idx       = SLIP_UART_IDX,
        .idle_timeout_ms = SLIP_IDLE_TIMEOUT_MS,
        .rx_max_len     = SLIP_RX_MAX_LEN,
        .tx_max_len     = SLIP_TX_MAX_LEN,
        .on_rx          = slip_on_rx,
        .on_rx_arg      = NULL,
    };
    g_slip_uart = Com_Uart_Create(&cfg);
}

void App_Slip_Recv_Loop(void) {
    // All work is done in the on_rx idle callback (ISR context).
    // This task body just parks.
}
