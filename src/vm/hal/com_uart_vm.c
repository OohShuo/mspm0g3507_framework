#include <stdlib.h>

#include "com_uart.h"

void Com_Uart_Init(void) {}
Com_uart* Com_Uart_Create(const Com_uart_config* c) {
    (void)c;
    return NULL;
}
void Com_Uart_Send(Com_uart* o, const uint8_t* d, uint32_t l) {
    (void)o;
    (void)d;
    (void)l;
}
