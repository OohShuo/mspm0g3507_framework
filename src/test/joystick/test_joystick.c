#include "test_joystick.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "joystick.h"
#include "task.h"

Joystick* g_joystick = NULL;

static void joystick_task(void* arg) {
    (void)arg;
    const Joystick_config cfg = {
        .adc_idx = ADC_JOYSTICK_IDX,
        .adc_channel_x = ADC_JOYSTICK_X_CHANNEL,
        .adc_channel_y = ADC_JOYSTICK_Y_CHANNEL,
        .x_min_voltage = JOYSTICK_X_MIN_VOLTAGE,
        .x_max_voltage = JOYSTICK_X_MAX_VOLTAGE,
        .y_min_voltage = JOYSTICK_Y_MIN_VOLTAGE,
        .y_max_voltage = JOYSTICK_Y_MAX_VOLTAGE,
        .x_offset = JOYSTICK_X_OFFSET,
        .y_offset = JOYSTICK_Y_OFFSET,
        .x_dead_zone = JOYSTICK_X_DEAD_ZONE,
        .y_dead_zone = JOYSTICK_Y_DEAD_ZONE,
        .x_reverse = JOYSTICK_X_REVERSE,
        .y_reverse = JOYSTICK_Y_REVERSE,
    };
    g_joystick = Joystick_Create(&cfg);
    Joystick_Calibrate_Center(
        g_joystick, JOYSTICK_CALIBRATION_SAMPLES, JOYSTICK_CALIBRATION_INTERVAL_MS);
    while (1) { vTaskDelay(pdMS_TO_TICKS(5000)); }
}

void Test_Joystick_Task_Def(void) { xTaskCreate(joystick_task, "Joystick", 128, NULL, 1, NULL); }
