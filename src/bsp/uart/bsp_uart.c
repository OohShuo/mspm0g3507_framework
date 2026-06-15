#include "bsp_uart.h"

#include <stddef.h>
#include <string.h>

#include "board_config.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "dl_dma.h"
#include "dl_timera.h"
#include "dl_timerg.h"
#include "dl_uart_main.h"
#include "rtt_log.h"
#include "ti_msp_dl_config.h"
#include "vector.h"

#if UART_NUM

    #if UART_0_DMA_TX_CHANNEL != DMA_CH4_CHAN_ID
        #error "UART0 TX DMA channel does not match the SysConfig-generated channel"
    #endif

    #if UART_0_DMA_RX_CHANNEL != DMA_CH3_CHAN_ID
        #error "UART0 RX DMA channel does not match the SysConfig-generated channel"
    #endif

struct Bsp_uart_instance_t {
    UART_Regs* inst;
    uint32_t dma_tx_channel;
    uint32_t dma_rx_channel;
    IRQn_Type int_irqn;
    GPTIMER_Regs* idle_timer;
    IRQn_Type idle_timer_irqn;
    uint32_t idle_timer_clock_freq;
    struct {
        Vector* cb_vec;
        Vector* cb_arg_vec;
    } tx_dma_done, tx_done, rx_dma_done, rx_idle;

    volatile uint8_t tx_in_progress;
    volatile uint8_t rx_in_progress;
    volatile uint8_t continuous_rx_active;
    uint8_t continuous_rx_in_progress;
    uint32_t continuous_rx_len;
    uint8_t* continuous_rx_dest_buf;
    uint32_t continuous_rx_max_len;

    uint8_t continuous_rx_buf[BSP_UART_CONTINUOUS_RX_BUF_SIZE];
};

static struct Bsp_uart_instance_t bsp_uart_instances[UART_NUM] = {0};

void Bsp_Uart_Init(void) {
    for (uint32_t i = 0; i < UART_NUM; i++) {
        bsp_uart_instances[i].inst = ((UART_Regs*[])UART_PORTS)[i];
        bsp_uart_instances[i].dma_tx_channel = ((uint32_t[])UART_DMA_TX_CHANNELS)[i];
        bsp_uart_instances[i].dma_rx_channel = ((uint32_t[])UART_DMA_RX_CHANNELS)[i];
        bsp_uart_instances[i].int_irqn = ((IRQn_Type[])UART_INT_IRQNS)[i];
        bsp_uart_instances[i].idle_timer = ((GPTIMER_Regs*[])UART_IDLE_TIMERS)[i];
        bsp_uart_instances[i].idle_timer_irqn = ((IRQn_Type[])UART_IDLE_TIMER_IRQNS)[i];
        bsp_uart_instances[i].idle_timer_clock_freq = ((uint32_t[])UART_IDLE_TIMER_CLOCK_FREQS)[i];
        bsp_uart_instances[i].tx_dma_done.cb_vec = Vector_Init(sizeof(Bsp_uart_tx_dma_done_cb_t), 1);
        bsp_uart_instances[i].tx_dma_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_uart_instances[i].tx_done.cb_vec = Vector_Init(sizeof(Bsp_uart_tx_done_cb_t), 1);
        bsp_uart_instances[i].tx_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_uart_instances[i].rx_dma_done.cb_vec = Vector_Init(sizeof(Bsp_uart_rx_dma_done_cb_t), 1);
        bsp_uart_instances[i].rx_dma_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_uart_instances[i].rx_idle.cb_vec = Vector_Init(sizeof(Bsp_uart_rx_idle_cb_t), 1);
        bsp_uart_instances[i].rx_idle.cb_arg_vec = Vector_Init(sizeof(void*), 1);

        DL_DMA_enableChannel(DMA, bsp_uart_instances[i].dma_tx_channel);
        DL_DMA_enableChannel(DMA, bsp_uart_instances[i].dma_rx_channel);
        NVIC_EnableIRQ(bsp_uart_instances[i].int_irqn);
        NVIC_EnableIRQ(bsp_uart_instances[i].idle_timer_irqn);
    }
}

