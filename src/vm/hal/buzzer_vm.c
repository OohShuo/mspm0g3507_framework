#include "buzzer.h"
#include <stdlib.h>
#include <string.h>

// Weak stubs for music/sfx libraries (defined in buzzer_def.c on real HW)
const Music music_library[music_idx_count] = {{NULL,0}};
const Music buzzer_sfx_library[buzzer_sfx_count] = {{NULL,0}};

void Buzzer_Init(void) {}

Buzzer* Buzzer_Create(const Buzzer_config* c) {
    (void)c;
    static int dummy;
    return (Buzzer*)&dummy;  // Buzzer is opaque; caller only uses the pointer
}

void Buzzer_Play_Music(Buzzer* o, Music_idx m, uint8_t loop)    { (void)o;(void)m;(void)loop; }
void Buzzer_Play_Sfx(Buzzer* o, Buzzer_sfx_idx s)               { (void)o;(void)s; }
void Buzzer_Play(Buzzer* o, const Music* m, uint8_t loop)       { (void)o;(void)m;(void)loop; }
void Buzzer_Stop(Buzzer* o)                                      { (void)o; }
void Buzzer_Update_All(void)                                      {}
