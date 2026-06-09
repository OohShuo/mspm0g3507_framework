#include "devices/msp/m0p/mspm0g350x.h"
// chip header must come before bsp_uart.h — the latter pulls in
// hw_uart.h → driverlib.h, which processes dl_uart_main/dl_uart.h
// before anything else gets a chance to define __MSPM0_HAS_UART_MAIN__.
// Without this ordering, dl_uart.h's body is gated to empty and the
// typedefs in dl_uart_main.h fail with "unknown type name
// 'DL_UART_Config'".

#include <stddef.h>

#include "board_config.h"
#include "bsp_uart.h"
#include "dl_dma.h"
#include "dl_uart_main.h"
#include "vector.h"

#if UART_NUM

struct Bsp_uart_instance_t {
    UART_Regs* inst;
    uint32_t dma_tx_channel;
    uint32_t dma_rx_channel;
    IRQn_Type int_irqn;
    struct {
        Vector* cb_vec;
        Vector* cb_arg_vec;
    } tx_dma_done, tx_done, rx_dma_done;

    // Set true by Write/Read, cleared by the matching EOT_DONE /
    // DMA_DONE_RX in the ISR. Wait_For_Complete spins on these so
    // _Blocking variants don't need a separate counter or completion
    // queue.
    volatile bool tx_in_progress;
    volatile bool rx_in_progress;

    // Continuous RX: when active, the ISR re-arms the next DMA
    // reception with the same (buf, len) after every DMA_DONE_RX.
    // The callback fires on every completion so the caller can
    // consume the buffer before the next byte arrives. Stop with
    // Bsp_Uart_Stop_Continuous_Rx, or implicitly by calling
    // Bsp_Uart_Read (which clears continuous_rx_active).
    volatile bool continuous_rx_active;
    uint8_t* continuous_rx_buf;
    uint32_t continuous_rx_len;
};

static struct Bsp_uart_instance_t bsp_uart_instances[UART_NUM] = {0};

void Bsp_Uart_Init(void) {
    for (uint32_t i = 0; i < UART_NUM; i++) {
        bsp_uart_instances[i].inst = ((UART_Regs*[])UART_PORTS)[i];
        bsp_uart_instances[i].dma_tx_channel = ((uint32_t[])UART_DMA_TX_CHANNELS)[i];
        bsp_uart_instances[i].dma_rx_channel = ((uint32_t[])UART_DMA_RX_CHANNELS)[i];
        bsp_uart_instances[i].int_irqn = ((IRQn_Type[])UART_INT_IRQNS)[i];
        bsp_uart_instances[i].tx_dma_done.cb_vec = Vector_Init(sizeof(Bsp_uart_tx_dma_done_cb_t), 1);
        bsp_uart_instances[i].tx_dma_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_uart_instances[i].tx_done.cb_vec = Vector_Init(sizeof(Bsp_uart_tx_done_cb_t), 1);
        bsp_uart_instances[i].tx_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_uart_instances[i].rx_dma_done.cb_vec = Vector_Init(sizeof(Bsp_uart_rx_dma_done_cb_t), 1);
        bsp_uart_instances[i].rx_dma_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);

        DL_DMA_enableChannel(DMA, bsp_uart_instances[i].dma_tx_channel);
        DL_DMA_enableChannel(DMA, bsp_uart_instances[i].dma_rx_channel);
        NVIC_EnableIRQ(bsp_uart_instances[i].int_irqn);
    }
}

void Bsp_Uart_Write(uint32_t idx, const uint8_t* data, uint32_t len) {
    if (idx >= UART_NUM) { return; }
    struct Bsp_uart_instance_t* u = &bsp_uart_instances[idx];

    DL_DMA_setSrcAddr(DMA, u->dma_tx_channel, (uint32_t)data);
    DL_DMA_setDestAddr(DMA, u->dma_tx_channel, (uint32_t)(&u->inst->TXDATA));
    DL_DMA_setTransferSize(DMA, u->dma_tx_channel, len);

    // Set the flag before enabling the channel so a fast EOT_DONE
    // (e.g. zero-length transfer) can't race past Wait_For_Complete.
    u->tx_in_progress = true;
    DL_DMA_enableChannel(DMA, u->dma_tx_channel);
}

