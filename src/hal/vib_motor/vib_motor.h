#pragma once

#include <stdint.h>

/* The PWM pin must drive the motor through a transistor or MOSFET stage with
 * a flyback diode or other suitable protection. Never drive a motor directly
 * from a GPIO/PWM pin. */

#define VIB_MOTOR_DEFAULT_PWM_FREQ_HZ      20000u
#define VIB_MOTOR_DEFAULT_MAX_DUTY_PERCENT 60u
#define VIB_MOTOR_DEFAULT_MASTER_STRENGTH  70u
#define VIB_MOTOR_MIN_RETRIGGER_MS         20u

typedef enum {
    vib_effect_menu_tick = 0,
    vib_effect_menu_select,
    vib_effect_back,
    vib_effect_action_light,
    vib_effect_jump,
    vib_effect_shot,
    vib_effect_pickup,
    vib_effect_score,
    vib_effect_merge,
    vib_effect_hit_light,
    vib_effect_hit_heavy,
    vib_effect_life_lost,
    vib_effect_victory,
    vib_effect_defeat,
    vib_effect_count
} Vib_motor_effect;

typedef struct {
    uint8_t pwm_idx;
    uint32_t pwm_freq_hz;
    uint8_t max_duty_percent;
    uint8_t master_strength_percent;
} Vib_motor_config;

typedef struct {
    uint8_t strength_percent;
    uint16_t duration_ms;
    uint16_t gap_ms;
} Vib_motor_step;

typedef struct {
    const Vib_motor_step* steps;
    uint8_t length;
} Vib_motor_pattern;

typedef struct Vib_motor_t {
    Vib_motor_config config;
    uint32_t started_at;
    uint16_t duration_ms;
    uint8_t requested_strength;
    uint8_t active;
    uint8_t output_on;
    uint8_t enabled;
    uint8_t priority;
    uint8_t has_played;
    uint8_t step_index;
    uint8_t in_gap;
    uint32_t last_play_at;
    const Vib_motor_pattern* pattern;
} Vib_motor;

void Vib_Motor_Init(void);
Vib_motor* Vib_Motor_Create(const Vib_motor_config* config);
void Vib_Motor_Play(Vib_motor* obj, uint8_t strength_percent, uint16_t duration_ms);
void Vib_Motor_Play_Effect(Vib_motor* obj, Vib_motor_effect effect);
void Vib_Motor_Stop(Vib_motor* obj);
void Vib_Motor_Update_All(void);
void Vib_Motor_Set_Master_Strength(Vib_motor* obj, uint8_t strength_percent);
uint8_t Vib_Motor_Get_Master_Strength(Vib_motor* obj);
void Vib_Motor_Set_Enabled(Vib_motor* obj, uint8_t enabled);
uint8_t Vib_Motor_Is_Enabled(Vib_motor* obj);
