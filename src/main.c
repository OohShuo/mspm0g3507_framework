#include "FreeRTOS.h"
#include "app.h"
#include "bsp.h"
#include "hal.h"
#include "local_lib.h"
#include "retarget.h"
#include "task.h"
#include "test.h"
#include "ti_msp_dl_config.h"

TaskHandle_t flash_mgr_init_task_handle = NULL;

extern void Flash_Mgr_Init(void);

#define FLASH_MGR_ENABLE 0

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

    Hal_Task_Def();
    Test_Task_Def();

#if FLASH_MGR_ENABLE
    xTaskCreate(task_flash_mgr_init, "FlashMgrInit", 512, NULL, 2, &flash_mgr_init_task_handle);
#endif

    vTaskStartScheduler();

    while (1);
}
