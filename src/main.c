#include "FreeRTOS.h"
#include "app.h"
#include "bsp.h"
#include "hal.h"
#include "task.h"
#include "ti_msp_dl_config.h"

TaskHandle_t led_task_handle = NULL;

static void task_led(void* arg) {
    uint32_t tick = xTaskGetTickCount();

    while (1) {
        Hal_Led_Loop();

        vTaskDelayUntil(&tick, pdMS_TO_TICKS(500));
    }
}

int main(void) {
    SYSCFG_DL_init();

    Bsp_Init();
    Hal_Init();
    App_Init();

    xTaskCreate(task_led, "LED_Task", 128, NULL, 1, &led_task_handle);

    vTaskStartScheduler();

    while (1);
}