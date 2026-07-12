#include "test_w5500_udp.h"

#include "test_config.h"

#if TEST_W5500_UDP_ENABLE

    #include <stddef.h>

    #include "FreeRTOS.h"
    #include "board_config.h"
    #include "com_udp.h"
    #include "task.h"
    #include "w5500_hal.h"

    #define W5500_TEST_SPI_IDX        SPI_LCD_IDX
    #define W5500_TEST_CS_GPIO_IDX    GPIO_SPI_CS_IDX
    #define W5500_TEST_RST_GPIO_IDX   GPIO_TFT_RST_IDX
    #define W5500_TEST_SOCKET         0u
    #define W5500_TEST_LOCAL_PORT     5000u
    #define W5500_TEST_RX_BUF_SIZE    512u
    #define W5500_TEST_TX_BUF_SIZE    512u
    #define W5500_TEST_POLL_PERIOD_MS 10u

static Wiz5500* g_wiz = NULL;
static Com_udp* g_udp = NULL;

static void on_rx(Com_udp* obj, const uint8_t* data, uint32_t len, uint8_t flags, void* arg) {
    (void)flags;
    (void)arg;

    uint8_t src_ip[4];
    uint16_t src_port;
    Com_Udp_Get_Src(obj, src_ip, &src_port);
    Com_Udp_Send(obj, data, len, src_ip, src_port);
}

static void w5500_udp_test_init(void) {
    const W5500_Config wiz_cfg = {
        .spi_idx = W5500_TEST_SPI_IDX,
        .cs_gpio_idx = W5500_TEST_CS_GPIO_IDX,
        .rst_gpio_idx = W5500_TEST_RST_GPIO_IDX,
        .spi_mutex = NULL,
    };
    g_wiz = W5500_Create(&wiz_cfg);
    if (g_wiz == NULL) { return; }

    const Com_udp_config udp_cfg = {
        .wiz = g_wiz,
        .mac = {0x02, 0x00, 0x00, 0x00, 0x55, 0x00},
        .ip = {192, 168, 1, 123},
        .sn = {255, 255, 255, 0},
        .gw = {192, 168, 1, 1},
        .sock_n = W5500_TEST_SOCKET,
        .local_port = W5500_TEST_LOCAL_PORT,
        .rx_buf_size = W5500_TEST_RX_BUF_SIZE,
        .tx_buf_size = W5500_TEST_TX_BUF_SIZE,
        .on_rx = on_rx,
        .on_rx_arg = NULL,
    };
    g_udp = Com_Udp_Create(&udp_cfg);
}

static void w5500_udp_test_task(void* arg) {
    (void)arg;
    w5500_udp_test_init();

    uint32_t tick = xTaskGetTickCount();
    while (1) {
        if (g_udp != NULL) { Com_Udp_Poll(); }
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(W5500_TEST_POLL_PERIOD_MS));
    }
}

void Test_W5500_Udp_Task_Def(void) { xTaskCreate(w5500_udp_test_task, "W5500_UDP_Test", 512, NULL, 1, NULL); }

#endif
