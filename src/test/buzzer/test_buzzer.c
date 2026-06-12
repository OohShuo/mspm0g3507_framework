#include "test_buzzer.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "buzzer.h"
#include "task.h"

static void buzzer_task(void* arg) {
    (void)arg;
    const Buzzer_config cfg = {.pwm_idx = PWM_BUZZER_IDX};
    Buzzer_Create(&cfg);
    while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

void Test_Buzzer_Task_Def(void) { xTaskCreate(buzzer_task, "Buzzer", 128, NULL, 1, NULL); }
