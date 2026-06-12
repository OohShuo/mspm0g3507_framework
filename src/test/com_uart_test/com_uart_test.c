#include "com_uart_test.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bsp_time.h"
#include "com_uart.h"
#include "freertos_alloc.h"
#include "rtt_log.h"
#include "task.h"

#define COM_UART_TEST_UART_IDX       0
#define COM_UART_TEST_RX_MAX_LEN     253
#define COM_UART_TEST_TX_MAX_LEN     253
#define COM_UART_TEST_IDLE_TIMEOUT   5  // ms
#define COM_UART_TEST_SEND_PERIOD_MS 1000

static Com_uart* g_com = NULL;
static uint32_t g_last_send_ms = 0;

static TaskHandle_t g_task_handle = NULL;

static void on_rx(Com_uart* obj, const uint8_t* data, uint32_t len, uint8_t flags, void* arg) {
    (void)flags;
    (void)arg;

    char buf[129];
    uint32_t n = (len < sizeof(buf) - 1) ? len : (uint32_t)(sizeof(buf) - 1);
    memcpy(buf, data, n);
    buf[n] = '\0';

    for (uint32_t i = 0; i < n; i++) {
        char c = (data[i] >= 32 && data[i] <= 126) ? (char)data[i] : '.';
        printf("%c", c);
    }
    printf("\n");

    Com_Uart_Send(obj, data, len);
}

static void com_uart_test_init(void) {
    static const Com_uart_config cfg = {
        .uart_idx = COM_UART_TEST_UART_IDX,
        .idle_timeout_ms = COM_UART_TEST_IDLE_TIMEOUT,
        .rx_max_len = COM_UART_TEST_RX_MAX_LEN,
        .tx_max_len = COM_UART_TEST_TX_MAX_LEN,
        .on_rx = on_rx,
        .on_rx_arg = NULL,
    };
    g_com = Com_Uart_Create(&cfg);
    g_last_send_ms = Bsp_Get_Tick_Ms();
}

static void com_uart_test_loop(void) {
    if (g_com == NULL) { return; }

    const uint32_t now_ms = Bsp_Get_Tick_Ms();
    if ((now_ms - g_last_send_ms) < COM_UART_TEST_SEND_PERIOD_MS) { return; }
    g_last_send_ms = now_ms;

    char buf[64];
    int n = snprintf(buf, sizeof(buf), "From board %lu.%03lu s", (unsigned long)(now_ms / 1000u),
        (unsigned long)(now_ms % 1000u));
    if (n <= 0) { return; }

    static char* payloads[] = {
        "Hello, world!\n",
        "The quick brown fox jumps over the lazy dog.\n",
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n",
        "1234567890\n",
        "!@#$^&*()_+-=[]{}|;':\",./<>?\n",
        "Short\n",
    };

    static uint32_t payload_idx = 0;
    Com_Uart_Send(g_com, (const uint8_t*)payloads[payload_idx], (uint32_t)strlen(payloads[payload_idx]));
    payload_idx = (payload_idx + 1) % (sizeof(payloads) / sizeof(payloads[0]));
}

static void com_uart_test_task(void* arg) {
    (void)arg;

    uint32_t tick = xTaskGetTickCount();

    com_uart_test_init();

    while (1) {
        com_uart_test_loop();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(10));
    }
}

void Com_Uart_Test_Task_Def(void) {
    xTaskCreate(com_uart_test_task, "Com_UART_Test", 512, NULL, 1, &g_task_handle);
}
