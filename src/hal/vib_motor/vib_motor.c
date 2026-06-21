#include "vib_motor.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bsp_pwm.h"
#include "bsp_time.h"
#include "freertos_alloc.h"
#include "task.h"
#include "vector.h"

static Vector* vib_motor_instances;

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

static const Vib_motor_step pattern_life_lost_steps[] = {{45u, 80u, 40u}, {35u, 80u, 0u}};
static const Vib_motor_pattern pattern_life_lost = {pattern_life_lost_steps, 2u};
static const Vib_motor_step pattern_victory_steps[] = {{30u, 40u, 50u}, {30u, 40u, 0u}};
static const Vib_motor_pattern pattern_victory = {pattern_victory_steps, 2u};

static const Vib_motor_pattern* const effect_patterns[vib_effect_count] = {&pattern_menu_tick,
    &pattern_menu_select, &pattern_back, &pattern_action_light, &pattern_jump, &pattern_shot, &pattern_pickup,
    &pattern_score, &pattern_merge, &pattern_hit_light, &pattern_hit_heavy, &pattern_life_lost,
    &pattern_victory, &pattern_defeat};

static const uint8_t effect_priorities[vib_effect_count] = {
    1u, 2u, 2u, 2u, 2u, 2u, 3u, 3u, 3u, 4u, 4u, 5u, 5u, 5u};

static uint8_t clamp_percent(uint8_t percent) { return percent > 100u ? 100u : percent; }

static void stop_output(Vib_motor* obj) {
    if (obj == NULL || !obj->output_on) { return; }
    Bsp_Pwm_Stop(obj->config.pwm_idx);
    obj->output_on = 0u;
}

static void output_strength(Vib_motor* obj, uint8_t strength_percent) {
    strength_percent = clamp_percent(strength_percent);
    if (!obj->enabled || strength_percent == 0u || obj->config.master_strength_percent == 0u) {
        stop_output(obj);
        return;
    }
    const uint32_t duty_percent = (uint32_t)strength_percent * obj->config.master_strength_percent *
                                  obj->config.max_duty_percent / 10000u;
    Bsp_Pwm_Stop(obj->config.pwm_idx);
    Bsp_Pwm_Set_Freq(obj->config.pwm_idx, obj->config.pwm_freq_hz);
    Bsp_Pwm_Set_Duty(obj->config.pwm_idx, (float)duty_percent / 100.0f);
    Bsp_Pwm_Start(obj->config.pwm_idx);
    obj->output_on = 1u;
}

static uint8_t can_accept(Vib_motor* obj, uint8_t priority, uint32_t now) {
    if (!obj->enabled) { return 0u; }
    if (obj->active && priority < obj->priority) { return 0u; }
    const uint8_t within_cooldown = obj->has_played && now - obj->last_play_at < VIB_MOTOR_MIN_RETRIGGER_MS;
    if (within_cooldown && (!obj->active || priority <= obj->priority)) { return 0u; }
    obj->has_played = 1u;
    obj->last_play_at = now;
    obj->priority = priority;
    return 1u;
}

static void finish_playback(Vib_motor* obj) {
    obj->active = 0u;
    obj->priority = 0u;
    obj->pattern = NULL;
    obj->step_index = 0u;
    obj->in_gap = 0u;
    stop_output(obj);
}

static void start_pattern_step(Vib_motor* obj) {
    const Vib_motor_step* step = &obj->pattern->steps[obj->step_index];
    obj->requested_strength = step->strength_percent;
    obj->duration_ms = step->duration_ms;
    obj->in_gap = 0u;
    output_strength(obj, obj->requested_strength);
}

static void update_pattern(Vib_motor* obj, uint32_t now) {
    while (obj->active && obj->pattern != NULL) {
        const Vib_motor_step* step = &obj->pattern->steps[obj->step_index];
        const uint32_t elapsed = now - obj->started_at;
        if (!obj->in_gap) {
            if (elapsed < step->duration_ms) { return; }
            stop_output(obj);
            obj->started_at += step->duration_ms;
            if (step->gap_ms > 0u) {
                obj->in_gap = 1u;
                continue;
            }
        } else {
            if (elapsed < step->gap_ms) { return; }
            obj->started_at += step->gap_ms;
        }

        obj->step_index++;
        if (obj->step_index >= obj->pattern->length) {
            finish_playback(obj);
            return;
        }
        start_pattern_step(obj);
    }
}

