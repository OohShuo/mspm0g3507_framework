#include <assert.h>
#include <stdint.h>

#include "audio_synth_vm.h"

static int16_t abs_sample(int16_t value) { return value < 0 ? (int16_t)-value : value; }

int main(void) {
    static const Buzzer_note notes[] = {{440u, 10u, 50u, 100u, 0u}};
    static const Music music = {notes, 1u};
    Vm_audio_synth synth;
    int16_t samples[12] = {0};

    Vm_Audio_Synth_Init(&synth);
    Vm_Audio_Synth_Play(&synth, &music);
    Vm_Audio_Synth_Render(&synth, samples, 12u, 1000u);
    for (uint8_t i = 0; i < 5u; i++) { assert(samples[i] != 0); }
    for (uint8_t i = 5u; i < 12u; i++) { assert(samples[i] == 0); }
    assert(Vm_Audio_Synth_Is_Active(&synth) == 0u);

    Vm_Audio_Synth_Set_Volume(&synth, 50u);
    Vm_Audio_Synth_Play(&synth, &music);
    Vm_Audio_Synth_Render(&synth, samples, 1u, 1000u);
    assert(abs_sample(samples[0]) == 6000);
    Vm_Audio_Synth_Stop(&synth);
    assert(Vm_Audio_Synth_Is_Active(&synth) == 0u);
    return 0;
}
