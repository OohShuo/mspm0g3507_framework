#include "com_uart_test.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "bsp_time.h"
#include "com_uart.h"
#include "rtt_log.h"

// === Config ===
#define COM_UART_TEST_UART_IDX       0
#define COM_UART_TEST_DATA_LEN       32  // payload size, must fit "From board 999.999 s" comfortably
#define COM_UART_TEST_IDLE_TIMEOUT   10  // ms; 10 ms is a safe mid-speed idle threshold
#define COM_UART_TEST_SEND_PERIOD_MS 1000

// === State ===
static Com_uart* g_com = NULL;
static uint32_t g_last_send_ms = 0;

static void on_rx(Com_uart* obj, void* arg) {
    (void)arg;
    if (obj == NULL) { return; }
    // obj->data_rx.data holds the payload (COM_UART_TEST_DATA_LEN bytes),
    // .len is the payload length. Print as a null-terminated string.
    char buf[COM_UART_TEST_DATA_LEN + 1];
    memcpy(buf, obj->data_rx.data, obj->data_rx.len);
    buf[obj->data_rx.len] = '\0';
    printf("[com_uart_test] RX (%u): %s\n", (unsigned)obj->data_rx.len, buf);
}

void App_Com_Uart_Test_Init(void) {
    static const Com_uart_config cfg = {
        .uart_idx = COM_UART_TEST_UART_IDX,
        .data_len = COM_UART_TEST_DATA_LEN,
        .idle_timeout_ms = COM_UART_TEST_IDLE_TIMEOUT,
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

    char buf[COM_UART_TEST_DATA_LEN];
    int n = snprintf(buf, sizeof(buf), "From board %lu.%03lu s", (unsigned long)(now_ms / 1000u),
        (unsigned long)(now_ms % 1000u));
    if (n < 0 || (uint32_t)n > COM_UART_TEST_DATA_LEN) { return; }

    Com_Uart_Send(g_com, (const uint8_t*)buf);
}
