#include "com_uart.h"

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_uart.h"
#include "freertos_alloc.h"
#include "rtt_log.h"
#include "vector.h"

#if UART_NUM

static Vector* com_uart_instances = NULL;

static void com_uart_idle_cb(uint32_t idx, uint8_t* data, uint32_t len, void* arg);
static void com_uart_proto_on_chunk(const uint8_t* data, uint16_t len, uint8_t flags, void* arg);

void Com_Uart_Init(void) {
    if (com_uart_instances != NULL) { return; }
    com_uart_instances = Vector_Init(sizeof(Com_uart*), 4);
}

Com_uart* Com_Uart_Create(const Com_uart_config* config) {
    if (config == NULL || com_uart_instances == NULL) { return NULL; }
    if (config->uart_idx >= UART_NUM) { return NULL; }
    if (config->rx_max_len == 0) { return NULL; }
    if (config->tx_max_len == 0) { return NULL; }

    Com_uart* obj = (Com_uart*)pvPortMalloc(sizeof(Com_uart));
    if (obj == NULL) { return NULL; }
    memset(obj, 0, sizeof(Com_uart));
    obj->config = *config;

    obj->rx_buf = (uint8_t*)pvPortMalloc(config->rx_max_len);
    if (obj->rx_buf == NULL) {
        vPortFree(obj);
        return NULL;
    }

    obj->proto =
        Protocol_Create(config->protocol_type, config->protocol_max_payload, com_uart_proto_on_chunk, obj);

    Bsp_Uart_Register_Rx_Idle_Cb(config->uart_idx, com_uart_idle_cb, obj);
    Bsp_Uart_Start_Continuous_Rx(config->uart_idx, config->idle_timeout_ms, obj->rx_buf, config->rx_max_len);

    Vector_Push_Back(com_uart_instances, (void*)&obj);
    return obj;
}

void Com_Uart_Send(Com_uart* obj, const uint8_t* data, uint32_t len) {
    if (obj == NULL || data == NULL) { return; }

    if (obj->proto != NULL) {
        obj->proto->ops->send_pack(obj->proto, data, (uint16_t)len);
        data = obj->proto->ops->send_get_buf(obj->proto);
        len = obj->proto->ops->send_get_len(obj->proto);
    }

    if (len == 0 || len > obj->config.tx_max_len) { return; }
    Bsp_Uart_Write(obj->config.uart_idx, data, len);
}

static void com_uart_proto_on_chunk(const uint8_t* data, uint16_t len, uint8_t flags, void* arg) {
    Com_uart* obj = (Com_uart*)arg;
    if (obj == NULL) { return; }
    if (obj->config.on_rx != NULL) { obj->config.on_rx(obj, data, len, flags, obj->config.on_rx_arg); }
}

static void com_uart_idle_cb(uint32_t idx, uint8_t* data, uint32_t len, void* arg) {
    Com_uart* obj = (Com_uart*)arg;
    if (obj == NULL || obj->config.uart_idx != idx) { return; }
    if (len == 0) { return; }

    if (obj->proto != NULL) { obj->proto->ops->recv_feed(obj->proto, data, (uint16_t)len); }
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
