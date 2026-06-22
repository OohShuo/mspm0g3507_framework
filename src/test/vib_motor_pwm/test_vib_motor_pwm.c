#include "test_vib_motor_pwm.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "task.h"
#include "vib_motor_pwm.h"

static Vib_motor_pwm* g_motor = NULL;

static void vib_motor_pwm_task(void* arg) {
    (void)arg;
    const Vib_motor_pwm_config config = {
        .pwm_idx = PWM_VIB_MOTOR_PWM_IDX,
        .pwm_freq_hz = VIB_MOTOR_PWM_DEFAULT_FREQ_HZ,
        .max_duty_percent = VIB_MOTOR_PWM_DEFAULT_MAX_DUTY_PERCENT,
        .master_strength_percent = VIB_MOTOR_PWM_DEFAULT_MASTER_STRENGTH,
    };
    g_motor = Vib_Motor_Pwm_Create(&config);

    while (1) {
        Vib_Motor_Pwm_Play_Effect(g_motor, vib_pwm_effect_menu_tick);
        vTaskDelay(pdMS_TO_TICKS(1000));

        Vib_Motor_Pwm_Play_Effect(g_motor, vib_pwm_effect_menu_select);
        vTaskDelay(pdMS_TO_TICKS(1500));

        Vib_Motor_Pwm_Play_Effect(g_motor, vib_pwm_effect_hit_heavy);
        vTaskDelay(pdMS_TO_TICKS(2500));

        Vib_Motor_Pwm_Play_Effect(g_motor, vib_pwm_effect_victory);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void Test_Vib_Motor_Pwm_Task_Def(void) {
    xTaskCreate(vib_motor_pwm_task, "VibMotorPwm", 128, NULL, 1, NULL);
}
