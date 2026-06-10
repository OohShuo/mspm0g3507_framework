#include "FreeRTOS.h"
#include "app.h"
#include "bsp.h"
#include "hal.h"
#include "local_lib.h"
#include "retarget.h"
#include "task.h"
#include "ti_msp_dl_config.h"

TaskHandle_t main_task_handle = NULL;
TaskHandle_t app_task_handle = NULL;
TaskHandle_t buzzer_task_handle = NULL;
TaskHandle_t lcd_test_task_handle = NULL;
TaskHandle_t st7789_img_test_task_handle = NULL;
TaskHandle_t lvgl_hello_task_handle = NULL;
TaskHandle_t lvgl_ball_task_handle = NULL;
TaskHandle_t w25q32_test_task_handle = NULL;
TaskHandle_t rtt_test_task_handle = NULL;
TaskHandle_t lfs_test_task_handle = NULL;
TaskHandle_t com_uart_test_task_handle = NULL;

extern void App_Lcd_Test_Init(void);
extern void App_Lcd_Test_Loop(void);
extern void App_St7789_Img_Test_Init(void);
extern void App_St7789_Img_Test_Loop(void);
extern void App_Lvgl_Hello_Init(void);
extern void App_Lvgl_Hello_Loop(void);
extern void App_Lvgl_Ball_Init(void);
extern void App_Lvgl_Ball_Loop(void);
extern void App_W25q32_Test_Init(void);
extern void App_W25q32_Test_Loop(void);
extern void Rtt_Test_Init(void);
extern void Rtt_Test_Loop(void);
extern void App_Lfs_Test_Init(void);
extern void App_Lfs_Test_Loop(void);
extern void App_Com_Uart_Test_Init(void);
extern void App_Com_Uart_Test_Loop(void);

#define LCD_TEST_ENABLE        0
#define ST7789_IMG_TEST_ENABLE 0
#define LVGL_HELLO_ENABLE      0
#define LVGL_BALL_ENABLE       0
#define W25Q32_TEST_ENABLE     0
#define RTT_TEST_ENABLE        0
#define LFS_TEST_ENABLE        0
#define COM_UART_TEST_ENABLE   1

static void task_gpio(void* arg) {
    uint32_t tick = xTaskGetTickCount();

    while (1) {
        Hal_Gpio_Loop();

        vTaskDelayUntil(&tick, pdMS_TO_TICKS(10));
    }
}

static void task_app(void* arg) {
    uint32_t tick = xTaskGetTickCount();

    while (1) {
        App_Loop();

        vTaskDelayUntil(&tick, pdMS_TO_TICKS(2));
    }
}

static void task_buzzer(void* arg) {
    uint32_t tick = xTaskGetTickCount();

    while (1) {
        Hal_Buzzer_Loop();

        vTaskDelayUntil(&tick, pdMS_TO_TICKS(5));
    }
}

#if LCD_TEST_ENABLE
static void task_lcd_test(void* arg) {
    (void)arg;
    // Init runs from a task because St7789_Send_Init_Seq uses vTaskDelay.
    App_Lcd_Test_Init();
    while (1) {
        App_Lcd_Test_Loop();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
#endif

#if ST7789_IMG_TEST_ENABLE
static void task_st7789_img_test(void* arg) {
    (void)arg;
    // Same init-from-task pattern as lcd_test: the ST7789 startup sequence
    // busy-waits ~720 ms and we don't want to block the scheduler start.
    App_St7789_Img_Test_Init();
    while (1) {
        App_St7789_Img_Test_Loop();
        // One full 220x240 frame over soft SPI takes a few hundred ms.
        // 20 ms tick matches the other LCD tests.
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
#endif

#if W25Q32_TEST_ENABLE
static void task_w25q32_test(void* arg) {
    (void)arg;
    // Init was already called from App_Init() before the scheduler started.
    // Calling it again here would leak the first W25q32 instance.App_W25q32_Test_Init();
    App_W25q32_Test_Init();
    while (1) {
        App_W25q32_Test_Loop();
        // Re-run every 5 s. The full battery does one 4KB sector erase +
        // one 256B page program; W25Q32 endurance is ~100K erase/sector,
        // so this leaves the flash with years of margin during a debug session.
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
#endif

#if LVGL_HELLO_ENABLE
static void task_lvgl_hello(void* arg) {
    (void)arg;
    App_Lvgl_Hello_Init();
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        App_Lvgl_Hello_Loop();
        // 5 ms is well under LV_DEF_REFR_PERIOD (33 ms); over-pump a
        // little so animations stay smooth. lv_timer_handler() is a
        // no-op when nothing is due.
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(20));
    }
}
#endif

#if LVGL_BALL_ENABLE
static void task_lvgl_ball(void* arg) {
    (void)arg;
    App_Lvgl_Ball_Init();
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        App_Lvgl_Ball_Loop();
        // 20 ms ≈ 50 fps, fast enough to look smooth at 2-3 px/frame
        // and slow enough that software SPI can keep up. vTaskDelayUntil
        // anchors to the wall clock, so the ball never drifts over time.
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(20));
    }
}
#endif

#if RTT_TEST_ENABLE
static void task_rtt_test(void* arg) {
    (void)arg;
    Rtt_Test_Init();
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        Rtt_Test_Loop();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(1000));
    }
}
#endif

