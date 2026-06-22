#pragma once

#include <stdint.h>

/*
 * GPIO vibration motor driver.
 *
 * HARDWARE NOTE:
 * The MCU GPIO only outputs a low-current control signal.
 * The actual hardware MUST drive the vibration motor through a
 * transistor or MOSFET stage with a flyback diode or other suitable
 * protection.  Never drive a motor directly from a GPIO pin.
 *
 * GPIO_VIB_MOTOR_IDX maps to the gate/base of the driver transistor,
 * NOT to the motor supply pin.
 */

#define VIB_MOTOR_GPIO_MIN_RETRIGGER_MS 20u

typedef enum {
    vib_gpio_effect_menu_tick = 0,
    vib_gpio_effect_menu_select,
    vib_gpio_effect_back,
    vib_gpio_effect_action_light,
    vib_gpio_effect_jump,
    vib_gpio_effect_shot,
    vib_gpio_effect_pickup,
    vib_gpio_effect_score,
    vib_gpio_effect_merge,
    vib_gpio_effect_hit_light,
    vib_gpio_effect_hit_heavy,
    vib_gpio_effect_life_lost,
    vib_gpio_effect_victory,
    vib_gpio_effect_defeat,
    vib_gpio_effect_count
} Vib_motor_gpio_effect;

/* Compatibility macros so APP-layer game code can keep using vib_effect_xxx names. */
#define vib_effect_menu_tick      vib_gpio_effect_menu_tick
#define vib_effect_menu_select    vib_gpio_effect_menu_select
#define vib_effect_back           vib_gpio_effect_back
#define vib_effect_action_light   vib_gpio_effect_action_light
#define vib_effect_jump           vib_gpio_effect_jump
#define vib_effect_shot           vib_gpio_effect_shot
#define vib_effect_pickup         vib_gpio_effect_pickup
#define vib_effect_score          vib_gpio_effect_score
#define vib_effect_merge          vib_gpio_effect_merge
#define vib_effect_hit_light      vib_gpio_effect_hit_light
#define vib_effect_hit_heavy      vib_gpio_effect_hit_heavy
#define vib_effect_life_lost      vib_gpio_effect_life_lost
#define vib_effect_victory        vib_gpio_effect_victory
#define vib_effect_defeat         vib_gpio_effect_defeat

typedef struct {
    uint8_t gpio_idx;
    uint8_t active_high;
    uint8_t enabled;
} Vib_motor_gpio_config;

typedef struct {
    uint16_t on_ms;
    uint16_t off_ms;
} Vib_motor_gpio_step;

typedef struct {
    const Vib_motor_gpio_step* steps;
    uint8_t length;
} Vib_motor_gpio_pattern;

typedef struct Vib_motor_gpio_t {
    Vib_motor_gpio_config config;

    uint8_t active;
    uint8_t output_on;
    uint8_t priority;
    uint8_t has_played;
    uint8_t step_index;
    uint8_t in_gap;

    uint32_t started_at;
    uint32_t last_play_at;

    uint16_t duration_ms;
    const Vib_motor_gpio_pattern* pattern;
} Vib_motor_gpio;

void Vib_Motor_Gpio_Init(void);
Vib_motor_gpio* Vib_Motor_Gpio_Create(const Vib_motor_gpio_config* config);

void Vib_Motor_Gpio_Play(Vib_motor_gpio* obj, uint16_t duration_ms);
void Vib_Motor_Gpio_Play_Effect(Vib_motor_gpio* obj, Vib_motor_gpio_effect effect);
void Vib_Motor_Gpio_Stop(Vib_motor_gpio* obj);
void Vib_Motor_Gpio_Update_All(void);

void Vib_Motor_Gpio_Set_Enabled(Vib_motor_gpio* obj, uint8_t enabled);
uint8_t Vib_Motor_Gpio_Is_Enabled(Vib_motor_gpio* obj);