void Bsp_Uart_Write(uint32_t idx, const uint8_t* data, uint32_t len) {
    if (idx >= UART_NUM) { return; }
    struct Bsp_uart_instance_t* u = &bsp_uart_instances[idx];

    uint32_t spin = 1000000U;
    while (DL_UART_Main_isBusy(u->inst) != 0U && spin--) { __NOP(); }

    DL_DMA_disableChannel(DMA, u->dma_tx_channel);
    DL_DMA_setSrcAddr(DMA, u->dma_tx_channel, (uint32_t)data);
    DL_DMA_setDestAddr(DMA, u->dma_tx_channel, (uint32_t)(&u->inst->TXDATA));
    DL_DMA_setTransferSize(DMA, u->dma_tx_channel, len);

    u->tx_in_progress = 1;
    DL_DMA_enableChannel(DMA, u->dma_tx_channel);
}

void Bsp_Uart_Read(uint32_t idx, uint8_t* data, uint32_t len) {
    if (idx >= UART_NUM) { return; }
    struct Bsp_uart_instance_t* u = &bsp_uart_instances[idx];

    u->continuous_rx_active = 0;

    DL_DMA_setSrcAddr(DMA, u->dma_rx_channel, (uint32_t)(&u->inst->RXDATA));
    DL_DMA_setDestAddr(DMA, u->dma_rx_channel, (uint32_t)data);
    DL_DMA_setTransferSize(DMA, u->dma_rx_channel, len);

    u->rx_in_progress = 1;
    DL_DMA_enableChannel(DMA, u->dma_rx_channel);
}

void Bsp_Uart_Start_Continuous_Rx(uint32_t idx, uint32_t idle_timeout_ms, uint8_t* buf, uint32_t max_len) {
    if (idx >= UART_NUM) { return; }
    struct Bsp_uart_instance_t* u = &bsp_uart_instances[idx];

    u->continuous_rx_active = 1;
    u->continuous_rx_len = 0;
    u->continuous_rx_in_progress = 0;
    u->continuous_rx_dest_buf = buf;
    u->continuous_rx_max_len = max_len;

    DL_Timer_setLoadValue(u->idle_timer, u->idle_timer_clock_freq / 1000 * idle_timeout_ms - 1);

    DL_DMA_setSrcAddr(DMA, u->dma_rx_channel, (uint32_t)(&u->inst->RXDATA));
    DL_DMA_setDestAddr(DMA, u->dma_rx_channel, (uint32_t)u->continuous_rx_buf);
    DL_DMA_setTransferSize(DMA, u->dma_rx_channel, BSP_UART_CONTINUOUS_RX_BUF_SIZE);

    DL_Timer_startCounter(u->idle_timer);
    DL_DMA_enableChannel(DMA, u->dma_rx_channel);
}

