#include "test_joystick.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "joystick.h"
#include "task.h"

static void joystick_task(void* arg) {
    (void)arg;
    const Joystick_config cfg = {
        .adc_idx = ADC_JOYSTICK_IDX,
        .adc_channel_x = ADC_JOYSTICK_X_CHANNEL,
        .adc_channel_y = ADC_JOYSTICK_Y_CHANNEL,
    };
    Joystick_Create(&cfg);
    while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

void Test_Joystick_Task_Def(void) { xTaskCreate(joystick_task, "Joystick", 128, NULL, 1, NULL); }
