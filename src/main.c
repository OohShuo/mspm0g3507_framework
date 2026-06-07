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

static void task_lcd_test(void* arg) {
    (void)arg;
    // LCD test is currently disabled — see task_w25q32_test for the active
    // bsp_spi verification.
    while (1) { vTaskDelay(pdMS_TO_TICKS(100)); }
}

static void task_w25q32_test(void* arg) {
    (void)arg;
    // Init was already called from App_Init() before the scheduler started.
    // Calling it again here would leak the first W25q32 instance.
    while (1) {
        App_W25q32_Test_Loop();
        // Re-run every 5 s. The full battery does one 4KB sector erase +
        // one 256B page program; W25Q32 endurance is ~100K erase/sector,
        // so this leaves the flash with years of margin during a debug session.
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

int main(void) {
    SYSCFG_DL_init();

    Bsp_Init();
    Hal_Init();
    App_Init();

    xTaskCreate(task_gpio, "Gpio_Task", 128, NULL, 1, &main_task_handle);
    xTaskCreate(task_app, "APP_Task", 128, NULL, 1, &app_task_handle);
    xTaskCreate(task_buzzer, "Buzzer_Task", 128, NULL, 1, &buzzer_task_handle);
    xTaskCreate(task_lcd_test, "LCD_Test", 256, NULL, 1, &lcd_test_task_handle);
    xTaskCreate(task_w25q32_test, "W25Q32_Test", 256, NULL, 1, &w25q32_test_task_handle);

    vTaskStartScheduler();

    while (1);
}
