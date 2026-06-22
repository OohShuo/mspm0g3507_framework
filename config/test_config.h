#pragma once

#define TEST_BUTTON_ENABLE        0
#define TEST_BUZZER_ENABLE        0
#define TEST_COM_UART_ENABLE      0
#define TEST_JOYSTICK_ENABLE      0
#define TEST_LCD_ENABLE           0
#define TEST_LED_BREATH_ENABLE    0
#define TEST_LED_SIMPLE_ENABLE    0
#define TEST_LFS_ENABLE           0
#define TEST_LVGL_BALL_ENABLE     0
#define TEST_LVGL_HELLO_ENABLE    0
#define TEST_RTT_ENABLE           0
#define TEST_SLIP_RECV_ENABLE     0
#define TEST_ST7789_IMG_ENABLE    0
#define TEST_VIB_MOTOR_GPIO_ENABLE 0
#define TEST_W25Q32_ENABLE        0

#define TEST_ANY_ENABLE                                                                  \
    (TEST_BUTTON_ENABLE || TEST_BUZZER_ENABLE || TEST_COM_UART_ENABLE ||                 \
        TEST_JOYSTICK_ENABLE || TEST_LCD_ENABLE || TEST_LED_BREATH_ENABLE ||             \
        TEST_LED_SIMPLE_ENABLE || TEST_LFS_ENABLE || TEST_LVGL_BALL_ENABLE ||            \
        TEST_LVGL_HELLO_ENABLE || TEST_RTT_ENABLE || TEST_SLIP_RECV_ENABLE ||            \
        TEST_ST7789_IMG_ENABLE || TEST_VIB_MOTOR_GPIO_ENABLE || TEST_W25Q32_ENABLE)
