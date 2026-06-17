#include <stdlib.h>
#include <string.h>

#include "buzzer.h"

// Weak stub for SFX library (defined in buzzer_def.c on real HW)
const Music buzzer_sfx_library[buzzer_sfx_count] = {{NULL, 0}};

void Buzzer_Init(void) {}

Buzzer* Buzzer_Create(const Buzzer_config* c) {
    (void)c;
    static int dummy;
    return (Buzzer*)&dummy;  // Buzzer is opaque; caller only uses the pointer
}

void Buzzer_Play_Sfx(Buzzer* o, Buzzer_sfx_idx s) {
    (void)o;
    (void)s;
}
void Buzzer_Play(Buzzer* o, const Music* m) {
    (void)o;
    (void)m;
}
void Buzzer_Stop(Buzzer* o) { (void)o; }
void Buzzer_Update_All(void) {}

void Buzzer_Set_Volume(Buzzer* o, uint8_t v) {
    (void)o;
    (void)v;
}

uint8_t Buzzer_Get_Volume(Buzzer* o) {
    (void)o;
    return 50;
}
