#include "test.h"

#include "com_uart_test/com_uart_test.h"
#include "test_config.h"

void Test_Init(void) {
#if TEST_COM_UART_ENABLE
    Com_Uart_Test_Task_Def();
#endif
}
