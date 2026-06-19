#include "FreeRTOS.h"
#include "app_config.h"
#include "app.h"
#include "board_config.h"
#include "bsp.h"
#include "hal.h"
#include "local_lib.h"
#include "retarget.h"
#include "task.h"
#include "test.h"
#include "test_config.h"
#include "ti_msp_dl_config.h"

static void normalize_debug_reset(void) {
#if DEBUG_RESET_ESCALATE_TO_BOOTRST
    const DL_SYSCTL_RESET_CAUSE cause = DL_SYSCTL_getResetCause();
    if (cause == DL_SYSCTL_RESET_CAUSE_SYSRST_DEBUG_TRIGGERED ||
        cause == DL_SYSCTL_RESET_CAUSE_CPURST_DEBUG_TRIGGERED) {
        DL_SYSCTL_resetDevice(DL_SYSCTL_RESET_BOOT);
        while (1) {}
    }
#endif
}

int main(void) {
    normalize_debug_reset();
    SYSCFG_DL_init();
    NVIC_EnableIRQ(DMA_INT_IRQn);

#if FRAMEWORK_USE_RTT
    Syscall_Init();
#endif

#if LOCAL_LIB_INIT_ENABLE
    Local_Lib_Init();
#endif
    Bsp_Init();
    Hal_Init();
    App_Init();

    Hal_Task_Def();
#if TEST_ANY_ENABLE
    Test_Task_Def();
#endif
    App_Task_Def();

    vTaskStartScheduler();

    while (1);
}