void Vib_Motor_Init(void) {
    if (vib_motor_instances == NULL) { vib_motor_instances = Vector_Init(sizeof(Vib_motor*), 4u); }
}

Vib_motor* Vib_Motor_Create(const Vib_motor_config* config) {
    if (config == NULL || vib_motor_instances == NULL) { return NULL; }
    Vib_motor* obj = (Vib_motor*)pvPortMalloc(sizeof(*obj));
    if (obj == NULL) { return NULL; }
    memset(obj, 0, sizeof(*obj));
    obj->config = *config;
    obj->config.pwm_freq_hz = config->pwm_freq_hz == 0u ? VIB_MOTOR_DEFAULT_PWM_FREQ_HZ : config->pwm_freq_hz;
    obj->config.max_duty_percent = clamp_percent(config->max_duty_percent);
    obj->config.master_strength_percent = clamp_percent(config->master_strength_percent);
    obj->enabled = 1u;
    Vector_Push_Back(vib_motor_instances, (void*)&obj);
    return obj;
}

void Vib_Motor_Play(Vib_motor* obj, uint8_t strength_percent, uint16_t duration_ms) {
    if (obj == NULL || !obj->enabled) { return; }
    taskENTER_CRITICAL();
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (!can_accept(obj, 2u, now)) {
        taskEXIT_CRITICAL();
        return;
    }
    obj->requested_strength = clamp_percent(strength_percent);
    obj->duration_ms = duration_ms;
    obj->started_at = now;
    obj->active = 1u;
    obj->pattern = NULL;
    obj->step_index = 0u;
    obj->in_gap = 0u;
    output_strength(obj, obj->requested_strength);
    taskEXIT_CRITICAL();
}

void Vib_Motor_Play_Effect(Vib_motor* obj, Vib_motor_effect effect) {
    if (obj == NULL || (unsigned)effect >= vib_effect_count) { return; }
    taskENTER_CRITICAL();
    const uint32_t now = Bsp_Get_Tick_Ms();
    const uint8_t priority = effect_priorities[effect];
    if (!can_accept(obj, priority, now)) {
        taskEXIT_CRITICAL();
        return;
    }
    obj->pattern = effect_patterns[effect];
    obj->step_index = 0u;
    obj->started_at = now;
    obj->active = 1u;
    start_pattern_step(obj);
    taskEXIT_CRITICAL();
}

void Vib_Motor_Stop(Vib_motor* obj) {
    if (obj == NULL) { return; }
    taskENTER_CRITICAL();
    finish_playback(obj);
    taskEXIT_CRITICAL();
}

void Vib_Motor_Update_All(void) {
    if (vib_motor_instances == NULL) { return; }
    const uint32_t now = Bsp_Get_Tick_Ms();
    taskENTER_CRITICAL();
    for (uint32_t i = 0; i < Vector_Get_Size(vib_motor_instances); i++) {
        Vib_motor* obj = *(Vib_motor**)Vector_Get_At(vib_motor_instances, i);
        if (obj == NULL || !obj->active) { continue; }
        if (obj->pattern != NULL) {
            update_pattern(obj, now);
        } else if (now - obj->started_at >= obj->duration_ms) {
            finish_playback(obj);
        }
    }
    taskEXIT_CRITICAL();
}

void Vib_Motor_Set_Master_Strength(Vib_motor* obj, uint8_t strength_percent) {
    if (obj == NULL) { return; }
    taskENTER_CRITICAL();
    obj->config.master_strength_percent = clamp_percent(strength_percent);
    if (obj->active && !obj->in_gap) { output_strength(obj, obj->requested_strength); }
    taskEXIT_CRITICAL();
}

uint8_t Vib_Motor_Get_Master_Strength(Vib_motor* obj) {
    return obj == NULL ? 0u : obj->config.master_strength_percent;
}

void Vib_Motor_Set_Enabled(Vib_motor* obj, uint8_t enabled) {
    if (obj == NULL) { return; }
    taskENTER_CRITICAL();
    obj->enabled = enabled != 0u;
    if (!obj->enabled) { finish_playback(obj); }
    taskEXIT_CRITICAL();
}

uint8_t Vib_Motor_Is_Enabled(Vib_motor* obj) { return obj == NULL ? 0u : obj->enabled; }