#if LFS_TEST_ENABLE
static void task_lfs_test(void* arg) {
    (void)arg;
    // lfs test runs the full battery once in Init. Loop is a no-op,
    // so the task just parks here. Bumping the priority above the
    // LVGL/LCD tasks would also be safe — the test touches the SPI
    // bus only during Init, before the scheduler starts.
    App_Lfs_Test_Init();
    while (1) {
        App_Lfs_Test_Loop();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

#if COM_UART_TEST_ENABLE
static void task_com_uart_test(void* arg) {
    (void)arg;
    App_Com_Uart_Test_Init();
    uint32_t tick = xTaskGetTickCount();
    while (1) {
        App_Com_Uart_Test_Loop();
        // 1 s tick is the send cadence; on_rx fires from the bsp idle
        // ISR and prints synchronously via printf, so the loop only
        // needs to wake up often enough to drive the heartbeat send.
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(1000));
    }
}
#endif

int main(void) {
    SYSCFG_DL_init();

    Syscall_Init();

    Local_Lib_Init();
    Bsp_Init();
    Hal_Init();
    App_Init();

    xTaskCreate(task_gpio, "Gpio_Task", 64, NULL, 1, &main_task_handle);
    xTaskCreate(task_app, "APP_Task", 64, NULL, 1, &app_task_handle);
    xTaskCreate(task_buzzer, "Buzzer_Task", 64, NULL, 1, &buzzer_task_handle);
#if LCD_TEST_ENABLE
    xTaskCreate(task_lcd_test, "LCD_Test", 256, NULL, 1, &lcd_test_task_handle);
#endif
#if ST7789_IMG_TEST_ENABLE
    xTaskCreate(task_st7789_img_test, "ST7789_Img", 256, NULL, 1, &st7789_img_test_task_handle);
#endif
#if LVGL_HELLO_ENABLE
    xTaskCreate(task_lvgl_hello, "LVGL_Hello", 1024, NULL, 1, &lvgl_hello_task_handle);
#endif
#if LVGL_BALL_ENABLE
    xTaskCreate(task_lvgl_ball, "LVGL_Ball", 1024, NULL, 1, &lvgl_ball_task_handle);
#endif
#if W25Q32_TEST_ENABLE
    xTaskCreate(task_w25q32_test, "W25Q32_Test", 128, NULL, 1, &w25q32_test_task_handle);
#endif
#if RTT_TEST_ENABLE
    xTaskCreate(task_rtt_test, "RTT_Test", 512, NULL, 1, &rtt_test_task_handle);
#endif
#if LFS_TEST_ENABLE
    xTaskCreate(task_lfs_test, "LFS_Test", 1024, NULL, 1, &lfs_test_task_handle);
#endif
#if COM_UART_TEST_ENABLE
    xTaskCreate(task_com_uart_test, "ComUartTest", 512, NULL, 1, &com_uart_test_task_handle);
#endif

    vTaskStartScheduler();

    while (1);
}
