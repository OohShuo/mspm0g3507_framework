#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bsp_time.h"
#include "haptics_vm.h"
#include "task.h"
#include "vib_motor_gpio.h"

static Vib_motor_gpio g_motor;

/* ── GPIO vibration patterns ─────────────────────────────────────────── */

#define SINGLE_GPIO_STEP(name, on_time)                                \
    static const Vib_motor_gpio_step name##_steps[] = {{on_time, 0u}}; \
    static const Vib_motor_gpio_pattern name = {name##_steps, 1u}

SINGLE_GPIO_STEP(pattern_menu_tick, 200u);
SINGLE_GPIO_STEP(pattern_menu_select, 200u);
SINGLE_GPIO_STEP(pattern_back, 225u);
SINGLE_GPIO_STEP(pattern_action_light, 200u);
SINGLE_GPIO_STEP(pattern_jump, 200u);
SINGLE_GPIO_STEP(pattern_shot, 200u);
SINGLE_GPIO_STEP(pattern_pickup, 200u);
SINGLE_GPIO_STEP(pattern_score, 200u);
SINGLE_GPIO_STEP(pattern_merge, 200u);
SINGLE_GPIO_STEP(pattern_hit_light, 225u);
SINGLE_GPIO_STEP(pattern_hit_heavy, 450u);

static const Vib_motor_gpio_step g_life_lost_steps[] = {{400u, 200u}, {400u, 0u}};
static const Vib_motor_gpio_pattern g_life_lost = {g_life_lost_steps, 2u};
static const Vib_motor_gpio_step g_victory_steps[] = {{200u, 250u}, {200u, 0u}};
static const Vib_motor_gpio_pattern g_victory = {g_victory_steps, 2u};
static const Vib_motor_gpio_step g_defeat_steps[] = {{300u, 200u}, {300u, 200u}, {800u, 0u}};
static const Vib_motor_gpio_pattern g_defeat = {g_defeat_steps, 3u};

static const Vib_motor_gpio_pattern* const g_effect_patterns[vib_gpio_effect_count] = {&pattern_menu_tick,
    &pattern_menu_select, &pattern_back, &pattern_action_light, &pattern_jump, &pattern_shot, &pattern_pickup,
    &pattern_score, &pattern_merge, &pattern_hit_light, &pattern_hit_heavy, &g_life_lost, &g_victory,
    &g_defeat};
static const uint8_t g_effect_priorities[vib_gpio_effect_count] = {
    1u, 2u, 2u, 2u, 2u, 2u, 3u, 3u, 3u, 4u, 4u, 5u, 5u, 5u};

/* ── VM-specific GPIO simulation ─────────────────────────────────────── */

static void output_on(Vib_motor_gpio* motor) {
    if (motor == NULL || !motor->config.enabled) { return; }
    motor->output_on = 1u;
    Vm_Haptics_Set_Strength(100u);
}

static void output_off(Vib_motor_gpio* motor) {
    if (motor == NULL) { return; }
    motor->output_on = 0u;
    Vm_Haptics_Set_Strength(0u);
}

/* ── Priority / debounce ─────────────────────────────────────────────── */

static uint8_t can_accept(Vib_motor_gpio* motor, uint8_t priority, uint32_t now) {
    if (!motor->config.enabled || (motor->active && priority < motor->priority)) { return 0u; }
    const uint8_t within_cooldown =
        motor->has_played && now - motor->last_play_at < VIB_MOTOR_GPIO_MIN_RETRIGGER_MS;
    if (within_cooldown && (!motor->active || priority <= motor->priority)) { return 0u; }
    motor->has_played = 1u;
    motor->last_play_at = now;
    motor->priority = priority;
    return 1u;
}

static void finish_playback(Vib_motor_gpio* motor) {
    motor->active = 0u;
    motor->priority = 0u;
    motor->pattern = NULL;
    motor->step_index = 0u;
    motor->in_gap = 0u;
    output_off(motor);
}

static void start_pattern_step(Vib_motor_gpio* motor) {
    const Vib_motor_gpio_step* step = &motor->pattern->steps[motor->step_index];
    motor->duration_ms = step->on_ms;
    motor->in_gap = 0u;
    output_on(motor);
}

static void update_pattern(Vib_motor_gpio* motor, uint32_t now) {
    while (motor->active && motor->pattern != NULL) {
        const Vib_motor_gpio_step* step = &motor->pattern->steps[motor->step_index];
        const uint32_t elapsed = now - motor->started_at;
        if (!motor->in_gap) {
            if (elapsed < step->on_ms) { return; }
            output_off(motor);
            motor->started_at += step->on_ms;
            if (step->off_ms > 0u) {
                motor->in_gap = 1u;
                continue;
            }
        } else {
            if (elapsed < step->off_ms) { return; }
            motor->started_at += step->off_ms;
        }
        motor->step_index++;
        if (motor->step_index >= motor->pattern->length) {
            finish_playback(motor);
            return;
        }
        start_pattern_step(motor);
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void Vib_Motor_Gpio_Init(void) {
    memset(&g_motor, 0, sizeof(g_motor));
    Vm_Haptics_Set_Strength(0u);
}

Vib_motor_gpio* Vib_Motor_Gpio_Create(const Vib_motor_gpio_config* config) {
    if (config == NULL) { return NULL; }
    memset(&g_motor, 0, sizeof(g_motor));
    g_motor.config = *config;
    return &g_motor;
}

void Vib_Motor_Gpio_Play(Vib_motor_gpio* motor, uint16_t duration_ms) {
    if (motor == NULL) { return; }
    taskENTER_CRITICAL();
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (can_accept(motor, 2u, now)) {
        motor->duration_ms = duration_ms;
        motor->started_at = now;
        motor->active = 1u;
        motor->pattern = NULL;
        output_on(motor);
    }
    taskEXIT_CRITICAL();
}

void Vib_Motor_Gpio_Play_Effect(Vib_motor_gpio* motor, Vib_motor_gpio_effect effect) {
    if (motor == NULL || (unsigned)effect >= vib_gpio_effect_count) { return; }
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

void Vib_Motor_Gpio_Stop(Vib_motor_gpio* motor) {
    if (motor == NULL) { return; }
    taskENTER_CRITICAL();
    finish_playback(motor);
    taskEXIT_CRITICAL();
}

void Vib_Motor_Gpio_Update_All(void) {
    taskENTER_CRITICAL();
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (g_motor.active && g_motor.pattern != NULL) {
        update_pattern(&g_motor, now);
    } else if (g_motor.active && now - g_motor.started_at >= g_motor.duration_ms) {
        finish_playback(&g_motor);
    }
    taskEXIT_CRITICAL();
}

void Vib_Motor_Gpio_Set_Enabled(Vib_motor_gpio* motor, uint8_t enabled) {
    if (motor == NULL) { return; }
    motor->config.enabled = enabled != 0u;
    if (!motor->config.enabled) { finish_playback(motor); }
}

uint8_t Vib_Motor_Gpio_Is_Enabled(Vib_motor_gpio* motor) { return motor != NULL && motor->config.enabled; }
