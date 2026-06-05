#pragma once
#include <stdint.h>

#include "bsp_pwm.h"

typedef struct Music_t {
    const uint16_t* score;
    uint8_t length;
    uint16_t speed_npm;
} Music;

typedef enum {
    music_idx_main_theme = 0,
    music_idx_victory,
    music_idx_death,
    music_idx_main_theme_2,
    music_idx_mario,
    music_idx_totoro,
    music_idx_count
} Music_idx;

typedef struct {
    uint8_t pwm_idx;
} Buzzer_config;

typedef struct {
    Buzzer_config config;

    uint8_t is_playing;
    uint8_t is_looping;

    const uint16_t* music_score;
    uint16_t score_length;
    uint16_t speed_npm;

    uint16_t note_index;
    uint32_t last_note_time;
    uint16_t note_now;
    uint16_t note_last;

    // 滑音状态
    uint16_t glissando_src_freq;    // 滑音起始频率
    uint16_t glissando_dst_freq;    // 滑音目标频率
    uint32_t glissando_start_time;  // 滑音开始时间
    uint32_t glissando_duration;    // 滑音持续时间(ms)
} Buzzer;

extern const Music music_library[];

void Buzzer_Init(void);

Buzzer* Buzzer_Create(const Buzzer_config* config);
void Buzzer_Play(Buzzer* obj, const Music* music, uint8_t is_loop);
void Buzzer_Stop(Buzzer* obj);
void Buzzer_Update_All(void);