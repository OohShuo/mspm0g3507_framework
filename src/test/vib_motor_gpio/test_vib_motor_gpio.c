#include "test_vib_motor_gpio.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "task.h"
#include "vib_motor_gpio.h"

static Vib_motor_gpio* g_motor = NULL;

static void vib_motor_gpio_task(void* arg) {
    (void)arg;
    const Vib_motor_gpio_config config = {
        .gpio_idx = GPIO_VIB_MOTOR_IDX,
        .active_high = 1u,
        .enabled = 1u,
    };
    g_motor = Vib_Motor_Gpio_Create(&config);

    while (1) {
        /* 1. Simple pulse: 50 ms on */
        Vib_Motor_Gpio_Play(g_motor, 50u);
        vTaskDelay(pdMS_TO_TICKS(500));

        /* 2. menu_select effect */
        Vib_Motor_Gpio_Play_Effect(g_motor, vib_gpio_effect_menu_select);
        vTaskDelay(pdMS_TO_TICKS(500));

        /* 3. victory effect (double short pulse) */
        Vib_Motor_Gpio_Play_Effect(g_motor, vib_gpio_effect_victory);
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* 4. defeat effect (short-short-long) */
        Vib_Motor_Gpio_Play_Effect(g_motor, vib_gpio_effect_defeat);
        vTaskDelay(pdMS_TO_TICKS(1500));

        /* 5. Test Set_Enabled(false) — should not output */
        Vib_Motor_Gpio_Set_Enabled(g_motor, 0u);
        Vib_Motor_Gpio_Play_Effect(g_motor, vib_gpio_effect_hit_heavy);
        vTaskDelay(pdMS_TO_TICKS(500));

        /* 6. Re-enable and test Stop */
        Vib_Motor_Gpio_Set_Enabled(g_motor, 1u);
        Vib_Motor_Gpio_Play(g_motor, 1000u);
        vTaskDelay(pdMS_TO_TICKS(100));
        Vib_Motor_Gpio_Stop(g_motor);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void Test_Vib_Motor_Gpio_Task_Def(void) {
    xTaskCreate(vib_motor_gpio_task, "VibMotorGpio", 128, NULL, 1, NULL);
}
