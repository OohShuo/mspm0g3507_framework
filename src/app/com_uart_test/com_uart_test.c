#include "com_uart_test.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "bsp_time.h"
#include "com_uart.h"
#include "rtt_log.h"

#define COM_UART_TEST_UART_IDX       0
#define COM_UART_TEST_IDLE_TIMEOUT   10   // ms
#define COM_UART_TEST_SEND_PERIOD_MS 1000

static Com_uart* g_com = NULL;
static uint32_t g_last_send_ms = 0;

static void on_rx(Com_uart* obj, const uint8_t* data, uint32_t len, void* arg) {
    (void)obj;
    (void)arg;
    // data points into the bsp's internal rx buffer — only valid during this call.
    // Copy out for printing. Cap at 128 to keep stack usage bounded.
    char buf[129];
    uint32_t n = (len < sizeof(buf) - 1) ? len : (uint32_t)(sizeof(buf) - 1);
    memcpy(buf, data, n);
    buf[n] = '\0';
    printf("[com_uart_test] RX (%u): %s\n", (unsigned)len, buf);
}

void App_Com_Uart_Test_Init(void) {
    static const Com_uart_config cfg = {
        .uart_idx = COM_UART_TEST_UART_IDX,
        .idle_timeout_ms = COM_UART_TEST_IDLE_TIMEOUT,
        .rx_max_len = 128,
        .tx_max_len = 128,
        .on_rx = on_rx,
        .on_rx_arg = NULL,
    };
    g_com = Com_Uart_Create(&cfg);
    g_last_send_ms = Bsp_Get_Tick_Ms();
}

void App_Com_Uart_Test_Loop(void) {
    if (g_com == NULL) { return; }

    const uint32_t now_ms = Bsp_Get_Tick_Ms();
    if ((now_ms - g_last_send_ms) < COM_UART_TEST_SEND_PERIOD_MS) { return; }
    g_last_send_ms = now_ms;

    char buf[64];
    int n = snprintf(buf, sizeof(buf), "From board %lu.%03lu s", (unsigned long)(now_ms / 1000u),
        (unsigned long)(now_ms % 1000u));
    if (n <= 0) { return; }

    Com_Uart_Send(g_com, (const uint8_t*)buf, (uint32_t)n);
}
