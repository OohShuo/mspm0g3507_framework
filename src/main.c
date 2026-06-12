#include "FreeRTOS.h"
#include "app.h"
#include "bsp.h"
#include "hal.h"
#include "local_lib.h"
#include "retarget.h"
#include "task.h"
#include "test.h"
#include "ti_msp_dl_config.h"

TaskHandle_t main_task_handle = NULL;
TaskHandle_t app_task_handle = NULL;
TaskHandle_t buzzer_task_handle = NULL;
TaskHandle_t flash_mgr_init_task_handle = NULL;

extern void Flash_Mgr_Init(void);

#define FLASH_MGR_ENABLE 0

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

#if FLASH_MGR_ENABLE
static void task_flash_mgr_init(void* arg) {
    (void)arg;
    Flash_Mgr_Init();
    vTaskDelete(NULL);
}
#endif

int main(void) {
    SYSCFG_DL_init();

    Syscall_Init();

    Local_Lib_Init();
    Bsp_Init();
    Hal_Init();
    App_Init();

    Test_Init();

    xTaskCreate(task_gpio, "Gpio_Task", 64, NULL, 1, &main_task_handle);
    xTaskCreate(task_app, "APP_Task", 64, NULL, 1, &app_task_handle);
    xTaskCreate(task_buzzer, "Buzzer_Task", 64, NULL, 1, &buzzer_task_handle);
#if FLASH_MGR_ENABLE
    xTaskCreate(task_flash_mgr_init, "FlashMgrInit", 512, NULL, 2, &flash_mgr_init_task_handle);
#endif

    vTaskStartScheduler();

    while (1);
}
