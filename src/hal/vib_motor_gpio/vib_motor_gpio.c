#include "vib_motor_gpio.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bsp_gpio.h"
#include "bsp_time.h"
#include "freertos_alloc.h"
#include "task.h"
#include "vector.h"

static Vector* vib_motor_gpio_instances;

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

static const Vib_motor_gpio_step pattern_life_lost_steps[] = {{400u, 200u}, {400u, 0u}};
static const Vib_motor_gpio_pattern pattern_life_lost = {pattern_life_lost_steps, 2u};

static const Vib_motor_gpio_step pattern_victory_steps[] = {{200u, 250u}, {200u, 0u}};
static const Vib_motor_gpio_pattern pattern_victory = {pattern_victory_steps, 2u};

static const Vib_motor_gpio_step pattern_defeat_steps[] = {{300u, 200u}, {300u, 200u}, {800u, 0u}};
static const Vib_motor_gpio_pattern pattern_defeat = {pattern_defeat_steps, 3u};

static const Vib_motor_gpio_pattern* const effect_patterns[vib_gpio_effect_count] = {&pattern_menu_tick,
    &pattern_menu_select, &pattern_back, &pattern_action_light, &pattern_jump, &pattern_shot, &pattern_pickup,
    &pattern_score, &pattern_merge, &pattern_hit_light, &pattern_hit_heavy, &pattern_life_lost,
    &pattern_victory, &pattern_defeat};

static const uint8_t gpio_effect_priorities[vib_gpio_effect_count] = {
    1u, /* menu_tick */
    2u, /* menu_select */
    2u, /* back */
    2u, /* action_light */
    2u, /* jump */
    2u, /* shot */
    3u, /* pickup */
    3u, /* score */
    3u, /* merge */
    4u, /* hit_light */
    4u, /* hit_heavy */
    5u, /* life_lost */
    5u, /* victory */
    5u, /* defeat */
};

/* ── GPIO helpers ────────────────────────────────────────────────────── */

static void output_on(Vib_motor_gpio* obj) {
    if (obj == NULL || !obj->config.enabled) { return; }

    Bsp_Gpio_Write(obj->config.gpio_idx, obj->config.active_high ? bsp_gpio_state_set : bsp_gpio_state_reset);

    obj->output_on = 1u;
}

static void output_off(Vib_motor_gpio* obj) {
    if (obj == NULL) { return; }

    Bsp_Gpio_Write(obj->config.gpio_idx, obj->config.active_high ? bsp_gpio_state_reset : bsp_gpio_state_set);

    obj->output_on = 0u;
}

/* ── Priority / debounce ─────────────────────────────────────────────── */

static uint8_t can_accept(Vib_motor_gpio* obj, uint8_t priority, uint32_t now) {
    if (!obj->config.enabled) { return 0u; }
    if (obj->active && priority < obj->priority) { return 0u; }
    const uint8_t within_cooldown =
        obj->has_played && now - obj->last_play_at < VIB_MOTOR_GPIO_MIN_RETRIGGER_MS;
    if (within_cooldown && (!obj->active || priority <= obj->priority)) { return 0u; }
    obj->has_played = 1u;
    obj->last_play_at = now;
    obj->priority = priority;
    return 1u;
}

static void finish_playback(Vib_motor_gpio* obj) {
    obj->active = 0u;
    obj->priority = 0u;
    obj->pattern = NULL;
    obj->step_index = 0u;
    obj->in_gap = 0u;
    output_off(obj);
}

/* ── Pattern state machine ───────────────────────────────────────────── */

static void start_pattern_step(Vib_motor_gpio* obj) {
    const Vib_motor_gpio_step* step = &obj->pattern->steps[obj->step_index];
    obj->duration_ms = step->on_ms;
    obj->in_gap = 0u;
    output_on(obj);
}

