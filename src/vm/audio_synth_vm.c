#include "audio_synth_vm.h"

#include <stddef.h>
#include <string.h>

#define VM_AUDIO_AMPLITUDE 12000

static uint8_t clamp_percent(uint8_t value) { return value > 100u ? 100u : value; }

void Vm_Audio_Synth_Init(Vm_audio_synth* synth) {
    if (synth == NULL) { return; }
    memset(synth, 0, sizeof(*synth));
    synth->master_volume = 100u;
}

void Vm_Audio_Synth_Play(Vm_audio_synth* synth, const Music* music) {
    if (synth == NULL) { return; }
    synth->music = music;
    synth->note_index = 0u;
    synth->sample_in_note = 0u;
    synth->phase = 0u;
    synth->active = music != NULL && music->notes != NULL && music->length > 0u;
}

void Vm_Audio_Synth_Stop(Vm_audio_synth* synth) {
    if (synth == NULL) { return; }
    synth->music = NULL;
    synth->active = 0u;
    synth->note_index = 0u;
    synth->sample_in_note = 0u;
}

void Vm_Audio_Synth_Set_Volume(Vm_audio_synth* synth, uint8_t volume_percent) {
    if (synth != NULL) { synth->master_volume = clamp_percent(volume_percent); }
}

uint8_t Vm_Audio_Synth_Get_Volume(const Vm_audio_synth* synth) {
    return synth == NULL ? 0u : synth->master_volume;
}

uint8_t Vm_Audio_Synth_Is_Active(const Vm_audio_synth* synth) { return synth != NULL && synth->active; }

static uint16_t current_frequency(
    const Vm_audio_synth* synth, const Buzzer_note* note, uint32_t gate_samples) {
    if ((note->flags & BUZZER_NOTE_GLISSANDO) == 0u || gate_samples == 0u ||
        synth->note_index + 1u >= synth->music->length) {
        return note->frequency_hz;
    }
    const uint16_t target = synth->music->notes[synth->note_index + 1u].frequency_hz;
    if (target == 0u) { return note->frequency_hz; }
    const int32_t delta = (int32_t)target - note->frequency_hz;
    return (uint16_t)((int32_t)note->frequency_hz +
                      delta * (int32_t)synth->sample_in_note / (int32_t)gate_samples);
}

void Vm_Audio_Synth_Render(
    Vm_audio_synth* synth, int16_t* samples, uint32_t frame_count, uint32_t sample_rate) {
    if (samples == NULL || sample_rate == 0u) { return; }
    for (uint32_t frame = 0u; frame < frame_count; frame++) {
        samples[frame] = 0;
        if (synth == NULL || !synth->active) { continue; }

        const Buzzer_note* note = &synth->music->notes[synth->note_index];
        uint32_t duration_samples = (uint32_t)note->duration_ms * sample_rate / 1000u;
        if (duration_samples == 0u) { duration_samples = 1u; }
        const uint32_t gate_samples = duration_samples * note->gate_percent / 100u;

        if (synth->sample_in_note < gate_samples && note->frequency_hz != 0u) {
            const uint16_t frequency = current_frequency(synth, note, gate_samples);
            const uint32_t phase_step = (uint32_t)(((uint64_t)frequency << 32u) / sample_rate);
            const uint8_t volume = (uint8_t)((uint16_t)note->volume_percent * synth->master_volume / 100u);
            const int16_t amplitude = (int16_t)(VM_AUDIO_AMPLITUDE * volume / 100u);
            synth->phase += phase_step;
            samples[frame] = (synth->phase & 0x80000000u) != 0u ? amplitude : (int16_t)-amplitude;
        }

        synth->sample_in_note++;
        if (synth->sample_in_note < duration_samples) { continue; }
        synth->sample_in_note = 0u;
        synth->note_index++;
        if (synth->note_index >= synth->music->length) { Vm_Audio_Synth_Stop(synth); }
    }
}
