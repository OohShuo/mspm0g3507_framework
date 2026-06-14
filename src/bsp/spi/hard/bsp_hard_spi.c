#include <stdlib.h>

#include "board_config.h"
#include "bsp_spi.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "dl_dma.h"
#include "dl_spi.h"
#include "vector.h"

#define SPI_RX_SCRATCH_SIZE 512

#if SPI_NUM

static const uint8_t s_spi_dummy_tx[SPI_RX_SCRATCH_SIZE] = {
    [0 ... SPI_RX_SCRATCH_SIZE - 1] = 0xFF,
};

struct Bsp_spi_instance_t {
    SPI_Regs* inst;
    uint32_t dma_tx_channel;
    uint32_t dma_rx_channel;
    IRQn_Type int_irqn;
    struct {
        Vector* cb_vec;
        Vector* cb_arg_vec;
    } tx_dma_done, tx_done, rx_dma_done, idle;
    uint8_t rx_scratch[SPI_RX_SCRATCH_SIZE];
};

static struct Bsp_spi_instance_t bsp_spi_instances[SPI_NUM] = {0};

void Bsp_Hard_Spi_Init(void) {
    for (uint32_t i = 0; i < SPI_NUM; i++) {
        bsp_spi_instances[i].inst = ((SPI_Regs*[])SPI_PORTS)[i];
        bsp_spi_instances[i].dma_tx_channel = ((uint32_t[])SPI_DMA_TX_CHANNELS)[i];
        bsp_spi_instances[i].dma_rx_channel = ((uint32_t[])SPI_DMA_RX_CHANNELS)[i];
        bsp_spi_instances[i].int_irqn = ((IRQn_Type[])SPI_INT_IRQNS)[i];
        DL_SPI_setBitRateSerialClockDivider(
            bsp_spi_instances[i].inst, ((uint32_t[])SPI_CLOCK_DIVIDERS)[i]);
        bsp_spi_instances[i].tx_dma_done.cb_vec = Vector_Init(sizeof(Bsp_spi_tx_dma_done_cb_t), 1);
        bsp_spi_instances[i].tx_dma_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_spi_instances[i].tx_done.cb_vec = Vector_Init(sizeof(Bsp_spi_tx_done_cb_t), 1);
        bsp_spi_instances[i].tx_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_spi_instances[i].rx_dma_done.cb_vec = Vector_Init(sizeof(Bsp_spi_rx_dma_done_cb_t), 1);
        bsp_spi_instances[i].rx_dma_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_spi_instances[i].idle.cb_vec = Vector_Init(sizeof(Bsp_spi_idle_cb_t), 1);
        bsp_spi_instances[i].idle.cb_arg_vec = Vector_Init(sizeof(void*), 1);

        DL_DMA_enableChannel(DMA, bsp_spi_instances[i].dma_tx_channel);
        DL_DMA_enableChannel(DMA, bsp_spi_instances[i].dma_rx_channel);
        NVIC_EnableIRQ(bsp_spi_instances[i].int_irqn);
    }
}

void Bsp_Hard_Spi_Write(uint32_t idx, const uint8_t* data, uint32_t len) {
    if (idx >= SPI_NUM) return;

    if (len > SPI_RX_SCRATCH_SIZE) { len = SPI_RX_SCRATCH_SIZE; }

    DL_DMA_disableChannel(DMA, bsp_spi_instances[idx].dma_rx_channel);
    DL_DMA_disableChannel(DMA, bsp_spi_instances[idx].dma_tx_channel);

    DL_DMA_setSrcAddr(
        DMA, bsp_spi_instances[idx].dma_rx_channel, (uint32_t)(&bsp_spi_instances[idx].inst->RXDATA));
    DL_DMA_setDestAddr(
        DMA, bsp_spi_instances[idx].dma_rx_channel, (uint32_t)bsp_spi_instances[idx].rx_scratch);
    DL_DMA_setTransferSize(DMA, bsp_spi_instances[idx].dma_rx_channel, len);

    DL_DMA_setSrcAddr(DMA, bsp_spi_instances[idx].dma_tx_channel, (uint32_t)data);
    DL_DMA_setDestAddr(
        DMA, bsp_spi_instances[idx].dma_tx_channel, (uint32_t)(&bsp_spi_instances[idx].inst->TXDATA));
    DL_DMA_setTransferSize(DMA, bsp_spi_instances[idx].dma_tx_channel, len);

    DL_DMA_enableChannel(DMA, bsp_spi_instances[idx].dma_rx_channel);
    DL_DMA_enableChannel(DMA, bsp_spi_instances[idx].dma_tx_channel);
}

