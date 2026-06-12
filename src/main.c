#include "FreeRTOS.h"
#include "app.h"
#include "bsp.h"
#include "hal.h"
#include "local_lib.h"
#include "retarget.h"
#include "task.h"
#include "test.h"
#include "ti_msp_dl_config.h"

int main(void) {
    SYSCFG_DL_init();

    Syscall_Init();

    Local_Lib_Init();
    Bsp_Init();
    Hal_Init();
    App_Init();

    Hal_Task_Def();
    Test_Task_Def();
    App_Task_Def();

    vTaskStartScheduler();

    while (1);
}
