#include "buzzer.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bsp_pwm.h"
#include "bsp_time.h"
#include "freertos_alloc.h"
#include "task.h"
#include "vector.h"

typedef struct {
    const Music* sequence;
    uint16_t note_index;
    uint32_t note_started_at;
    uint8_t active;
    uint8_t note_started;
    uint8_t note_silenced;
} Buzzer_track;

struct Buzzer_t {
    Buzzer_config config;
    Buzzer_track sfx;
    uint16_t output_frequency;
    uint8_t output_active;
    uint8_t sfx_priority;
};

static Vector* buzzer_instances = NULL;

static const uint8_t sfx_priorities[buzzer_sfx_count] = {
    [buzzer_sfx_menu_move]        = 1,
    [buzzer_sfx_menu_select]      = 2,
    [buzzer_sfx_pellet]           = 1,
    [buzzer_sfx_power]            = 3,
    [buzzer_sfx_ghost]            = 3,
    [buzzer_sfx_pacman_waka]      = 1,
    [buzzer_sfx_snake_eat]        = 1,
    [buzzer_sfx_snake_turn]       = 1,
    [buzzer_sfx_snake_grow]       = 2,
    [buzzer_sfx_lane_change]      = 1,
    [buzzer_sfx_overtake]         = 1,
    [buzzer_sfx_racing_crash]     = 4,
    [buzzer_sfx_tank_fire]        = 1,
    [buzzer_sfx_tank_hit]         = 2,
    [buzzer_sfx_air_fire]         = 1,
    [buzzer_sfx_air_hit]          = 2,
    [buzzer_sfx_air_pickup]       = 2,
    [buzzer_sfx_boss_alert]       = 5,
    [buzzer_sfx_tetris_move]      = 1,
    [buzzer_sfx_tetris_rotate]    = 1,
    [buzzer_sfx_tetris_lock]      = 2,
    [buzzer_sfx_tetris_line_clear]= 3,
    [buzzer_sfx_tetris_tetris]    = 4,
    [buzzer_sfx_breakout_bounce]  = 1,
    [buzzer_sfx_breakout_brick]   = 2,
    [buzzer_sfx_pong_paddle]      = 1,
    [buzzer_sfx_pong_wall]        = 1,
    [buzzer_sfx_pong_score]       = 3,
    [buzzer_sfx_gomoku_place]     = 2,
    [buzzer_sfx_slide]            = 1,
    [buzzer_sfx_merge]            = 2,
    [buzzer_sfx_dino_jump]        = 1,
    [buzzer_sfx_dino_duck]        = 1,
    [buzzer_sfx_flappy_flap]      = 1,
    [buzzer_sfx_flappy_score]     = 2,
    [buzzer_sfx_maze_move]        = 1,
    [buzzer_sfx_maze_goal]        = 3,
    [buzzer_sfx_needle_launch]    = 2,
    [buzzer_sfx_needle_stick]     = 2,
    [buzzer_sfx_explosion]        = 3,
    [buzzer_sfx_life_lost]        = 5,
    [buzzer_sfx_victory]          = 5,
    [buzzer_sfx_defeat]           = 5,
};

static void silence(Buzzer* obj) {
    if (obj == NULL || !obj->output_active) { return; }
    Bsp_Pwm_Stop(obj->config.pwm_idx);
    obj->output_active = 0;
    obj->output_frequency = 0;
}

static void output_frequency(Buzzer* obj, uint16_t frequency_hz, uint8_t volume_percent) {
    if (frequency_hz == 0 || volume_percent == 0) {
        silence(obj);
        return;
    }

    if (volume_percent > 100u) { volume_percent = 100u; }
    Bsp_Pwm_Stop(obj->config.pwm_idx);
    Bsp_Pwm_Set_Freq(obj->config.pwm_idx, frequency_hz);
    Bsp_Pwm_Set_Duty(obj->config.pwm_idx, 0.15f + 0.0035f * volume_percent);
    Bsp_Pwm_Start(obj->config.pwm_idx);
    obj->output_active = 1;
    obj->output_frequency = frequency_hz;
}

static void reset_track(Buzzer_track* track, const Music* sequence) {
    track->sequence = sequence;
    track->note_index = 0;
    track->note_started_at = 0;
    track->active = sequence != NULL && sequence->notes != NULL && sequence->length > 0;
    track->note_started = 0;
    track->note_silenced = 0;
}