void Bsp_Hard_Spi_Read(uint32_t idx, uint8_t* data, uint32_t len) {
    if (idx >= SPI_NUM) return;

    if (len > SPI_RX_SCRATCH_SIZE) { len = SPI_RX_SCRATCH_SIZE; }

    DL_DMA_disableChannel(DMA, bsp_spi_instances[idx].dma_rx_channel);
    DL_DMA_disableChannel(DMA, bsp_spi_instances[idx].dma_tx_channel);

    DL_DMA_setSrcAddr(DMA, bsp_spi_instances[idx].dma_tx_channel, (uint32_t)s_spi_dummy_tx);
    DL_DMA_setDestAddr(
        DMA, bsp_spi_instances[idx].dma_tx_channel, (uint32_t)(&bsp_spi_instances[idx].inst->TXDATA));
    DL_DMA_setTransferSize(DMA, bsp_spi_instances[idx].dma_tx_channel, len);

    DL_DMA_setSrcAddr(
        DMA, bsp_spi_instances[idx].dma_rx_channel, (uint32_t)(&bsp_spi_instances[idx].inst->RXDATA));
    DL_DMA_setDestAddr(DMA, bsp_spi_instances[idx].dma_rx_channel, (uint32_t)data);
    DL_DMA_setTransferSize(DMA, bsp_spi_instances[idx].dma_rx_channel, len);

    DL_DMA_enableChannel(DMA, bsp_spi_instances[idx].dma_rx_channel);
    DL_DMA_enableChannel(DMA, bsp_spi_instances[idx].dma_tx_channel);
}

static void busy_wait_for_complete(uint32_t idx) {
    if (idx >= SPI_NUM) { return; }
    struct Bsp_spi_instance_t* spi = &bsp_spi_instances[idx];

    while (DL_DMA_getTransferSize(DMA, spi->dma_tx_channel) != 0u ||
           DL_DMA_getTransferSize(DMA, spi->dma_rx_channel) != 0u) {
        __NOP();
    }

    SPI_Regs* inst = spi->inst;
    while (!DL_SPI_isTXFIFOEmpty(inst)) { __NOP(); }
    while (DL_SPI_isBusy(inst)) { __NOP(); }
}

void Bsp_Hard_Spi_Wait_For_Complete(uint32_t idx) { busy_wait_for_complete(idx); }

void Bsp_Hard_Spi_Write_Blocking(uint32_t idx, const uint8_t* data, uint32_t len) {
    Bsp_Hard_Spi_Write(idx, data, len);
    Bsp_Hard_Spi_Wait_For_Complete(idx);
}

void Bsp_Hard_Spi_Read_Blocking(uint32_t idx, uint8_t* data, uint32_t len) {
    Bsp_Hard_Spi_Read(idx, data, len);
    Bsp_Hard_Spi_Wait_For_Complete(idx);
}

void Bsp_Hard_Spi_Register_Tx_Dma_Done_Cb(uint32_t idx, Bsp_spi_tx_dma_done_cb_t cb, void* cb_arg) {
    if (idx >= SPI_NUM || cb == NULL) return;

    Vector_Push_Back(bsp_spi_instances[idx].tx_dma_done.cb_vec, (void*)&cb);
    Vector_Push_Back(bsp_spi_instances[idx].tx_dma_done.cb_arg_vec, (void*)&cb_arg);
}

void Bsp_Hard_Spi_Register_Tx_Done_Cb(uint32_t idx, Bsp_spi_tx_done_cb_t cb, void* cb_arg) {
    if (idx >= SPI_NUM || cb == NULL) return;

    Vector_Push_Back(bsp_spi_instances[idx].tx_done.cb_vec, (void*)&cb);
    Vector_Push_Back(bsp_spi_instances[idx].tx_done.cb_arg_vec, (void*)&cb_arg);
}

