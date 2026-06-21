#include "test_vib_motor.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "task.h"
#include "vib_motor.h"

Vib_motor* g_vib_motor = NULL;

static void vib_motor_task(void* arg) {
    (void)arg;
    const Vib_motor_config config = {
        .pwm_idx = PWM_VIB_MOTOR_IDX,
        .pwm_freq_hz = VIB_MOTOR_DEFAULT_PWM_FREQ_HZ,
        .max_duty_percent = VIB_MOTOR_DEFAULT_MAX_DUTY_PERCENT,
        .master_strength_percent = VIB_MOTOR_DEFAULT_MASTER_STRENGTH,
    };
    g_vib_motor = Vib_Motor_Create(&config);
    while (1) {
        Vib_Motor_Play_Effect(g_vib_motor, vib_effect_menu_tick);
        vTaskDelay(pdMS_TO_TICKS(1000));

        Vib_Motor_Play_Effect(g_vib_motor, vib_effect_menu_select);
        vTaskDelay(pdMS_TO_TICKS(1500));

        Vib_Motor_Play_Effect(g_vib_motor, vib_effect_hit_heavy);
        vTaskDelay(pdMS_TO_TICKS(2500));

        Vib_Motor_Play_Effect(g_vib_motor, vib_effect_victory);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void Test_Vib_Motor_Task_Def(void) { xTaskCreate(vib_motor_task, "Vib_Motor", 128, NULL, 1, NULL); }
