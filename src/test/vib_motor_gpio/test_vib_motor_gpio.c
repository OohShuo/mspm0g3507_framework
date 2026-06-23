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
        for (int i = 0; i < vib_gpio_effect_count; i++) {
            Vib_Motor_Gpio_Play_Effect(g_motor, (Vib_motor_gpio_effect)i);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void Test_Vib_Motor_Gpio_Task_Def(void) {
    xTaskCreate(vib_motor_gpio_task, "VibMotorGpio", 128, NULL, 1, NULL);
}
