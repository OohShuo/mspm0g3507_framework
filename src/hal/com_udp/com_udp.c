#include "com_udp.h"

#if FRAMEWORK_USE_WIZNET

    #include <stddef.h>
    #include <string.h>

    #include "FreeRTOS.h"
    #include "freertos_alloc.h"
    #include "protocol.h"
    #include "socket.h"
    #include "vector.h"
    #include "wizchip_conf.h"

/* ------------------------------------------------------------------ */
/* Subsystem state                                                     */
/* ------------------------------------------------------------------ */

static uint8_t g_wizchip_ready = 0;
static Vector* g_instances = NULL;

    #define COM_UDP_SOCKET_COUNT 8u

static void force_socket_closed(uint8_t sn) {
    setSn_CR(sn, Sn_CR_CLOSE);
    while (getSn_CR(sn) != 0u) { ; }
    setSn_IR(sn, 0xFFu);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void Com_Udp_Init(void) {
    if (g_instances != NULL) return;
    g_instances = Vector_Init(sizeof(Com_udp*), COM_UDP_SOCKET_COUNT);
}

Com_udp* Com_Udp_Create(const Com_udp_config* cfg) {
    if (cfg == NULL || g_instances == NULL || cfg->wiz == NULL) return NULL;
    if (cfg->sock_n >= COM_UDP_SOCKET_COUNT || cfg->rx_buf_size == 0 || cfg->tx_buf_size == 0) {
        return NULL;
    }
    if (Vector_Get_Size(g_instances) >= COM_UDP_SOCKET_COUNT) return NULL;

    for (uint32_t i = 0; i < Vector_Get_Size(g_instances); i++) {
        Com_udp* existing = *(Com_udp**)Vector_Get_At(g_instances, i);
        if (existing != NULL && existing->config.sock_n == cfg->sock_n) return NULL;
    }

    /* ---- one-time wizchip init (shared by all instances) ---- */
    if (!g_wizchip_ready) {
        W5500_Reset(cfg->wiz);

        uint8_t tx[8] = {2, 2, 2, 2, 2, 2, 2, 2};
        uint8_t rx[8] = {2, 2, 2, 2, 2, 2, 2, 2};
        if (wizchip_init(tx, rx) != 0) return NULL;

        wiz_NetInfo net;
        memcpy(net.mac, cfg->mac, 6);
        memcpy(net.ip, cfg->ip, 4);
        memcpy(net.sn, cfg->sn, 4);
        memcpy(net.gw, cfg->gw, 4);
        net.dns[0] = 8;
        net.dns[1] = 8;
        net.dns[2] = 8;
        net.dns[3] = 8;
        net.dhcp = NETINFO_STATIC;
        ctlnetwork(CN_SET_NETINFO, (void*)&net);

        g_wizchip_ready = 1;
    }

    /* ---- allocate struct + buffers ---- */
    Com_udp* obj = (Com_udp*)pvPortMalloc(sizeof(Com_udp));
    if (obj == NULL) return NULL;
    memset(obj, 0, sizeof(Com_udp));
    obj->config = *cfg;

    obj->rx_buf = (uint8_t*)pvPortMalloc(cfg->rx_buf_size);
    if (obj->rx_buf == NULL) {
        vPortFree(obj);
        return NULL;
    }

    obj->tx_buf = (uint8_t*)pvPortMalloc(cfg->tx_buf_size);
    if (obj->tx_buf == NULL) {
        vPortFree(obj->rx_buf);
        vPortFree(obj);
        return NULL;
    }

    int8_t ret = socket(cfg->sock_n, Sn_MR_UDP, cfg->local_port, 0);
    if (ret != (int8_t)cfg->sock_n) {
        vPortFree(obj->tx_buf);
        vPortFree(obj->rx_buf);
        vPortFree(obj);
        return NULL;
    }

    Vector_Push_Back(g_instances, (void*)&obj);
    return obj;
}

void Com_Udp_Poll(void) {
    if (g_instances == NULL) return;

    uint32_t n = Vector_Get_Size(g_instances);
    for (uint32_t i = 0; i < n; i++) {
        Com_udp* obj = *(Com_udp**)Vector_Get_At(g_instances, i);
        if (obj == NULL) continue;

        uint8_t sn = obj->config.sock_n;

        switch (getSn_SR(sn)) {
            case SOCK_UDP: {
                uint16_t avail = getSn_RX_RSR(sn);
                if (avail == 0) break;
                if (avail > obj->config.rx_buf_size) avail = obj->config.rx_buf_size;

                int32_t rlen = recvfrom(sn, obj->rx_buf, avail, obj->last_src_ip, &obj->last_src_port);
                if (rlen <= 0) {
                    if (rlen < 0) {
                        force_socket_closed(sn);
                        obj->rx_in_progress = 0;
                    }
                    break;
                }

                uint8_t pack_info = PACK_COMPLETED;
                if (getsockopt(sn, SO_PACKINFO, &pack_info) != SOCK_OK) {
                    force_socket_closed(sn);
                    obj->rx_in_progress = 0;
                    break;
                }

                if (obj->config.on_rx) {
                    uint8_t flags = 0;
                    if (!obj->rx_in_progress) flags |= PROTOCOL_CHUNK_FIRST;
                    if ((pack_info & PACK_REMAINED) == 0u) flags |= PROTOCOL_CHUNK_LAST;
                    obj->config.on_rx(obj, obj->rx_buf, (uint32_t)rlen, flags, obj->config.on_rx_arg);
                }
                obj->rx_in_progress = (uint8_t)((pack_info & PACK_REMAINED) != 0u);
                break;
            }
            case SOCK_CLOSED:
                obj->rx_in_progress = 0;
                socket(sn, Sn_MR_UDP, obj->config.local_port, 0);
                break;
            default:
                break;
        }
    }
}

int32_t Com_Udp_Send(
    Com_udp* obj, const uint8_t* data, uint32_t len, const uint8_t* dest_ip, uint16_t dest_port) {
    if (obj == NULL || data == NULL || dest_ip == NULL) return SOCKERR_ARG;
    if (len == 0 || len > obj->config.tx_buf_size || len > UINT16_MAX) return SOCKERR_DATALEN;
    if (getSn_SR(obj->config.sock_n) != SOCK_UDP) return SOCKERR_SOCKSTATUS;

    memcpy(obj->tx_buf, data, len);
    return sendto(obj->config.sock_n, obj->tx_buf, (uint16_t)len, (uint8_t*)dest_ip, dest_port);
}

void Com_Udp_Get_Src(Com_udp* obj, uint8_t* out_ip, uint16_t* out_port) {
    if (obj == NULL) return;
    if (out_ip) memcpy(out_ip, obj->last_src_ip, 4);
    if (out_port) *out_port = obj->last_src_port;
}

#endif
