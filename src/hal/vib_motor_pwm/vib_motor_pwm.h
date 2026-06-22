#pragma once

#include <stdint.h>

/* The PWM pin must drive the motor through a transistor or MOSFET stage with
 * a flyback diode or other suitable protection. Never drive a motor directly
 * from a GPIO/PWM pin. */

#define VIB_MOTOR_PWM_DEFAULT_FREQ_HZ      20000u
#define VIB_MOTOR_PWM_DEFAULT_MAX_DUTY_PERCENT 60u
#define VIB_MOTOR_PWM_DEFAULT_MASTER_STRENGTH  70u
#define VIB_MOTOR_PWM_MIN_RETRIGGER_MS         20u

#define PWM_VIB_MOTOR_PWM_IDX              PWM_BUZZER_IDX /* placeholder: no dedicated PWM channel */

typedef enum {
    vib_pwm_effect_menu_tick = 0,
    vib_pwm_effect_menu_select,
    vib_pwm_effect_back,
    vib_pwm_effect_action_light,
    vib_pwm_effect_jump,
    vib_pwm_effect_shot,
    vib_pwm_effect_pickup,
    vib_pwm_effect_score,
    vib_pwm_effect_merge,
    vib_pwm_effect_hit_light,
    vib_pwm_effect_hit_heavy,
    vib_pwm_effect_life_lost,
    vib_pwm_effect_victory,
    vib_pwm_effect_defeat,
    vib_pwm_effect_count
} Vib_motor_pwm_effect;

typedef struct {
    uint8_t pwm_idx;
    uint32_t pwm_freq_hz;
    uint8_t max_duty_percent;
    uint8_t master_strength_percent;
} Vib_motor_pwm_config;

typedef struct {
    uint8_t strength_percent;
    uint16_t duration_ms;
    uint16_t gap_ms;
} Vib_motor_pwm_step;

typedef struct {
    const Vib_motor_pwm_step* steps;
    uint8_t length;
} Vib_motor_pwm_pattern;

typedef struct Vib_motor_pwm_t {
    Vib_motor_pwm_config config;
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
    const Vib_motor_pwm_pattern* pattern;
} Vib_motor_pwm;

void Vib_Motor_Pwm_Init(void);
Vib_motor_pwm* Vib_Motor_Pwm_Create(const Vib_motor_pwm_config* config);
void Vib_Motor_Pwm_Play(Vib_motor_pwm* obj, uint8_t strength_percent, uint16_t duration_ms);
void Vib_Motor_Pwm_Play_Effect(Vib_motor_pwm* obj, Vib_motor_pwm_effect effect);
void Vib_Motor_Pwm_Stop(Vib_motor_pwm* obj);
void Vib_Motor_Pwm_Update_All(void);
void Vib_Motor_Pwm_Set_Master_Strength(Vib_motor_pwm* obj, uint8_t strength_percent);
uint8_t Vib_Motor_Pwm_Get_Master_Strength(Vib_motor_pwm* obj);
void Vib_Motor_Pwm_Set_Enabled(Vib_motor_pwm* obj, uint8_t enabled);
uint8_t Vib_Motor_Pwm_Is_Enabled(Vib_motor_pwm* obj);
