#pragma once
#include <stdint.h>

#include "bsp_pwm.h"

// 滑音标记：最高位置 1 表示该音符需要滑音到下一个音
#define NOTE_FLAG_GLISSANDO 0x8000

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
    uint16_t glissando_src_freq;   // 滑音起始频率
    uint16_t glissando_dst_freq;   // 滑音目标频率
    uint32_t glissando_start_time; // 滑音开始时间
    uint32_t glissando_duration;   // 滑音持续时间(ms)
} Buzzer;

// 预定义的音乐
extern const uint16_t music1[];
extern const uint8_t music1_len;

extern const uint16_t music2[];
extern const uint8_t music2_len;

extern const uint16_t music3[];
extern const uint8_t music3_len;

extern const uint16_t music4[];
extern const uint8_t music4_len;

extern const uint16_t music5[];
extern const uint8_t music5_len;

extern const uint16_t music6[];
extern const uint8_t music6_len;

extern const uint16_t music7[];
extern const uint8_t music7_len;

extern const uint16_t music8[];
extern const uint8_t music8_len;

extern const uint16_t music9[];
extern const uint8_t music9_len;

extern const uint16_t music10[];
extern const uint8_t music10_len;

extern const uint16_t music11[];
extern const uint8_t music11_len;

extern const uint16_t music12[];
extern const uint8_t music12_len;

extern const uint16_t music13[];
extern const uint8_t music13_len;

extern const uint16_t music14[];
extern const uint8_t music14_len;

extern const uint16_t music15[];
extern const uint8_t music15_len;

extern const uint16_t music16[];
extern const uint8_t music16_len;

extern const uint16_t music17[];
extern const uint8_t music17_len;

void Buzzer_Init(void);

Buzzer* Buzzer_Create(const Buzzer_config* config);
void Buzzer_Play(Buzzer* obj, const uint16_t* score, uint16_t length, uint16_t speed_npm, uint8_t loop);
void Buzzer_Stop(Buzzer* obj);
void Buzzer_Update_All(void);