void Bsp_Uart_Read(uint32_t idx, uint8_t* data, uint32_t len) {
    if (idx >= UART_NUM) { return; }
    struct Bsp_uart_instance_t* u = &bsp_uart_instances[idx];

    // A one-shot Read implicitly cancels continuous mode. Otherwise
    // a read issued after Start_Continuous_Rx would, on its
    // completion, get silently re-armed using the continuous buffer
    // (not the one passed here) — surprising.
    u->continuous_rx_active = false;

    DL_DMA_setSrcAddr(DMA, u->dma_rx_channel, (uint32_t)(&u->inst->RXDATA));
    DL_DMA_setDestAddr(DMA, u->dma_rx_channel, (uint32_t)data);
    DL_DMA_setTransferSize(DMA, u->dma_rx_channel, len);

    u->rx_in_progress = true;
    DL_DMA_enableChannel(DMA, u->dma_rx_channel);
}

void Bsp_Uart_Start_Continuous_Rx(uint32_t idx, uint8_t* data, uint32_t len) {
    if (idx >= UART_NUM) { return; }
    struct Bsp_uart_instance_t* u = &bsp_uart_instances[idx];

    // Latch the buffer + length so the ISR can re-arm with the same
    // (data, len) after every DMA_DONE_RX. Then kick off the first
    // reception — the same code path as Bsp_Uart_Read but without
    // clearing continuous_rx_active (we just set it).
    u->continuous_rx_buf = data;
    u->continuous_rx_len = len;
    u->continuous_rx_active = true;

    DL_DMA_setSrcAddr(DMA, u->dma_rx_channel, (uint32_t)(&u->inst->RXDATA));
    DL_DMA_setDestAddr(DMA, u->dma_rx_channel, (uint32_t)data);
    DL_DMA_setTransferSize(DMA, u->dma_rx_channel, len);

    u->rx_in_progress = true;
    DL_DMA_enableChannel(DMA, u->dma_rx_channel);
}

void Bsp_Uart_Stop_Continuous_Rx(uint32_t idx) {
    if (idx >= UART_NUM) { return; }
    bsp_uart_instances[idx].continuous_rx_active = false;
    // The in-flight DMA still completes — its DMA_DONE_RX fires the
    // callback one last time and the ISR sees the flag is now false,
    // so no re-arm. No need to touch the DMA channel itself.
}

void Bsp_Uart_Wait_For_Complete(uint32_t idx) {
    if (idx >= UART_NUM) { return; }
    struct Bsp_uart_instance_t* u = &bsp_uart_instances[idx];
    while (u->tx_in_progress || u->rx_in_progress) { __NOP(); }
}

void Bsp_Uart_Write_Blocking(uint32_t idx, const uint8_t* data, uint32_t len) {
    Bsp_Uart_Write(idx, data, len);
    Bsp_Uart_Wait_For_Complete(idx);
}

void Bsp_Uart_Read_Blocking(uint32_t idx, uint8_t* data, uint32_t len) {
    Bsp_Uart_Read(idx, data, len);
    Bsp_Uart_Wait_For_Complete(idx);
}

void Bsp_Uart_Register_Tx_Dma_Done_Cb(uint32_t idx, Bsp_uart_tx_dma_done_cb_t cb, void* cb_arg) {
    if (idx >= UART_NUM || cb == NULL) { return; }
    Vector_Push_Back(bsp_uart_instances[idx].tx_dma_done.cb_vec, (void*)&cb);
    Vector_Push_Back(bsp_uart_instances[idx].tx_dma_done.cb_arg_vec, (void*)&cb_arg);
}

void Bsp_Uart_Register_Tx_Done_Cb(uint32_t idx, Bsp_uart_tx_done_cb_t cb, void* cb_arg) {
    if (idx >= UART_NUM || cb == NULL) { return; }
    Vector_Push_Back(bsp_uart_instances[idx].tx_done.cb_vec, (void*)&cb);
    Vector_Push_Back(bsp_uart_instances[idx].tx_done.cb_arg_vec, (void*)&cb_arg);
}

void Bsp_Uart_Register_Rx_Dma_Done_Cb(uint32_t idx, Bsp_uart_rx_dma_done_cb_t cb, void* cb_arg) {
    if (idx >= UART_NUM || cb == NULL) { return; }
    Vector_Push_Back(bsp_uart_instances[idx].rx_dma_done.cb_vec, (void*)&cb);
    Vector_Push_Back(bsp_uart_instances[idx].rx_dma_done.cb_arg_vec, (void*)&cb_arg);
}

