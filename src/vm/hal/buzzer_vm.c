#include "buzzer.h"

#include <SDL2/SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const Music* sequence;
    uint16_t note_index;
    uint32_t note_started_at;
    uint8_t active;
    uint8_t note_started;
    uint8_t note_silenced;
} Vm_buzzer_track;

struct Buzzer_t {
    Vm_buzzer_track sfx;
    uint8_t sfx_priority;
    uint8_t master_volume;
};

static Buzzer g_buzzer;
static SDL_AudioDeviceID g_audio_device = 0;
static SDL_mutex* g_audio_mutex = NULL;
static double g_phase = 0.0;
static uint16_t g_output_frequency = 0;
static uint8_t g_output_volume = 0;

static uint8_t sfx_priority(Buzzer_sfx_idx sfx) {
    if (sfx == buzzer_sfx_life_lost || sfx == buzzer_sfx_victory || sfx == buzzer_sfx_defeat ||
        sfx == buzzer_sfx_boss_alert) {
        return 5u;
    }
    if (sfx == buzzer_sfx_tetris_tetris || sfx == buzzer_sfx_racing_crash) { return 4u; }
    if (sfx == buzzer_sfx_flappy_score || sfx == buzzer_sfx_pong_score ||
        sfx == buzzer_sfx_maze_goal) {
        return 3u;
    }
    return 1u;
}

static void audio_cb(void* userdata, uint8_t* stream, int len) {
    (void)userdata;
    int16_t* out = (int16_t*)stream;
    const int samples = len / (int)sizeof(int16_t);

    uint16_t freq;
    uint8_t vol;
    if (g_audio_mutex != NULL) { SDL_LockMutex(g_audio_mutex); }
    freq = g_output_frequency;
    vol = g_output_volume;
    if (g_audio_mutex != NULL) { SDL_UnlockMutex(g_audio_mutex); }

    if (freq == 0u || vol == 0u) {
        memset(stream, 0, (size_t)len);
        return;
    }

    const double sample_rate = 48000.0;
    const double amp = 2800.0 * (double)vol / 100.0;
    for (int i = 0; i < samples; i++) {
        out[i] = (int16_t)(g_phase < 0.5 ? amp : -amp);
        g_phase += (double)freq / sample_rate;
        if (g_phase >= 1.0) { g_phase -= 1.0; }
    }
}

static void set_output(uint16_t frequency_hz, uint8_t volume_percent) {
    if (g_audio_mutex != NULL) { SDL_LockMutex(g_audio_mutex); }
    g_output_frequency = frequency_hz;
    g_output_volume = volume_percent;
    if (frequency_hz == 0u || volume_percent == 0u) { g_phase = 0.0; }
    if (g_audio_mutex != NULL) { SDL_UnlockMutex(g_audio_mutex); }
}

static void silence(void) { set_output(0, 0); }

static void reset_track(Vm_buzzer_track* track, const Music* sequence) {
    track->sequence = sequence;
    track->note_index = 0;
    track->note_started_at = 0;
    track->active = sequence != NULL && sequence->notes != NULL && sequence->length > 0u;
    track->note_started = 0;
    track->note_silenced = 0;
}

static uint16_t next_frequency(const Vm_buzzer_track* track) {
    const uint16_t next_index = (uint16_t)(track->note_index + 1u);
    if (next_index >= track->sequence->length) { return 0; }
    return track->sequence->notes[next_index].frequency_hz;
}

static uint8_t advance_track(Vm_buzzer_track* track) {
    track->note_index++;
    track->note_started = 0;
    track->note_silenced = 0;
    if (track->note_index < track->sequence->length) { return 1; }
    track->active = 0;
    return 0;
}

static uint8_t update_track(Buzzer* obj, Vm_buzzer_track* track, uint32_t now) {
    if (!track->active) { return 0; }

    const Buzzer_note* note = &track->sequence->notes[track->note_index];
    if (!track->note_started) {
        track->note_started = 1;
        track->note_started_at = now;
        track->note_silenced = 0;
        set_output(note->frequency_hz, (uint8_t)((uint16_t)note->volume_percent * obj->master_volume / 100u));
    }

    const uint32_t elapsed = now - track->note_started_at;
    const uint32_t duration = note->duration_ms == 0u ? 1u : note->duration_ms;
    const uint32_t gate_time = duration * note->gate_percent / 100u;

    if ((note->flags & BUZZER_NOTE_GLISSANDO) != 0u && note->frequency_hz != 0u && elapsed < gate_time &&
        gate_time > 0u) {
        const uint16_t target = next_frequency(track);
        if (target != 0u) {
            const int32_t delta = (int32_t)target - note->frequency_hz;
            const uint16_t frequency =
                (uint16_t)((int32_t)note->frequency_hz + delta * (int32_t)elapsed / (int32_t)gate_time);
            set_output(frequency, (uint8_t)((uint16_t)note->volume_percent * obj->master_volume / 100u));
        }
    }

    if (!track->note_silenced && elapsed >= gate_time) {
        silence();
        track->note_silenced = 1;
    }
    if (elapsed < duration) { return 1; }

    if (!advance_track(track)) {
        silence();
        return 0;
    }
    return update_track(obj, track, now);
}

void Buzzer_Init(void) {
    memset(&g_buzzer, 0, sizeof(g_buzzer));
    g_buzzer.master_volume = 50u;

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0u && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[VM] SDL audio init failed: %s\n", SDL_GetError());
        return;
    }

    g_audio_mutex = SDL_CreateMutex();
    SDL_AudioSpec want;
    memset(&want, 0, sizeof(want));
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_cb;

    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (g_audio_device == 0) {
        fprintf(stderr, "[VM] SDL audio open failed: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(g_audio_device, 0);
}

Buzzer* Buzzer_Create(const Buzzer_config* config) {
    (void)config;
    return &g_buzzer;
}

void Buzzer_Play_Sfx(Buzzer* obj, Buzzer_sfx_idx sfx) {
    if (obj == NULL || sfx >= buzzer_sfx_count) { return; }
    const uint8_t priority = sfx_priority(sfx);
    if (obj->sfx.active && priority < obj->sfx_priority) { return; }
    reset_track(&obj->sfx, &buzzer_sfx_library[sfx]);
    obj->sfx_priority = priority;
}

void Buzzer_Play(Buzzer* obj, const Music* music) {
    if (obj == NULL) { return; }
    reset_track(&obj->sfx, music);
    obj->sfx_priority = 0u;
}

void Buzzer_Stop(Buzzer* obj) {
    if (obj == NULL) { return; }
    memset(&obj->sfx, 0, sizeof(obj->sfx));
    obj->sfx_priority = 0u;
    silence();
}

void Buzzer_Update_All(void) {
    if (g_buzzer.sfx.active) {
        if (update_track(&g_buzzer, &g_buzzer.sfx, SDL_GetTicks())) { return; }
        g_buzzer.sfx_priority = 0u;
    }
    silence();
}

void Buzzer_Set_Volume(Buzzer* obj, uint8_t volume_percent) {
    if (obj == NULL) { return; }
    obj->master_volume = volume_percent > 100u ? 100u : volume_percent;
}

uint8_t Buzzer_Get_Volume(Buzzer* obj) {
    if (obj == NULL) { return 0; }
    return obj->master_volume;
}
