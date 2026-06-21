#include "buzzer.h"

#include <SDL2/SDL.h>
#include <stdio.h>

#include "audio_synth_vm.h"

#define VM_AUDIO_SAMPLE_RATE 48000

struct Buzzer_t {
    Vm_audio_synth synth;
    uint8_t sfx_priority;
};

static Buzzer g_buzzer;
static SDL_AudioDeviceID g_audio_device;

static const uint8_t g_sfx_priorities[buzzer_sfx_count] = {
    [buzzer_sfx_menu_move] = 1u,
    [buzzer_sfx_menu_select] = 2u,
    [buzzer_sfx_power] = 3u,
    [buzzer_sfx_ghost] = 3u,
    [buzzer_sfx_snake_grow] = 2u,
    [buzzer_sfx_racing_crash] = 4u,
    [buzzer_sfx_tank_hit] = 2u,
    [buzzer_sfx_air_hit] = 2u,
    [buzzer_sfx_air_pickup] = 2u,
    [buzzer_sfx_boss_alert] = 5u,
    [buzzer_sfx_tetris_lock] = 2u,
    [buzzer_sfx_tetris_line_clear] = 3u,
    [buzzer_sfx_tetris_tetris] = 4u,
    [buzzer_sfx_breakout_brick] = 2u,
    [buzzer_sfx_pong_score] = 3u,
    [buzzer_sfx_gomoku_place] = 2u,
    [buzzer_sfx_merge] = 2u,
    [buzzer_sfx_flappy_score] = 2u,
    [buzzer_sfx_maze_goal] = 3u,
    [buzzer_sfx_needle_launch] = 2u,
    [buzzer_sfx_needle_stick] = 2u,
    [buzzer_sfx_explosion] = 3u,
    [buzzer_sfx_life_lost] = 5u,
    [buzzer_sfx_victory] = 5u,
    [buzzer_sfx_defeat] = 5u,
};

static void lock_audio(void) {
    if (g_audio_device != 0u) { SDL_LockAudioDevice(g_audio_device); }
}

static void unlock_audio(void) {
    if (g_audio_device != 0u) { SDL_UnlockAudioDevice(g_audio_device); }
}

static void audio_callback(void* userdata, Uint8* stream, int length) {
    Buzzer* buzzer = (Buzzer*)userdata;
    const uint32_t frames = (uint32_t)length / sizeof(int16_t);
    Vm_Audio_Synth_Render(&buzzer->synth, (int16_t*)stream, frames, VM_AUDIO_SAMPLE_RATE);
    if (!Vm_Audio_Synth_Is_Active(&buzzer->synth)) { buzzer->sfx_priority = 0u; }
}

void Buzzer_Init(void) {
    Vm_Audio_Synth_Init(&g_buzzer.synth);
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[VM] SDL audio disabled: %s\n", SDL_GetError());
        return;
    }

    const SDL_AudioSpec desired = {
        .freq = VM_AUDIO_SAMPLE_RATE,
        .format = AUDIO_S16SYS,
        .channels = 1u,
        .samples = 512u,
        .callback = audio_callback,
        .userdata = &g_buzzer,
    };
    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
    if (g_audio_device == 0u) {
        fprintf(stderr, "[VM] SDL audio device unavailable: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(g_audio_device, 0);
}

Buzzer* Buzzer_Create(const Buzzer_config* config) {
    if (config == NULL) { return NULL; }
    lock_audio();
    Vm_Audio_Synth_Stop(&g_buzzer.synth);
    Vm_Audio_Synth_Set_Volume(&g_buzzer.synth, 50u);
    g_buzzer.sfx_priority = 0u;
    unlock_audio();
    return &g_buzzer;
}

void Buzzer_Play_Sfx(Buzzer* buzzer, Buzzer_sfx_idx sfx) {
    if (buzzer == NULL || (unsigned)sfx >= buzzer_sfx_count) { return; }
    lock_audio();
    const uint8_t priority = g_sfx_priorities[sfx] == 0u ? 1u : g_sfx_priorities[sfx];
    if (!Vm_Audio_Synth_Is_Active(&buzzer->synth) || priority >= buzzer->sfx_priority) {
        Vm_Audio_Synth_Play(&buzzer->synth, &buzzer_sfx_library[sfx]);
        buzzer->sfx_priority = priority;
    }
    unlock_audio();
}

void Buzzer_Play(Buzzer* buzzer, const Music* music) {
    if (buzzer == NULL) { return; }
    lock_audio();
    Vm_Audio_Synth_Play(&buzzer->synth, music);
    buzzer->sfx_priority = 0u;
    unlock_audio();
}

void Buzzer_Stop(Buzzer* buzzer) {
    if (buzzer == NULL) { return; }
    lock_audio();
    Vm_Audio_Synth_Stop(&buzzer->synth);
    buzzer->sfx_priority = 0u;
    unlock_audio();
}

void Buzzer_Update_All(void) {}

void Buzzer_Set_Volume(Buzzer* buzzer, uint8_t volume_percent) {
    if (buzzer == NULL) { return; }
    lock_audio();
    Vm_Audio_Synth_Set_Volume(&buzzer->synth, volume_percent);
    unlock_audio();
}

uint8_t Buzzer_Get_Volume(Buzzer* buzzer) {
    if (buzzer == NULL) { return 0u; }
    lock_audio();
    const uint8_t volume = Vm_Audio_Synth_Get_Volume(&buzzer->synth);
    unlock_audio();
    return volume;
}
