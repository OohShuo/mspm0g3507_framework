#include "bsp_uart.h"
void Bsp_Uart_Init(void) {}
void Bsp_Uart_Wait_For_Complete(uint32_t i) { (void)i; }
void Bsp_Uart_Write(uint32_t i, const uint8_t* d, uint32_t l) {
    (void)i;
    (void)d;
    (void)l;
}
void Bsp_Uart_Read(uint32_t i, uint8_t* d, uint32_t l) {
    (void)i;
    (void)d;
    (void)l;
}
void Bsp_Uart_Write_Blocking(uint32_t i, const uint8_t* d, uint32_t l) {
    (void)i;
    (void)d;
    (void)l;
}
void Bsp_Uart_Read_Blocking(uint32_t i, uint8_t* d, uint32_t l) {
    (void)i;
    (void)d;
    (void)l;
}
void Bsp_Uart_Start_Continuous_Rx(uint32_t i, uint32_t t, uint8_t* b, uint32_t m) {
    (void)i;
    (void)t;
    (void)b;
    (void)m;
}
void Bsp_Uart_Stop_Continuous_Rx(uint32_t i) { (void)i; }
void Bsp_Uart_Register_Tx_Dma_Done_Cb(uint32_t i, Bsp_uart_tx_dma_done_cb_t cb, void* a) {
    (void)i;
    (void)cb;
    (void)a;
}
void Bsp_Uart_Register_Tx_Done_Cb(uint32_t i, Bsp_uart_tx_done_cb_t cb, void* a) {
    (void)i;
    (void)cb;
    (void)a;
}
void Bsp_Uart_Register_Rx_Dma_Done_Cb(uint32_t i, Bsp_uart_rx_dma_done_cb_t cb, void* a) {
    (void)i;
    (void)cb;
    (void)a;
}
void Bsp_Uart_Register_Rx_Idle_Cb(uint32_t i, Bsp_uart_rx_idle_cb_t cb, void* a) {
    (void)i;
    (void)cb;
    (void)a;
}
