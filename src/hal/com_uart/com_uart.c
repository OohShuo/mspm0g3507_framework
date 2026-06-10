#include "com_uart.h"

#include <stdint.h>
#include <string.h>

#include "board_config.h"
#include "bsp_uart.h"
#include "freertos_alloc.h"
#include "soft_crc.h"
#include "vector.h"

#if UART_NUM

static Vector* com_uart_instances = NULL;

static void com_uart_idle_cb(uint32_t idx, uint8_t* data, uint32_t len);

void Com_Uart_Init(void) {
    if (com_uart_instances != NULL) { return; }

    com_uart_instances = Vector_Init(sizeof(Com_uart*), 4);
}

Com_uart* Com_Uart_Create(const Com_uart_config* config) {
    if (config == NULL || com_uart_instances == NULL) { return NULL; }
    if (config->data_len == 0 || config->data_len > 253) { return NULL; }
    if (config->uart_idx >= UART_NUM) { return NULL; }

    Com_uart* obj = (Com_uart*)pvPortMalloc(sizeof(Com_uart));
    if (obj == NULL) { return NULL; }
    memset(obj, 0, sizeof(Com_uart));

    obj->config = *config;
    obj->data_rx.len = config->data_len;
    obj->data_rx.data = (uint8_t*)pvPortMalloc(config->data_len);
    obj->data_rx.crc16 = 0;
    if (obj->data_rx.data == NULL) {
        vPortFree(obj);
        return NULL;
    }

    obj->tx_buf = (uint8_t*)pvPortMalloc((size_t)config->data_len + 3);
    if (obj->tx_buf == NULL) {
        vPortFree(obj->data_rx.data);
        vPortFree(obj);
        return NULL;
    }

    Bsp_Uart_Register_Rx_Idle_Cb(config->uart_idx, com_uart_idle_cb);
    Bsp_Uart_Start_Continuous_Rx(config->uart_idx, config->idle_timeout_ms);

    Vector_Push_Back(com_uart_instances, (void*)&obj);
    return obj;
}

void Com_Uart_Send(Com_uart* obj, const uint8_t* data) {
    if (obj == NULL || obj->tx_buf == NULL || data == NULL) { return; }
    if (soft_crc16_default == NULL) { return; }

    General_data frame = {
        .len = obj->config.data_len,
        .data = (uint8_t*)data,
        .crc16 = 0,
    };
    General_Data_To_Buffer(&frame, obj->tx_buf);
    uint16_t crc = Soft_Crc16_Calc(soft_crc16_default, obj->tx_buf, (uint32_t)obj->config.data_len + 1);
    memcpy(obj->tx_buf + 1 + obj->config.data_len, &crc, 2);

    Bsp_Uart_Write(obj->config.uart_idx, obj->tx_buf, (uint32_t)obj->config.data_len + 3);
}

static void com_uart_idle_cb(uint32_t idx, uint8_t* data, uint32_t len) {
    if (com_uart_instances == NULL) { return; }
    if (len < 3) { return; }

    uint16_t crc_rx;
    memcpy(&crc_rx, data + len - 2, 2);
    uint16_t crc_chk = Soft_Crc16_Calc(soft_crc16_default, data, len - 2);
    if (crc_rx != crc_chk) { return; }

    for (uint32_t i = 0; i < Vector_Get_Size(com_uart_instances); i++) {
        Com_uart* obj = *(Com_uart**)Vector_Get_At(com_uart_instances, i);
        if (obj == NULL) { continue; }
        if (obj->config.uart_idx != idx) { continue; }
        if (len != (uint32_t)obj->config.data_len + 3) { continue; }
        if (data[0] != obj->config.data_len) { continue; }

        memcpy(obj->data_rx.data, data + 1, obj->config.data_len);
        obj->data_rx.crc16 = crc_rx;

        if (obj->config.on_rx != NULL) { obj->config.on_rx(obj, obj->config.on_rx_arg); }
    }
}

#else  // UART_NUM == 0

void Com_Uart_Init(void) {}

Com_uart* Com_Uart_Create(const Com_uart_config* config) {
    (void)config;
    return NULL;
}

void Com_Uart_Send(Com_uart* obj, const uint8_t* data) {
    (void)obj;
    (void)data;
}

#endif