void Bsp_Hard_Spi_Register_Rx_Dma_Done_Cb(uint32_t idx, Bsp_spi_rx_dma_done_cb_t cb, void* cb_arg) {
    if (idx >= SPI_NUM || cb == NULL) return;

    Vector_Push_Back(bsp_spi_instances[idx].rx_dma_done.cb_vec, (void*)&cb);
    Vector_Push_Back(bsp_spi_instances[idx].rx_dma_done.cb_arg_vec, (void*)&cb_arg);
}

void Bsp_Hard_Spi_Irq_Handler(SPI_Regs* spi_inst) {
    for (uint32_t i = 0; i < SPI_NUM; i++) {
        struct Bsp_spi_instance_t* spi = &bsp_spi_instances[i];

        if (spi->inst != spi_inst) continue;

        switch (DL_SPI_getPendingInterrupt(spi->inst)) {
            case DL_SPI_IIDX_DMA_DONE_TX:
                for (uint32_t j = 0; j < Vector_Get_Size(spi->tx_dma_done.cb_vec); j++) {
                    Bsp_spi_tx_dma_done_cb_t cb =
                        *(Bsp_spi_tx_dma_done_cb_t*)Vector_Get_At(spi->tx_dma_done.cb_vec, j);
                    void* cb_arg = *(void**)Vector_Get_At(spi->tx_dma_done.cb_arg_vec, j);
                    cb(cb_arg);
                }
                break;
            case DL_SPI_IIDX_TX_EMPTY:
                for (uint32_t j = 0; j < Vector_Get_Size(spi->tx_done.cb_vec); j++) {
                    Bsp_spi_tx_done_cb_t cb = *(Bsp_spi_tx_done_cb_t*)Vector_Get_At(spi->tx_done.cb_vec, j);
                    void* cb_arg = *(void**)Vector_Get_At(spi->tx_done.cb_arg_vec, j);
                    cb(cb_arg);
                }
                break;
            case DL_SPI_IIDX_DMA_DONE_RX:
                for (uint32_t j = 0; j < Vector_Get_Size(spi->rx_dma_done.cb_vec); j++) {
                    Bsp_spi_rx_dma_done_cb_t cb =
                        *(Bsp_spi_rx_dma_done_cb_t*)Vector_Get_At(spi->rx_dma_done.cb_vec, j);
                    void* cb_arg = *(void**)Vector_Get_At(spi->rx_dma_done.cb_arg_vec, j);
                    cb(cb_arg);
                }
                break;
            case DL_SPI_IIDX_IDLE:
                for (uint32_t j = 0; j < Vector_Get_Size(spi->idle.cb_vec); j++) {
                    Bsp_spi_idle_cb_t cb = *(Bsp_spi_idle_cb_t*)Vector_Get_At(spi->idle.cb_vec, j);
                    void* cb_arg = *(void**)Vector_Get_At(spi->idle.cb_arg_vec, j);
                    cb(cb_arg);
                }
                break;
            default:
                break;
        }
    }
}

#else

void Bsp_Hard_Spi_Init(void) {}

void Bsp_Hard_Spi_Wait_For_Complete(uint32_t idx) { (void)idx; }

void Bsp_Hard_Spi_Write(uint32_t idx, const uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Hard_Spi_Read(uint32_t idx, uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Hard_Spi_Write_Blocking(uint32_t idx, const uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Hard_Spi_Read_Blocking(uint32_t idx, uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Hard_Spi_Register_Tx_Dma_Done_Cb(uint32_t idx, Bsp_spi_tx_dma_done_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}

void Bsp_Hard_Spi_Register_Tx_Done_Cb(uint32_t idx, Bsp_spi_tx_done_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}

void Bsp_Hard_Spi_Register_Rx_Dma_Done_Cb(uint32_t idx, Bsp_spi_rx_dma_done_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}

void Bsp_Hard_Spi_Register_Idle_Cb(uint32_t idx, Bsp_spi_idle_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}

void Bsp_Hard_Spi_Irq_Handler(SPI_Regs* spi_inst) { (void)spi_inst; }

#endif
