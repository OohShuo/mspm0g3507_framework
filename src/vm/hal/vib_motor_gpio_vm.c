#include <stddef.h>
#include <string.h>

#include "vib_motor_gpio.h"
#include "vib_motor_pwm.h"

static Vib_motor_gpio g_motor;
static Vib_motor_pwm* g_pwm_motor;

void Vib_Motor_Gpio_Init(void) {
    memset(&g_motor, 0, sizeof(g_motor));
    Vib_Motor_Pwm_Init();
}

Vib_motor_gpio* Vib_Motor_Gpio_Create(const Vib_motor_gpio_config* config) {
    if (config == NULL) { return NULL; }
    memset(&g_motor, 0, sizeof(g_motor));
    g_motor.config = *config;

    static const Vib_motor_pwm_config default_pwm_config = {
        .pwm_idx = 0u,
        .pwm_freq_hz = 20000u,
        .max_duty_percent = 100u,
        .master_strength_percent = 70u,
    };
    g_pwm_motor = Vib_Motor_Pwm_Create(&default_pwm_config);

    return &g_motor;
}

void Vib_Motor_Gpio_Play(Vib_motor_gpio* motor, uint16_t duration_ms) {
    if (motor == NULL || g_pwm_motor == NULL) { return; }
    Vib_Motor_Pwm_Play(g_pwm_motor, 100u, duration_ms);
}

void Vib_Motor_Gpio_Play_Effect(Vib_motor_gpio* motor, Vib_motor_gpio_effect effect) {
    if (motor == NULL || g_pwm_motor == NULL) { return; }
    Vib_Motor_Pwm_Play_Effect(g_pwm_motor, (Vib_motor_pwm_effect)effect);
}

void Vib_Motor_Gpio_Stop(Vib_motor_gpio* motor) {
    if (motor == NULL || g_pwm_motor == NULL) { return; }
    Vib_Motor_Pwm_Stop(g_pwm_motor);
}

void Vib_Motor_Gpio_Update_All(void) { Vib_Motor_Pwm_Update_All(); }

void Vib_Motor_Gpio_Set_Enabled(Vib_motor_gpio* motor, uint8_t enabled) {
    if (motor == NULL) { return; }
    motor->config.enabled = enabled != 0u;
    if (g_pwm_motor != NULL) { Vib_Motor_Pwm_Set_Enabled(g_pwm_motor, enabled); }
}

uint8_t Vib_Motor_Gpio_Is_Enabled(Vib_motor_gpio* motor) { return motor != NULL && motor->config.enabled; }