void Bsp_Uart_Stop_Continuous_Rx(uint32_t idx) {
    if (idx >= UART_NUM) { return; }
    bsp_uart_instances[idx].continuous_rx_active = 0;
    bsp_uart_instances[idx].continuous_rx_len = 0;

    DL_DMA_disableChannel(DMA, bsp_uart_instances[idx].dma_rx_channel);
    DL_Timer_stopCounter(bsp_uart_instances[idx].idle_timer);
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

void Bsp_Uart_Register_Rx_Idle_Cb(uint32_t idx, Bsp_uart_rx_idle_cb_t cb, void* cb_arg) {
    if (idx >= UART_NUM || cb == NULL) { return; }
    Vector_Push_Back(bsp_uart_instances[idx].rx_idle.cb_vec, (void*)&cb);
    Vector_Push_Back(bsp_uart_instances[idx].rx_idle.cb_arg_vec, (void*)&cb_arg);
}

void Bsp_Uart_Idle_Irq_Handler(uint32_t idx) {
    if (idx >= UART_NUM) { return; }
    struct Bsp_uart_instance_t* u = &bsp_uart_instances[idx];

    DL_Timer_clearInterruptStatus(u->idle_timer, DL_TIMER_INTERRUPT_ZERO_EVENT);

    if (!u->continuous_rx_active) { return; }

    DL_DMA_disableChannel(DMA, u->dma_rx_channel);

    uint32_t remaining = DL_DMA_getTransferSize(DMA, u->dma_rx_channel);
    uint32_t received =
        (BSP_UART_CONTINUOUS_RX_BUF_SIZE > remaining) ? BSP_UART_CONTINUOUS_RX_BUF_SIZE - remaining : 0;

    if (u->continuous_rx_len + received > u->continuous_rx_max_len) {
        u->continuous_rx_len = 0;
    } else {
        if (received > 0) {
            memcpy(u->continuous_rx_dest_buf + u->continuous_rx_len, u->continuous_rx_buf, received);
            u->continuous_rx_len += received;
        }

        if (u->continuous_rx_len > 0) {
            for (uint32_t j = 0; j < Vector_Get_Size(u->rx_idle.cb_vec); j++) {
                Bsp_uart_rx_idle_cb_t cb = *(Bsp_uart_rx_idle_cb_t*)Vector_Get_At(u->rx_idle.cb_vec, j);
                void* cb_arg = *(void**)Vector_Get_At(u->rx_idle.cb_arg_vec, j);
                cb(idx, u->continuous_rx_dest_buf, u->continuous_rx_len, cb_arg);
            }
        }
        u->continuous_rx_len = 0;
    }

    u->continuous_rx_in_progress = 0;

    DL_DMA_setDestAddr(DMA, u->dma_rx_channel, (uint32_t)u->continuous_rx_buf);
    DL_DMA_setTransferSize(DMA, u->dma_rx_channel, BSP_UART_CONTINUOUS_RX_BUF_SIZE);
    DL_DMA_enableChannel(DMA, u->dma_rx_channel);
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
                u->tx_in_progress = 0;
                for (uint32_t j = 0; j < Vector_Get_Size(u->tx_done.cb_vec); j++) {
                    Bsp_uart_tx_done_cb_t cb = *(Bsp_uart_tx_done_cb_t*)Vector_Get_At(u->tx_done.cb_vec, j);
                    void* cb_arg = *(void**)Vector_Get_At(u->tx_done.cb_arg_vec, j);
                    cb(cb_arg);
                }
                break;
            case DL_UART_MAIN_IIDX_DMA_DONE_RX: {
                u->rx_in_progress = 0;

                uint32_t load_value = DL_Timer_getLoadValue(u->idle_timer);
                DL_Timer_setTimerCount(u->idle_timer, load_value - 1);
                DL_Timer_startCounter(u->idle_timer);

                if (u->continuous_rx_len + BSP_UART_CONTINUOUS_RX_BUF_SIZE > u->continuous_rx_max_len) {
                    u->continuous_rx_len = 0;
                } else {
                    memcpy(u->continuous_rx_dest_buf + u->continuous_rx_len, u->continuous_rx_buf,
                        BSP_UART_CONTINUOUS_RX_BUF_SIZE);
                    u->continuous_rx_len += BSP_UART_CONTINUOUS_RX_BUF_SIZE;
                }

                DL_DMA_disableChannel(DMA, u->dma_rx_channel);
                DL_DMA_setDestAddr(DMA, u->dma_rx_channel, (uint32_t)u->continuous_rx_buf);
                DL_DMA_setTransferSize(DMA, u->dma_rx_channel, BSP_UART_CONTINUOUS_RX_BUF_SIZE);
                DL_DMA_enableChannel(DMA, u->dma_rx_channel);

                for (uint32_t j = 0; j < Vector_Get_Size(u->rx_dma_done.cb_vec); j++) {
                    Bsp_uart_rx_dma_done_cb_t cb =
                        *(Bsp_uart_rx_dma_done_cb_t*)Vector_Get_At(u->rx_dma_done.cb_vec, j);
                    void* cb_arg = *(void**)Vector_Get_At(u->rx_dma_done.cb_arg_vec, j);
                    cb(cb_arg);
                }
                break;
            }
            case DL_UART_MAIN_IIDX_RX:
                if (!u->continuous_rx_in_progress) {
                    u->continuous_rx_in_progress = 1;
                    uint32_t load_value = DL_Timer_getLoadValue(u->idle_timer);
                    DL_Timer_setTimerCount(u->idle_timer, load_value - 1);
                    DL_Timer_startCounter(u->idle_timer);
                }
                break;
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

void Bsp_Uart_Start_Continuous_Rx(uint32_t idx, uint32_t idle_timeout_ms) {
    (void)idx;
    (void)idle_timeout_ms;
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

void Bsp_Uart_Register_Rx_Idle_Cb(uint32_t idx, Bsp_uart_rx_idle_cb_t cb) {
    (void)idx;
    (void)cb;
}

void Bsp_Uart_Irq_Handler(UART_Regs* uart_inst) { (void)uart_inst; }

void Bsp_Uart_Idle_Irq_Handler(uint32_t idx) { (void)idx; }

#endif
