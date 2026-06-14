#include "app.h"

#include "FreeRTOS.h"
#include "app_config.h"
#include "flash_mgr.h"
#include "task.h"

void App_Init(void) {}

static void app_task(void* arg) {
    (void)arg;
    while (1) {

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void App_Task_Def(void) {
#if FLASH_MGR_ENABLE
    Flash_Mgr_Task_Def();
#endif
    xTaskCreate(app_task, "App", 128, NULL, 1, NULL);
}
