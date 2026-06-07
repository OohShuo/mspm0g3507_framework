#include "bsp_spi.h"

#include <sys/reent.h>

#include "board_config.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "dl_dma.h"
#include "dl_spi.h"
#include "vector.h"

#if SPI_NUM

// Scratch buffer for the RX DMA. We don't care about the received bytes
// (the LCD is a write-only device), but the SPI peripheral still shifts
// in one byte per shifted-out byte. The RX DMA must drain the RX FIFO
// or the SPI will stall mid-transfer.
#define SPI_RX_SCRATCH_SIZE 512

struct Bsp_spi_instance_t {
    SPI_Regs* inst;
    uint32_t dma_tx_channel;
    uint32_t dma_rx_channel;
    IRQn_Type int_irqn;
    struct {
        Vector* cb_vec;
        Vector* cb_arg_vec;
    } tx_dma_done, tx_done, rx_dma_done;
    uint8_t rx_scratch[SPI_RX_SCRATCH_SIZE];
};

static struct Bsp_spi_instance_t bsp_spi_instances[SPI_NUM] = {0};

void Bsp_Spi_Init(void) {
    for (uint32_t i = 0; i < SPI_NUM; i++) {
        bsp_spi_instances[i].inst = ((SPI_Regs*[])SPI_PORTS)[i];
        bsp_spi_instances[i].dma_tx_channel = ((uint32_t[])SPI_DMA_TX_CHANNELS)[i];
        bsp_spi_instances[i].dma_rx_channel = ((uint32_t[])SPI_DMA_RX_CHANNELS)[i];
        bsp_spi_instances[i].int_irqn = ((IRQn_Type[])SPI_INT_IRQNS)[i];
        bsp_spi_instances[i].tx_dma_done.cb_vec = Vector_Init(sizeof(Bsp_spi_tx_dma_done_cb_t), 1);
        bsp_spi_instances[i].tx_dma_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_spi_instances[i].tx_done.cb_vec = Vector_Init(sizeof(Bsp_spi_tx_done_cb_t), 1);
        bsp_spi_instances[i].tx_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);
        bsp_spi_instances[i].rx_dma_done.cb_vec = Vector_Init(sizeof(Bsp_spi_rx_dma_done_cb_t), 1);
        bsp_spi_instances[i].rx_dma_done.cb_arg_vec = Vector_Init(sizeof(void*), 1);

        DL_DMA_enableChannel(DMA, bsp_spi_instances[i].dma_tx_channel);
        DL_DMA_enableChannel(DMA, bsp_spi_instances[i].dma_rx_channel);
        NVIC_EnableIRQ(bsp_spi_instances[i].int_irqn);
    }
}

void Bsp_Spi_Write(uint32_t idx, const uint8_t* data, uint32_t len) {
    if (idx >= SPI_NUM) return;

    if (len > SPI_RX_SCRATCH_SIZE) { len = SPI_RX_SCRATCH_SIZE; }

    // RX DMA must be configured for the same size as the TX DMA. The SPI
    // peripheral shifts in one byte per byte shifted out — without the
    // RX DMA draining the RX FIFO, the SPI stalls after the first byte.
    DL_DMA_setSrcAddr(
        DMA, bsp_spi_instances[idx].dma_rx_channel, (uint32_t)(&bsp_spi_instances[idx].inst->RXDATA));
    DL_DMA_setDestAddr(DMA, bsp_spi_instances[idx].dma_rx_channel, (uint32_t)bsp_spi_instances[idx].rx_scratch);
    DL_DMA_setTransferSize(DMA, bsp_spi_instances[idx].dma_rx_channel, len);

    // TX DMA: source = user buffer, dest = TXDATA register.
    DL_DMA_setSrcAddr(DMA, bsp_spi_instances[idx].dma_tx_channel, (uint32_t)data);
    DL_DMA_setDestAddr(
        DMA, bsp_spi_instances[idx].dma_tx_channel, (uint32_t)(&bsp_spi_instances[idx].inst->TXDATA));
    DL_DMA_setTransferSize(DMA, bsp_spi_instances[idx].dma_tx_channel, len);

    // Enable RX first (so it's armed when TX triggers the first byte shift),
    // then TX (which kicks off the actual transfer).
    DL_DMA_enableChannel(DMA, bsp_spi_instances[idx].dma_rx_channel);
    DL_DMA_enableChannel(DMA, bsp_spi_instances[idx].dma_tx_channel);
}

void Bsp_Spi_Read(uint32_t idx, uint8_t* data, uint32_t len) {
    if (idx >= SPI_NUM) return;

    DL_DMA_setSrcAddr(
        DMA, bsp_spi_instances[idx].dma_rx_channel, (uint32_t)(&bsp_spi_instances[idx].inst->RXDATA));
    DL_DMA_setDestAddr(DMA, bsp_spi_instances[idx].dma_rx_channel, (uint32_t)data);
    DL_DMA_setTransferSize(DMA, bsp_spi_instances[idx].dma_rx_channel, len);

    DL_DMA_enableChannel(DMA, bsp_spi_instances[idx].dma_rx_channel);
}

void Bsp_Spi_Register_Tx_Dma_Done_Cb(uint32_t idx, Bsp_spi_tx_dma_done_cb_t cb, void* cb_arg) {
    if (idx >= SPI_NUM || cb == NULL) return;

    Vector_Push_Back(bsp_spi_instances[idx].tx_dma_done.cb_vec, (void*)&cb);
    Vector_Push_Back(bsp_spi_instances[idx].tx_dma_done.cb_arg_vec, (void*)&cb_arg);
}

void Bsp_Spi_Register_Tx_Done_Cb(uint32_t idx, Bsp_spi_tx_done_cb_t cb, void* cb_arg) {
    if (idx >= SPI_NUM || cb == NULL) return;

    Vector_Push_Back(bsp_spi_instances[idx].tx_done.cb_vec, (void*)&cb);
    Vector_Push_Back(bsp_spi_instances[idx].tx_done.cb_arg_vec, (void*)&cb_arg);
}

void Bsp_Spi_Register_Rx_Dma_Done_Cb(uint32_t idx, Bsp_spi_rx_dma_done_cb_t cb, void* cb_arg) {
    if (idx >= SPI_NUM || cb == NULL) return;

    Vector_Push_Back(bsp_spi_instances[idx].rx_dma_done.cb_vec, (void*)&cb);
    Vector_Push_Back(bsp_spi_instances[idx].rx_dma_done.cb_arg_vec, (void*)&cb_arg);
}

void Bsp_Spi_Irq_Handler(SPI_Regs* spi_inst) {
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
            default:
                break;
        }
    }
}

#else

void Bsp_Spi_Init(void) {}

void Bsp_Spi_Write(uint32_t idx, const uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Spi_Read(uint32_t idx, uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

void Bsp_Spi_Register_Tx_Dma_Done_Cb(uint32_t idx, Bsp_spi_tx_dma_done_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}

void Bsp_Spi_Register_Tx_Done_Cb(uint32_t idx, Bsp_spi_tx_done_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}

void Bsp_Spi_Register_Rx_Dma_Done_Cb(uint32_t idx, Bsp_spi_rx_dma_done_cb_t cb, void* cb_arg) {
    (void)idx;
    (void)cb;
    (void)cb_arg;
}

void Bsp_Spi_Irq_Handler(SPI_Regs* spi_inst) { (void)spi_inst; }

#endif