static void update_pattern(Vib_motor_gpio* obj, uint32_t now) {
    while (obj->active && obj->pattern != NULL) {
        const Vib_motor_gpio_step* step = &obj->pattern->steps[obj->step_index];
        const uint32_t elapsed = now - obj->started_at;
        if (!obj->in_gap) {
            /* Currently in the "on" phase */
            if (elapsed < step->on_ms) { return; }
            output_off(obj);
            obj->started_at += step->on_ms;
            if (step->off_ms > 0u) {
                obj->in_gap = 1u;
                continue;
            }
        } else {
            /* Currently in the "off" (gap) phase */
            if (elapsed < step->off_ms) { return; }
            obj->started_at += step->off_ms;
        }

        obj->step_index++;
        if (obj->step_index >= obj->pattern->length) {
            finish_playback(obj);
            return;
        }
        start_pattern_step(obj);
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void Vib_Motor_Gpio_Init(void) {
    if (vib_motor_gpio_instances == NULL) {
        vib_motor_gpio_instances = Vector_Init(sizeof(Vib_motor_gpio*), 4u);
    }
}

Vib_motor_gpio* Vib_Motor_Gpio_Create(const Vib_motor_gpio_config* config) {
    if (config == NULL || vib_motor_gpio_instances == NULL) { return NULL; }
    Vib_motor_gpio* obj = (Vib_motor_gpio*)pvPortMalloc(sizeof(*obj));
    if (obj == NULL) { return NULL; }
    memset(obj, 0, sizeof(*obj));
    obj->config = *config;
    /* Ensure GPIO output starts in the off state */
    output_off(obj);
    Vector_Push_Back(vib_motor_gpio_instances, (void*)&obj);
    return obj;
}

void Vib_Motor_Gpio_Play(Vib_motor_gpio* obj, uint16_t duration_ms) {
    if (obj == NULL || !obj->config.enabled) { return; }
    taskENTER_CRITICAL();
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (!can_accept(obj, 2u, now)) {
        taskEXIT_CRITICAL();
        return;
    }
    obj->duration_ms = duration_ms;
    obj->started_at = now;
    obj->active = 1u;
    obj->pattern = NULL;
    obj->step_index = 0u;
    obj->in_gap = 0u;
    output_on(obj);
    taskEXIT_CRITICAL();
}

void Vib_Motor_Gpio_Play_Effect(Vib_motor_gpio* obj, Vib_motor_gpio_effect effect) {
    if (obj == NULL || (unsigned)effect >= vib_gpio_effect_count) { return; }
    taskENTER_CRITICAL();
    const uint32_t now = Bsp_Get_Tick_Ms();
    const uint8_t priority = gpio_effect_priorities[effect];
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

void Vib_Motor_Gpio_Stop(Vib_motor_gpio* obj) {
    if (obj == NULL) { return; }
    taskENTER_CRITICAL();
    finish_playback(obj);
    taskEXIT_CRITICAL();
}

void Vib_Motor_Gpio_Update_All(void) {
    if (vib_motor_gpio_instances == NULL) { return; }
    const uint32_t now = Bsp_Get_Tick_Ms();
    taskENTER_CRITICAL();
    for (uint32_t i = 0; i < Vector_Get_Size(vib_motor_gpio_instances); i++) {
        Vib_motor_gpio* obj = *(Vib_motor_gpio**)Vector_Get_At(vib_motor_gpio_instances, i);
        if (obj == NULL || !obj->active) { continue; }
        if (obj->pattern != NULL) {
            update_pattern(obj, now);
        } else if (now - obj->started_at >= obj->duration_ms) {
            finish_playback(obj);
        }
    }
    taskEXIT_CRITICAL();
}

void Vib_Motor_Gpio_Set_Enabled(Vib_motor_gpio* obj, uint8_t enabled) {
    if (obj == NULL) { return; }
    taskENTER_CRITICAL();
    obj->config.enabled = enabled != 0u;
    if (!obj->config.enabled) { finish_playback(obj); }
    taskEXIT_CRITICAL();
}

uint8_t Vib_Motor_Gpio_Is_Enabled(Vib_motor_gpio* obj) { return obj == NULL ? 0u : obj->config.enabled; }
