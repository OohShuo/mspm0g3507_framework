#pragma once

#include <stdint.h>

#include "buzzer.h"

typedef struct {
    const Music* music;
    uint32_t sample_in_note;
    uint32_t phase;
    uint16_t note_index;
    uint8_t master_volume;
    uint8_t active;
} Vm_audio_synth;

void Vm_Audio_Synth_Init(Vm_audio_synth* synth);
void Vm_Audio_Synth_Play(Vm_audio_synth* synth, const Music* music);
void Vm_Audio_Synth_Stop(Vm_audio_synth* synth);
void Vm_Audio_Synth_Set_Volume(Vm_audio_synth* synth, uint8_t volume_percent);
uint8_t Vm_Audio_Synth_Get_Volume(const Vm_audio_synth* synth);
uint8_t Vm_Audio_Synth_Is_Active(const Vm_audio_synth* synth);
void Vm_Audio_Synth_Render(
    Vm_audio_synth* synth, int16_t* samples, uint32_t frame_count, uint32_t sample_rate);
