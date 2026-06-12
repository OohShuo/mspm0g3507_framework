#include "test.h"

#include "com_uart/test_com_uart.h"
#include "lcd/test_lcd.h"
#include "lfs/test_lfs.h"
#include "lvgl_ball/test_lvgl_ball.h"
#include "lvgl_hello/test_lvgl_hello.h"
#include "rtt/test_rtt.h"
#include "slip_recv/test_slip_recv.h"
#include "st7789_img/test_st7789_img.h"
#include "test_config.h"
#include "w25q32/test_w25q32.h"

void Test_Init(void) {
#if TEST_LCD_ENABLE
    Test_Lcd_Task_Def();
#endif
#if TEST_ST7789_IMG_ENABLE
    Test_St7789_Img_Task_Def();
#endif
#if TEST_LVGL_HELLO_ENABLE
    Test_Lvgl_Hello_Task_Def();
#endif
#if TEST_LVGL_BALL_ENABLE
    Test_Lvgl_Ball_Task_Def();
#endif
#if TEST_W25Q32_ENABLE
    Test_W25q32_Task_Def();
#endif
#if TEST_RTT_ENABLE
    Test_Rtt_Task_Def();
#endif
#if TEST_LFS_ENABLE
    Test_Lfs_Task_Def();
#endif
#if TEST_SLIP_RECV_ENABLE
    Test_Slip_Recv_Task_Def();
#endif
#if TEST_COM_UART_ENABLE
    Test_Com_Uart_Task_Def();
#endif
}
