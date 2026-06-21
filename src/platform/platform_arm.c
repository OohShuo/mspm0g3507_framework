#include "platform.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "task.h"
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

int Platform_Init(void) {
    normalize_debug_reset();
    SYSCFG_DL_init();
    NVIC_EnableIRQ(DMA_INT_IRQn);
    return 0;
}

int Platform_Start(void) {
    vTaskStartScheduler();
    return -1;
}
