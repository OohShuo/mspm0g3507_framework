#include "FreeRTOS.h"
#include "app.h"
#include "bsp.h"
#include "hal.h"
#include "task.h"
#include "ti_msp_dl_config.h"

TaskHandle_t main_task_handle = NULL;
TaskHandle_t app_task_handle = NULL;
TaskHandle_t buzzer_task_handle = NULL;
TaskHandle_t lcd_test_task_handle = NULL;
TaskHandle_t w25q32_test_task_handle = NULL;

extern void App_Lcd_Test_Init(void);
extern void App_Lcd_Test_Loop(void);
extern void App_W25q32_Test_Init(void);
extern void App_W25q32_Test_Loop(void);

// This SPI bus is dedicated to the ST7789 LCD. The W25Q32 test was
// sharing SPI0 with the LCD, which is fine when one of them is parked,
// but breaks the moment both tasks are alive (concurrent DMA config
// corrupts in-flight transactions on the same SPI instance). Re-enable
// the W25Q32 test only if you move it to a different SPI / bit-bang.
#define LCD_TEST_ENABLE    1
#define W25Q32_TEST_ENABLE 0

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

int main(void) {
    SYSCFG_DL_init();

    Bsp_Init();
    Hal_Init();
    App_Init();

    xTaskCreate(task_gpio, "Gpio_Task", 128, NULL, 1, &main_task_handle);
    xTaskCreate(task_app, "APP_Task", 128, NULL, 1, &app_task_handle);
    xTaskCreate(task_buzzer, "Buzzer_Task", 128, NULL, 1, &buzzer_task_handle);
#if LCD_TEST_ENABLE
    xTaskCreate(task_lcd_test, "LCD_Test", 512, NULL, 1, &lcd_test_task_handle);
#endif
#if W25Q32_TEST_ENABLE
    xTaskCreate(task_w25q32_test, "W25Q32_Test", 256, NULL, 1, &w25q32_test_task_handle);
#endif

    vTaskStartScheduler();

    while (1);
}
