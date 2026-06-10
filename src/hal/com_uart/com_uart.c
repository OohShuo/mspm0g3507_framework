#include "com_uart.h"

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_uart.h"
#include "freertos_alloc.h"
#include "rtt_log.h"
#include "soft_crc.h"
#include "vector.h"

#if UART_NUM

static Vector* com_uart_instances = NULL;

static void com_uart_idle_cb(uint32_t idx, uint8_t* data, uint32_t len, void* arg);

void Com_Uart_Init(void) {
    if (com_uart_instances != NULL) { return; }
    com_uart_instances = Vector_Init(sizeof(Com_uart*), 4);
}

Com_uart* Com_Uart_Create(const Com_uart_config* config) {
    if (config == NULL || com_uart_instances == NULL) { return NULL; }
    if (config->uart_idx >= UART_NUM) { return NULL; }
    if (config->rx_max_len == 0 || config->rx_max_len > 253) { return NULL; }
    if (config->tx_max_len == 0 || config->tx_max_len > 253) { return NULL; }

    Com_uart* obj = (Com_uart*)pvPortMalloc(sizeof(Com_uart));
    if (obj == NULL) { return NULL; }
    memset(obj, 0, sizeof(Com_uart));
    obj->config = *config;

    // tx_buf: [len][payload][crc16_lo][crc16_hi]
    obj->tx_buf = (uint8_t*)pvPortMalloc(1 + config->tx_max_len + 2);
    if (obj->tx_buf == NULL) {
        vPortFree(obj);
        return NULL;
    }

    obj->rx_buf = (uint8_t*)pvPortMalloc(1 + config->rx_max_len + 2);
    if (obj->rx_buf == NULL) {
        vPortFree(obj->tx_buf);
        vPortFree(obj);
        return NULL;
    }

    obj->rx_data.data = (uint8_t*)pvPortMalloc(config->rx_max_len);
    if (obj->rx_data.data == NULL) {
        vPortFree(obj->rx_buf);
        vPortFree(obj->tx_buf);
        vPortFree(obj);
        return NULL;
    }

    Bsp_Uart_Register_Rx_Idle_Cb(config->uart_idx, com_uart_idle_cb, obj);
    Bsp_Uart_Start_Continuous_Rx(
        config->uart_idx, config->idle_timeout_ms, obj->rx_buf, config->rx_max_len + 3);

    Vector_Push_Back(com_uart_instances, (void*)&obj);
    return obj;
}

void Com_Uart_Send(Com_uart* obj, const uint8_t* data, uint32_t len) {
    if (obj == NULL || data == NULL) { return; }
    if (len == 0 || len > obj->config.tx_max_len) { return; }
    if (soft_crc16_default == NULL) { return; }

    obj->tx_buf[0] = (uint8_t)len;
    memcpy(&obj->tx_buf[1], data, len);
    uint16_t crc = Soft_Crc16_Calc(soft_crc16_default, obj->tx_buf, len + 1);
    obj->tx_buf[1 + len] = (uint8_t)(crc & 0xFF);
    obj->tx_buf[2 + len] = (uint8_t)(crc >> 8);

    Bsp_Uart_Write(obj->config.uart_idx, obj->tx_buf, len + 3);
}

static void com_uart_idle_cb(uint32_t idx, uint8_t* data, uint32_t len, void* arg) {
    Com_uart* obj = (Com_uart*)arg;
    if (obj == NULL || obj->config.uart_idx != idx) { return; }

    if (len < 3) { return; }
    uint8_t payload_len = data[0];
    uint32_t expected = (uint32_t)payload_len + 3u;
    if (expected != len) {
        printf(
            "[com_uart] Idle RX length mismatch: expected %u, got %u\n", (unsigned)expected, (unsigned)len);
        return;
    }

    uint16_t crc_rx = (uint16_t)data[len - 2] | ((uint16_t)data[len - 1] << 8);
    if (obj->config.use_crc) {
        uint16_t crc_chk = Soft_Crc16_Calc(soft_crc16_default, data, len - 2);
        if (crc_rx != crc_chk) {
            printf("[com_uart] Idle RX CRC mismatch: expected 0x%04X, got 0x%04X\n", (unsigned)crc_chk,
                (unsigned)crc_rx);
            return;
        }
    }

    obj->rx_data.len = payload_len;
    obj->rx_data.crc16 = crc_rx;
    memcpy(obj->rx_data.data, &data[1], payload_len);
    obj->rx_update_flag = 1;

    if (obj->config.on_rx != NULL) { obj->config.on_rx(obj, &data[1], payload_len, obj->config.on_rx_arg); }
}

#else

void Com_Uart_Init(void) {}

Com_uart* Com_Uart_Create(const Com_uart_config* config) {
    (void)config;
    return NULL;
}

void Com_Uart_Send(Com_uart* obj, const uint8_t* data, uint32_t len) {
    (void)obj;
    (void)data;
    (void)len;
}

#endif
