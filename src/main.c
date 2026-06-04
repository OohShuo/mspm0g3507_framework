#include "FreeRTOS.h"
#include "app.h"
#include "bsp.h"
#include "hal.h"
#include "task.h"
#include "ti_msp_dl_config.h"

TaskHandle_t main_task_handle = NULL;
TaskHandle_t app_task_handle = NULL;
TaskHandle_t buzzer_task_handle = NULL;

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

int main(void) {
    SYSCFG_DL_init();

    Bsp_Init();
    Hal_Init();
    App_Init();

    xTaskCreate(task_gpio, "Gpio_Task", 128, NULL, 1, &main_task_handle);
    xTaskCreate(task_app, "APP_Task", 128, NULL, 1, &app_task_handle);
    xTaskCreate(task_buzzer, "Buzzer_Task", 128, NULL, 1, &buzzer_task_handle);

    vTaskStartScheduler();

    while (1);
}
