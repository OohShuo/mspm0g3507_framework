#include "test.h"

#include "com_uart_test/com_uart_test.h"
#include "lcd_test/lcd_test.h"
#include "lfs_test/lfs_test.h"
#include "lvgl_ball/lvgl_ball.h"
#include "lvgl_hello/lvgl_hello.h"
#include "rtt_test/rtt_test.h"
#include "slip_recv/slip_recv.h"
#include "st7789_img_test/st7789_img_test.h"
#include "test_config.h"
#include "w25q32_test/w25q32_test.h"

void Test_Init(void) {
#if TEST_LCD_ENABLE
    Lcd_Test_Task_Def();
#endif
#if TEST_ST7789_IMG_ENABLE
    St7789_Img_Test_Task_Def();
#endif
#if TEST_LVGL_HELLO_ENABLE
    Lvgl_Hello_Test_Task_Def();
#endif
#if TEST_LVGL_BALL_ENABLE
    Lvgl_Ball_Test_Task_Def();
#endif
#if TEST_W25Q32_ENABLE
    W25q32_Test_Task_Def();
#endif
#if TEST_RTT_ENABLE
    Rtt_Test_Task_Def();
#endif
#if TEST_LFS_ENABLE
    Lfs_Test_Task_Def();
#endif
#if TEST_SLIP_RECV_ENABLE
    Slip_Recv_Test_Task_Def();
#endif
#if TEST_COM_UART_ENABLE
    Com_Uart_Test_Task_Def();
#endif
}