void Bsp_Uart_Irq_Handler(UART_Regs* uart_inst) {
    for (uint32_t i = 0; i < UART_NUM; i++) {
        struct Bsp_uart_instance_t* u = &bsp_uart_instances[i];
        if (u->inst != uart_inst) { continue; }

        switch (DL_UART_Main_getPendingInterrupt(u->inst)) {
            case DL_UART_MAIN_IIDX_DMA_DONE_TX:
                for (uint32_t j = 0; j < Vector_Get_Size(u->tx_dma_done.cb_vec); j++) {
                    Bsp_uart_tx_dma_done_cb_t cb =
                        *(Bsp_uart_tx_dma_done_cb_t*)Vector_Get_At(u->tx_dma_done.cb_vec, j);
                    void* cb_arg = *(void**)Vector_Get_At(u->tx_dma_done.cb_arg_vec, j);
                    cb(cb_arg);
                }
                break;
            case DL_UART_MAIN_IIDX_EOT_DONE:
                // EOT_DONE is the "TX fully drained" event: DMA done
                // AND TX FIFO empty. This is what _Blocking should
                // wait on, not DMA_DONE_TX.
                u->tx_in_progress = false;
                for (uint32_t j = 0; j < Vector_Get_Size(u->tx_done.cb_vec); j++) {
                    Bsp_uart_tx_done_cb_t cb = *(Bsp_uart_tx_done_cb_t*)Vector_Get_At(u->tx_done.cb_vec, j);
                    void* cb_arg = *(void**)Vector_Get_At(u->tx_done.cb_arg_vec, j);
                    cb(cb_arg);
                }
                break;
            case DL_UART_MAIN_IIDX_DMA_DONE_RX: {
                u->rx_in_progress = false;
                for (uint32_t j = 0; j < Vector_Get_Size(u->rx_dma_done.cb_vec); j++) {
                    Bsp_uart_rx_dma_done_cb_t cb =
                        *(Bsp_uart_rx_dma_done_cb_t*)Vector_Get_At(u->rx_dma_done.cb_vec, j);
                    void* cb_arg = *(void**)Vector_Get_At(u->rx_dma_done.cb_arg_vec, j);
                    cb(cb_arg);
                }
                // Re-arm for the next reception if continuous mode is
                // still active. The flag may have been cleared by the
                // callback itself (e.g. it called Stop) so we
                // re-check after firing. Configure + enable, just
                // like Bsp_Uart_Read.
                if (u->continuous_rx_active) {
                    DL_DMA_setSrcAddr(DMA, u->dma_rx_channel, (uint32_t)(&u->inst->RXDATA));
                    DL_DMA_setDestAddr(DMA, u->dma_rx_channel, (uint32_t)u->continuous_rx_buf);
                    DL_DMA_setTransferSize(DMA, u->dma_rx_channel, u->continuous_rx_len);

                    u->rx_in_progress = true;
                    DL_DMA_enableChannel(DMA, u->dma_rx_channel);
                }
                break;
            }
            default:
                break;
        }
    }
}

#else  // UART_NUM == 0

void Bsp_Uart_Init(void) {}

void Bsp_Uart_Wait_For_Complete(uint32_t idx) { (void)idx; }

void Bsp_Uart_Write(uint32_t idx, const uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Uart_Read(uint32_t idx, uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Uart_Start_Continuous_Rx(uint32_t idx, uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Uart_Stop_Continuous_Rx(uint32_t idx) { (void)idx; }

void Bsp_Uart_Write_Blocking(uint32_t idx, const uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Uart_Read_Blocking(uint32_t idx, uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Uart_Register_Tx_Dma_Done_Cb(uint32_t idx, Bsp_uart_tx_dma_done_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}

void Bsp_Uart_Register_Tx_Done_Cb(uint32_t idx, Bsp_uart_tx_done_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}

void Bsp_Uart_Register_Rx_Dma_Done_Cb(uint32_t idx, Bsp_uart_rx_dma_done_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}

void Bsp_Uart_Irq_Handler(UART_Regs* uart_inst) { (void)uart_inst; }

#endif