static uint8_t advance_track(Buzzer_track* track) {
    track->note_index++;
    track->note_started = 0;
    track->note_silenced = 0;
    if (track->note_index < track->sequence->length) { return 1; }
    track->active = 0;
    return 0;
}

static uint16_t next_frequency(const Buzzer_track* track) {
    uint16_t next_index = (uint16_t)(track->note_index + 1u);
    if (next_index >= track->sequence->length) { return 0; }
    return track->sequence->notes[next_index].frequency_hz;
}

static uint8_t update_track(Buzzer* obj, Buzzer_track* track, uint32_t now) {
    if (!track->active) { return 0; }

    const Buzzer_note* note = &track->sequence->notes[track->note_index];
    if (!track->note_started) {
        track->note_started = 1;
        track->note_started_at = now;
        track->note_silenced = 0;
        output_frequency(obj, note->frequency_hz, note->volume_percent);
    }

    const uint32_t elapsed = now - track->note_started_at;
    const uint32_t duration = note->duration_ms == 0 ? 1u : note->duration_ms;
    const uint32_t gate_time = duration * note->gate_percent / 100u;

    if ((note->flags & BUZZER_NOTE_GLISSANDO) != 0 && note->frequency_hz != 0 && elapsed < gate_time &&
        gate_time > 0) {
        const uint16_t target = next_frequency(track);
        if (target != 0) {
            const int32_t delta = (int32_t)target - note->frequency_hz;
            const uint16_t frequency =
                (uint16_t)((int32_t)note->frequency_hz + delta * (int32_t)elapsed / (int32_t)gate_time);
            if (frequency != obj->output_frequency) {
                Bsp_Pwm_Set_Freq(obj->config.pwm_idx, frequency);
                obj->output_frequency = frequency;
            }
        }
    }

    if (!track->note_silenced && elapsed >= gate_time) {
        silence(obj);
        track->note_silenced = 1;
    }
    if (elapsed < duration) { return 1; }

    if (!advance_track(track)) {
        silence(obj);
        return 0;
    }
    return update_track(obj, track, now);
}

void Buzzer_Init(void) {
    if (buzzer_instances == NULL) { buzzer_instances = Vector_Init(sizeof(Buzzer*), 4); }
}

Buzzer* Buzzer_Create(const Buzzer_config* config) {
    if (config == NULL || buzzer_instances == NULL) { return NULL; }

    Buzzer* obj = (Buzzer*)pvPortMalloc(sizeof(Buzzer));
    if (obj == NULL) { return NULL; }

    memset(obj, 0, sizeof(*obj));
    obj->config = *config;
    Vector_Push_Back(buzzer_instances, (void*)&obj);
    return obj;
}

void Buzzer_Play_Sfx(Buzzer* obj, Buzzer_sfx_idx sfx) {
    if (obj == NULL || sfx >= buzzer_sfx_count) { return; }
    taskENTER_CRITICAL();
    const uint8_t priority = sfx_priorities[sfx];
    if (obj->sfx.active && priority < obj->sfx_priority) {
        taskEXIT_CRITICAL();
        return;
    }
    reset_track(&obj->sfx, &buzzer_sfx_library[sfx]);
    obj->sfx_priority = priority;
    taskEXIT_CRITICAL();
}

void Buzzer_Play(Buzzer* obj, const Music* music) {
    if (obj == NULL) { return; }
    taskENTER_CRITICAL();
    reset_track(&obj->sfx, music);
    obj->sfx_priority = 0;
    taskEXIT_CRITICAL();
}

void Buzzer_Stop(Buzzer* obj) {
    if (obj == NULL) { return; }
    taskENTER_CRITICAL();
    memset(&obj->sfx, 0, sizeof(obj->sfx));
    obj->sfx_priority = 0;
    silence(obj);
    taskEXIT_CRITICAL();
}

void Buzzer_Update_All(void) {
    if (buzzer_instances == NULL) { return; }
    const uint32_t now = Bsp_Get_Tick_Ms();

    taskENTER_CRITICAL();
    for (uint32_t i = 0; i < Vector_Get_Size(buzzer_instances); i++) {
        Buzzer* obj = *(Buzzer**)Vector_Get_At(buzzer_instances, i);
        if (obj == NULL) { continue; }

        if (obj->sfx.active) {
            if (update_track(obj, &obj->sfx, now)) { continue; }
            obj->sfx_priority = 0;
        }
        silence(obj);
    }
    taskEXIT_CRITICAL();
}
