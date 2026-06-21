#include "test.h"

#include "button/test_button.h"
#include "buzzer/test_buzzer.h"
#include "com_uart/test_com_uart.h"
#include "joystick/test_joystick.h"
#include "lcd/test_lcd.h"
#include "led_breath/test_led_breath.h"
#include "led_simple/test_led_simple.h"
#include "lfs/test_lfs.h"
#include "lvgl_ball/test_lvgl_ball.h"
#include "lvgl_hello/test_lvgl_hello.h"
#include "rtt/test_rtt.h"
#include "slip_recv/test_slip_recv.h"
#include "st7789_img/test_st7789_img.h"
#include "test_config.h"
#include "vib_motor/test_vib_motor.h"
#include "w25q32/test_w25q32.h"

void Test_Task_Def(void) {
#if TEST_BUTTON_ENABLE
    Test_Button_Task_Def();
#endif
#if TEST_BUZZER_ENABLE
    Test_Buzzer_Task_Def();
#endif
#if TEST_COM_UART_ENABLE
    Test_Com_Uart_Task_Def();
#endif
#if TEST_JOYSTICK_ENABLE
    Test_Joystick_Task_Def();
#endif
#if TEST_LCD_ENABLE
    Test_Lcd_Task_Def();
#endif
#if TEST_LED_BREATH_ENABLE
    Test_Led_Breath_Task_Def();
#endif
#if TEST_LED_SIMPLE_ENABLE
    Test_Led_Simple_Task_Def();
#endif
#if TEST_LVGL_BALL_ENABLE
    Test_Lvgl_Ball_Task_Def();
#endif
#if TEST_LVGL_HELLO_ENABLE
    Test_Lvgl_Hello_Task_Def();
#endif
#if TEST_RTT_ENABLE
    Test_Rtt_Task_Def();
#endif
#if TEST_SLIP_RECV_ENABLE
    Test_Slip_Recv_Task_Def();
#endif
#if TEST_ST7789_IMG_ENABLE
    Test_St7789_Img_Task_Def();
#endif
#if TEST_VIB_MOTOR_ENABLE
    Test_Vib_Motor_Task_Def();
#endif
#if TEST_W25Q32_ENABLE
    Test_W25q32_Task_Def();
#endif
#if TEST_LFS_ENABLE
    Test_Lfs_Task_Def();
#endif
}
