#pragma once

#include <stdint.h>

#include "w5500_hal.h"

typedef struct Com_udp_t Com_udp;

/**
 * @brief RX callback — same signature as Com_uart_on_rx_t.
 *
 * Use Com_Udp_Get_Src() to retrieve the sender's IP and port.
 */
typedef void (*Com_udp_on_rx_t)(Com_udp* obj, const uint8_t* data, uint32_t len,
                                uint8_t flags, void* arg);

typedef struct {
    Wiz5500* wiz;               /**< Wiz5500 instance from W5500_Create()          */

    /* ---- network --------------------------------------------------- */
    uint8_t  mac[6];
    uint8_t  ip[4];
    uint8_t  sn[4];
    uint8_t  gw[4];

    /* ---- socket ---------------------------------------------------- */
    uint8_t  sock_n;            /**< W5500 socket number (0-7)                  */
    uint16_t local_port;        /**< bind port                                  */
    uint16_t rx_buf_size;       /**< RX payload buffer (bytes)                  */
    uint16_t tx_buf_size;       /**< TX payload buffer (bytes)                  */

    /* ---- callback -------------------------------------------------- */
    Com_udp_on_rx_t on_rx;
    void*           on_rx_arg;
} Com_udp_config;

struct Com_udp_t {
    Com_udp_config config;
    uint8_t*        rx_buf;
    uint8_t*        tx_buf;
    uint8_t         last_src_ip[4];
    uint16_t        last_src_port;
};

/* ------------------------------------------------------------------ */
/* Public API  (mirrors com_uart)                                      */
/* ------------------------------------------------------------------ */

void      Com_Udp_Init(void);
Com_udp*  Com_Udp_Create(const Com_udp_config* cfg);
void      Com_Udp_Poll(void);
void      Com_Udp_Send(Com_udp* obj, const uint8_t* data, uint32_t len,
                       const uint8_t* dest_ip, uint16_t dest_port);
void      Com_Udp_Get_Src(Com_udp* obj, uint8_t* out_ip, uint16_t* out_port);
