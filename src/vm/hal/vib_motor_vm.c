#include <stddef.h>

#include "vib_motor.h"

static int vib_motor_dummy;
static uint8_t vib_motor_master_strength = VIB_MOTOR_DEFAULT_MASTER_STRENGTH;
static uint8_t vib_motor_enabled = 1u;

static uint8_t clamp_percent(uint8_t percent) { return percent > 100u ? 100u : percent; }

void Vib_Motor_Init(void) {
    vib_motor_master_strength = VIB_MOTOR_DEFAULT_MASTER_STRENGTH;
    vib_motor_enabled = 1u;
}

Vib_motor* Vib_Motor_Create(const Vib_motor_config* config) {
    if (config == NULL) { return NULL; }
    vib_motor_master_strength = clamp_percent(config->master_strength_percent);
    vib_motor_enabled = 1u;
    return (Vib_motor*)&vib_motor_dummy;
}

void Vib_Motor_Play(Vib_motor* obj, uint8_t strength_percent, uint16_t duration_ms) {
    (void)obj;
    (void)strength_percent;
    (void)duration_ms;
}

void Vib_Motor_Play_Effect(Vib_motor* obj, Vib_motor_effect effect) {
    (void)obj;
    (void)effect;
}

void Vib_Motor_Stop(Vib_motor* obj) { (void)obj; }

void Vib_Motor_Update_All(void) {}

void Vib_Motor_Set_Master_Strength(Vib_motor* obj, uint8_t strength_percent) {
    if (obj != NULL) { vib_motor_master_strength = clamp_percent(strength_percent); }
}

uint8_t Vib_Motor_Get_Master_Strength(Vib_motor* obj) { return obj == NULL ? 0u : vib_motor_master_strength; }

void Vib_Motor_Set_Enabled(Vib_motor* obj, uint8_t enabled) {
    if (obj != NULL) { vib_motor_enabled = enabled != 0u; }
}

uint8_t Vib_Motor_Is_Enabled(Vib_motor* obj) { return obj == NULL ? 0u : vib_motor_enabled; }
