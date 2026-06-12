#include "test_led_breath.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "led_breath.h"
#include "task.h"

static void led_breath_task(void* arg) {
    (void)arg;
    const Led_breath_config cfg = {
        .pwm_idx = PWM_VIB_MOTOR_IDX,
        .max_brightness = 100,
        .breath_freq_hz = 1.0f,
    };
    Led_Breath_Create(&cfg);
    while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

void Test_Led_Breath_Task_Def(void) { xTaskCreate(led_breath_task, "LED_Breath", 128, NULL, 1, NULL); }
