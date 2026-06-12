#include "app.h"

#include "FreeRTOS.h"
#include "flash_mgr.h"
#include "task.h"

static TaskHandle_t flash_mgr_task_handler = NULL;

void App_Init(void) {}

void App_Task_Def(void) {
    Flash_Mgr_Init();

    BaseType_t ret = xTaskCreate(Flash_Mgr_Loop, "FlashMgr", 1024, NULL, 1, &flash_mgr_task_handler);
    configASSERT(ret == pdPASS);
}
