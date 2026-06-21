#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bsp_time.h"
#include "haptics_vm.h"
#include "task.h"
#include "vib_motor.h"

static Vib_motor g_motor;

#define SINGLE_STEP(name, strength, duration)                                \
    static const Vib_motor_step name##_steps[] = {{strength, duration, 0u}}; \
    static const Vib_motor_pattern name = {name##_steps, 1u}

SINGLE_STEP(pattern_menu_tick, 10u, 12u);
SINGLE_STEP(pattern_menu_select, 20u, 35u);
SINGLE_STEP(pattern_back, 22u, 45u);
SINGLE_STEP(pattern_action_light, 12u, 18u);
SINGLE_STEP(pattern_jump, 14u, 18u);
SINGLE_STEP(pattern_shot, 15u, 20u);
SINGLE_STEP(pattern_pickup, 22u, 30u);
SINGLE_STEP(pattern_score, 18u, 25u);
SINGLE_STEP(pattern_merge, 24u, 35u);
SINGLE_STEP(pattern_hit_light, 28u, 45u);
SINGLE_STEP(pattern_hit_heavy, 45u, 90u);
SINGLE_STEP(pattern_defeat, 50u, 120u);

static const Vib_motor_step g_life_lost_steps[] = {{45u, 80u, 40u}, {35u, 80u, 0u}};
static const Vib_motor_pattern g_life_lost = {g_life_lost_steps, 2u};
static const Vib_motor_step g_victory_steps[] = {{30u, 40u, 50u}, {30u, 40u, 0u}};
static const Vib_motor_pattern g_victory = {g_victory_steps, 2u};

static const Vib_motor_pattern* const g_effect_patterns[vib_effect_count] = {&pattern_menu_tick,
    &pattern_menu_select, &pattern_back, &pattern_action_light, &pattern_jump, &pattern_shot, &pattern_pickup,
    &pattern_score, &pattern_merge, &pattern_hit_light, &pattern_hit_heavy, &g_life_lost, &g_victory,
    &pattern_defeat};
static const uint8_t g_effect_priorities[vib_effect_count] = {
    1u, 2u, 2u, 2u, 2u, 2u, 3u, 3u, 3u, 4u, 4u, 5u, 5u, 5u};

static uint8_t clamp_percent(uint8_t percent) { return percent > 100u ? 100u : percent; }

static void stop_output(Vib_motor* motor) {
    motor->output_on = 0u;
    Vm_Haptics_Set_Strength(0u);
}

static void output_strength(Vib_motor* motor, uint8_t requested_strength) {
    if (!motor->enabled || requested_strength == 0u || motor->config.master_strength_percent == 0u) {
        stop_output(motor);
        return;
    }
    const uint8_t effective =
        (uint8_t)((uint16_t)requested_strength * motor->config.master_strength_percent / 100u);
    motor->output_on = 1u;
    Vm_Haptics_Set_Strength(effective);
}

static uint8_t can_accept(Vib_motor* motor, uint8_t priority, uint32_t now) {
    if (!motor->enabled || (motor->active && priority < motor->priority)) { return 0u; }
    const uint8_t within_cooldown =
        motor->has_played && now - motor->last_play_at < VIB_MOTOR_MIN_RETRIGGER_MS;
    if (within_cooldown && (!motor->active || priority <= motor->priority)) { return 0u; }
    motor->has_played = 1u;
    motor->last_play_at = now;
    motor->priority = priority;
    return 1u;
}

static void finish_playback(Vib_motor* motor) {
    motor->active = 0u;
    motor->priority = 0u;
    motor->pattern = NULL;
    motor->step_index = 0u;
    motor->in_gap = 0u;
    stop_output(motor);
}

static void start_pattern_step(Vib_motor* motor) {
    const Vib_motor_step* step = &motor->pattern->steps[motor->step_index];
    motor->requested_strength = step->strength_percent;
    motor->duration_ms = step->duration_ms;
    motor->in_gap = 0u;
    output_strength(motor, motor->requested_strength);
}

static void update_pattern(Vib_motor* motor, uint32_t now) {
    while (motor->active && motor->pattern != NULL) {
        const Vib_motor_step* step = &motor->pattern->steps[motor->step_index];
        const uint32_t elapsed = now - motor->started_at;
        if (!motor->in_gap) {
            if (elapsed < step->duration_ms) { return; }
            stop_output(motor);
            motor->started_at += step->duration_ms;
            if (step->gap_ms > 0u) {
                motor->in_gap = 1u;
                continue;
            }
        } else {
            if (elapsed < step->gap_ms) { return; }
            motor->started_at += step->gap_ms;
        }
        motor->step_index++;
        if (motor->step_index >= motor->pattern->length) {
            finish_playback(motor);
            return;
        }
        start_pattern_step(motor);
    }
}

void Vib_Motor_Init(void) {
    memset(&g_motor, 0, sizeof(g_motor));
    Vm_Haptics_Set_Strength(0u);
}

Vib_motor* Vib_Motor_Create(const Vib_motor_config* config) {
    if (config == NULL) { return NULL; }
    memset(&g_motor, 0, sizeof(g_motor));
    g_motor.config = *config;
    g_motor.config.master_strength_percent = clamp_percent(config->master_strength_percent);
    g_motor.config.max_duty_percent = clamp_percent(config->max_duty_percent);
    g_motor.enabled = 1u;
    return &g_motor;
}

void Vib_Motor_Play(Vib_motor* motor, uint8_t strength_percent, uint16_t duration_ms) {
    if (motor == NULL) { return; }
    taskENTER_CRITICAL();
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (can_accept(motor, 2u, now)) {
        motor->requested_strength = clamp_percent(strength_percent);
        motor->duration_ms = duration_ms;
        motor->started_at = now;
        motor->active = 1u;
        motor->pattern = NULL;
        output_strength(motor, motor->requested_strength);
    }
    taskEXIT_CRITICAL();
}

void Vib_Motor_Play_Effect(Vib_motor* motor, Vib_motor_effect effect) {
    if (motor == NULL || (unsigned)effect >= vib_effect_count) { return; }
    taskENTER_CRITICAL();
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (can_accept(motor, g_effect_priorities[effect], now)) {
        motor->pattern = g_effect_patterns[effect];
        motor->step_index = 0u;
        motor->started_at = now;
        motor->active = 1u;
        start_pattern_step(motor);
    }
    taskEXIT_CRITICAL();
}

void Vib_Motor_Stop(Vib_motor* motor) {
    if (motor == NULL) { return; }
    taskENTER_CRITICAL();
    finish_playback(motor);
    taskEXIT_CRITICAL();
}

void Vib_Motor_Update_All(void) {
    taskENTER_CRITICAL();
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (g_motor.active && g_motor.pattern != NULL) {
        update_pattern(&g_motor, now);
    } else if (g_motor.active && now - g_motor.started_at >= g_motor.duration_ms) {
        finish_playback(&g_motor);
    }
    taskEXIT_CRITICAL();
}

void Vib_Motor_Set_Master_Strength(Vib_motor* motor, uint8_t strength_percent) {
    if (motor == NULL) { return; }
    motor->config.master_strength_percent = clamp_percent(strength_percent);
    if (motor->active && !motor->in_gap) { output_strength(motor, motor->requested_strength); }
}

uint8_t Vib_Motor_Get_Master_Strength(Vib_motor* motor) {
    return motor == NULL ? 0u : motor->config.master_strength_percent;
}

void Vib_Motor_Set_Enabled(Vib_motor* motor, uint8_t enabled) {
    if (motor == NULL) { return; }
    motor->enabled = enabled != 0u;
    if (!motor->enabled) { finish_playback(motor); }
}

uint8_t Vib_Motor_Is_Enabled(Vib_motor* motor) { return motor != NULL && motor->enabled; }
