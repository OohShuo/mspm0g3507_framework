#include "app.h"

#include "FreeRTOS.h"
#include "app_config.h"
#include "flash_mgr.h"
#include "task.h"
#include "ti_msp_dl_config.h"

void App_Init(void) {}

uint32_t a, b, c, d = 0;
void app_task(void* arg) {
    (void)arg;
    while (1) {
        a = PWM_2_INST->COUNTERREGS.CC_01[0];
        b = PWM_2_INST->COUNTERREGS.CC_01[1];
        c = PWM_2_INST->FPUB_0;
        d = PWM_2_INST->GEN_EVENT0.MIS;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void App_Task_Def(void) {
#if FLASH_MGR_ENABLE
    Flash_Mgr_Task_Def();
#endif
    xTaskCreate(app_task, "App", 128, NULL, 1, NULL);
}
