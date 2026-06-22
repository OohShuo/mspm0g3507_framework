#include <stddef.h>
#include <string.h>

#include "haptics_vm.h"
#include "vib_motor_pwm.h"

static Vib_motor_pwm g_motor;

static uint8_t clamp_percent(uint8_t percent) { return percent > 100u ? 100u : percent; }

void Vib_Motor_Pwm_Init(void) {
    memset(&g_motor, 0, sizeof(g_motor));
}

Vib_motor_pwm* Vib_Motor_Pwm_Create(const Vib_motor_pwm_config* config) {
    if (config == NULL) { return NULL; }
    memset(&g_motor, 0, sizeof(g_motor));
    g_motor.config = *config;
    g_motor.config.master_strength_percent = clamp_percent(config->master_strength_percent);
    g_motor.config.max_duty_percent = clamp_percent(config->max_duty_percent);
    g_motor.enabled = 1u;
    return &g_motor;
}

void Vib_Motor_Pwm_Play(Vib_motor_pwm* motor, uint8_t strength_percent, uint16_t duration_ms) {
    (void)motor;
    (void)strength_percent;
    (void)duration_ms;
}

void Vib_Motor_Pwm_Play_Effect(Vib_motor_pwm* motor, Vib_motor_pwm_effect effect) {
    (void)motor;
    (void)effect;
}

void Vib_Motor_Pwm_Stop(Vib_motor_pwm* motor) {
    (void)motor;
}

void Vib_Motor_Pwm_Update_All(void) {
    /* PWM vib_motor is not simulated on VM — use GPIO version instead. */
}

void Vib_Motor_Pwm_Set_Master_Strength(Vib_motor_pwm* motor, uint8_t strength_percent) {
    (void)motor;
    (void)strength_percent;
}

uint8_t Vib_Motor_Pwm_Get_Master_Strength(Vib_motor_pwm* motor) {
    return motor == NULL ? 0u : motor->config.master_strength_percent;
}

void Vib_Motor_Pwm_Set_Enabled(Vib_motor_pwm* motor, uint8_t enabled) {
    if (motor == NULL) { return; }
    motor->enabled = enabled != 0u;
}

uint8_t Vib_Motor_Pwm_Is_Enabled(Vib_motor_pwm* motor) {
    return motor != NULL && motor->enabled;
}
