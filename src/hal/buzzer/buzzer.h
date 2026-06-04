#pragma once
#include <stdint.h>

#include "bsp_pwm.h"

typedef struct {
    uint8_t pwm_idx;

    const uint16_t* music_score;
    uint16_t score_length;

    uint16_t speed_npm;  // notes per minute
} Buzzer_config;

typedef struct {
    Buzzer_config config;

    uint8_t is_playing;
    uint16_t note_index;
    uint32_t last_note_time;
    uint16_t note_now;
    uint16_t note_last;
} Buzzer;

extern const uint16_t music1[];
extern const uint8_t music1_len;

void Buzzer_Init(void);

Buzzer* Buzzer_Create(const Buzzer_config* config);
void Buzzer_Play(Buzzer* obj);
void Buzzer_Stop(Buzzer* obj);
void Buzzer_Update_All(void);
