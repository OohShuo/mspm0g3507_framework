#include "FreeRTOS.h"
#include "app.h"
#include "board_config.h"
#include "bsp.h"
#include "hal.h"
#include "local_lib.h"
#include "retarget.h"
#include "task.h"
#include "test.h"
#include "ti_msp_dl_config.h"

static void normalize_debug_reset(void) {
#if DEBUG_RESET_ESCALATE_TO_BOOTRST
    const DL_SYSCTL_RESET_CAUSE cause = DL_SYSCTL_getResetCause();
    if (cause == DL_SYSCTL_RESET_CAUSE_SYSRST_DEBUG_TRIGGERED ||
        cause == DL_SYSCTL_RESET_CAUSE_CPURST_DEBUG_TRIGGERED) {
        DL_SYSCTL_resetDevice(DL_SYSCTL_RESET_BOOT);
        while (1) { }
    }
#endif
}

int main(void) {
    normalize_debug_reset();
    SYSCFG_DL_init();
    NVIC_EnableIRQ(DMA_INT_IRQn);

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
