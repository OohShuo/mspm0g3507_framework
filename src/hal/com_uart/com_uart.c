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

    Com_uart* obj = (Com_uart*)pvPortMalloc(sizeof(Com_uart));
    if (obj == NULL) { return NULL; }
    memset(obj, 0, sizeof(Com_uart));
    obj->config = *config;

    obj->tx_buf = (uint8_t*)pvPortMalloc(1 + config->tx_max_len + 2);  // [len][data][crc16_lo][crc16_hi]
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

    Bsp_Uart_Register_Rx_Idle_Cb(config->uart_idx, com_uart_idle_cb, obj);
    Bsp_Uart_Start_Continuous_Rx(
        config->uart_idx, config->idle_timeout_ms, obj->rx_buf, config->rx_max_len + 3);

    Vector_Push_Back(com_uart_instances, (void*)&obj);
    return obj;
}

void Com_Uart_Send(Com_uart* obj, const uint8_t* data, uint32_t len) {
    if (obj == NULL || data == NULL) { return; }
    if (len == 0 || len > 253) { return; }
    if (soft_crc16_default == NULL) { return; }

    obj->tx_buf[0] = (uint8_t)len;
    memcpy(&obj->tx_buf[1], data, len);
    uint16_t crc = Soft_Crc16_Calc(soft_crc16_default, obj->tx_buf, len + 1);
    obj->tx_buf[1 + len] = (uint8_t)(crc & 0xFF);
    obj->tx_buf[2 + len] = (uint8_t)(crc >> 8);

    Bsp_Uart_Write(obj->config.uart_idx, obj->tx_buf, len + 3);
}

static void com_uart_idle_cb(uint32_t idx, uint8_t* data, uint32_t len, void* arg) {
    if (com_uart_instances == NULL) { return; }
    if (len < 3) { return; }

    Com_uart* obj = (Com_uart*)arg;
    if (obj->config.uart_idx != idx) { return; }

    obj->rx_data.len = len - 3;
    memcpy(obj->rx_data.data, &data[1], obj->rx_data.len);
    memcpy(&obj->rx_data.crc16, &data[1 + obj->rx_data.len], 2);
    obj->rx_update_flag = 1;

    // TODO crc 校验
    if (1) {
    } else {
        printf("[com_uart] crc failed: expected ..., got ...\n");
    }

    for (uint32_t i = 0; i < Vector_Get_Size(com_uart_instances); i++) {
        Com_uart* obj = *(Com_uart**)Vector_Get_At(com_uart_instances, i);
        if (obj == NULL) { continue; }
        if (obj->config.uart_idx != idx) { continue; }
        if (obj->config.on_rx != NULL) {
            obj->config.on_rx(obj, &data[1], obj->rx_data.len, obj->config.on_rx_arg);
        }
    }
}

#else  // UART_NUM == 0

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
