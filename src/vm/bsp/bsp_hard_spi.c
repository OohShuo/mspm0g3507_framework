#include "hard/bsp_hard_spi.h"
void Bsp_Hard_Spi_Init(void) {}
void Bsp_Hard_Spi_Wait_For_Complete(uint32_t i) { (void)i; }
void Bsp_Hard_Spi_Write(uint32_t i, const uint8_t* d, uint32_t l) {
    (void)i;
    (void)d;
    (void)l;
}
void Bsp_Hard_Spi_Read(uint32_t i, uint8_t* d, uint32_t l) {
    (void)i;
    (void)d;
    (void)l;
}
void Bsp_Hard_Spi_Write_Blocking(uint32_t i, const uint8_t* d, uint32_t l) {
    (void)i;
    (void)d;
    (void)l;
}
void Bsp_Hard_Spi_Read_Blocking(uint32_t i, uint8_t* d, uint32_t l) {
    (void)i;
    (void)d;
    (void)l;
}
void Bsp_Hard_Spi_Register_Tx_Dma_Done_Cb(uint32_t i, Bsp_spi_tx_dma_done_cb_t cb, void* a) {
    (void)i;
    (void)cb;
    (void)a;
}
void Bsp_Hard_Spi_Register_Tx_Done_Cb(uint32_t i, Bsp_spi_tx_done_cb_t cb, void* a) {
    (void)i;
    (void)cb;
    (void)a;
}
void Bsp_Hard_Spi_Register_Rx_Dma_Done_Cb(uint32_t i, Bsp_spi_rx_dma_done_cb_t cb, void* a) {
    (void)i;
    (void)cb;
    (void)a;
}
void Bsp_Hard_Spi_Register_Idle_Cb(uint32_t i, Bsp_spi_idle_cb_t cb, void* a) {
    (void)i;
    (void)cb;
    (void)a;
}
