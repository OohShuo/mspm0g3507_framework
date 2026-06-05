#include <stdint.h>

#include "buzzer.h"

// Tone frequency definitions
#define NOTE_XL_DO 7645
#define NOTE_XL_RE 6811
#define NOTE_XL_MI 6067
#define NOTE_XL_FA 5727
#define NOTE_XL_SO 5102
#define NOTE_XL_LA 4545
#define NOTE_XL_XI 4049
#define NOTE_L_DO  3822
#define NOTE_L_RE  3405
#define NOTE_L_MI  3033
#define NOTE_L_FA  2863
#define NOTE_L_SO  2551
#define NOTE_L_GSH 2408
#define NOTE_L_LA  2272
#define NOTE_L_XI  2024
#define NOTE_L_FSH 2711
#define NOTE_L_CSH 3640
#define NOTE_M_DO  1911
#define NOTE_M_RE  1702
#define NOTE_M_DSH 1607
#define NOTE_M_MI  1526
#define NOTE_M_FA  1431
#define NOTE_M_SO  1275
#define NOTE_M_LA  1136
#define NOTE_M_XI  1012
#define NOTE_H_DO  955
#define NOTE_H_RE  851
#define NOTE_H_MI  758
#define NOTE_H_FA  715
#define NOTE_H_CSH 721
#define NOTE_H_SO  637
#define NOTE_H_LA  568
#define NOTE_H_XI  506
#define NOTE_X_DO  478
#define NOTE_REST  0
#define GLISS      0x8000

// 8-bit 游戏机风格主题曲（低八度）
const uint16_t music_main_theme[] = {
    // A段 - 跳跃主旋律
    NOTE_L_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_M_MI, NOTE_M_DO,
    NOTE_L_SO, NOTE_M_DO, NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_L_XI, NOTE_M_DO,

    // B段 - 冲刺感
    NOTE_M_DO, NOTE_M_RE, NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_L_XI, NOTE_L_LA, NOTE_L_XI, NOTE_M_DO,
    NOTE_REST, NOTE_M_DO, NOTE_REST, NOTE_L_SO, NOTE_L_LA, NOTE_L_XI, NOTE_M_DO,

    // A段重复
    NOTE_L_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_M_MI, NOTE_M_DO,
    NOTE_L_SO, NOTE_M_DO, NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_L_XI, NOTE_M_DO,

    // C段 - 高潮上行
    NOTE_L_DO, NOTE_L_RE, NOTE_L_MI, NOTE_L_FA, NOTE_L_SO, NOTE_L_LA, NOTE_L_XI, NOTE_M_DO, NOTE_M_DO,
    NOTE_M_RE, NOTE_M_MI, NOTE_M_FA, NOTE_M_SO, NOTE_REST, NOTE_REST};
const uint8_t music_main_theme_len = sizeof(music_main_theme) / sizeof(music_main_theme[0]);

// 胜利音效 - 上行欢快旋律，循环播放
const uint16_t music_victory[] = {NOTE_L_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO,
    NOTE_M_MI, NOTE_L_SO, NOTE_M_DO, NOTE_M_MI, NOTE_M_SO, NOTE_M_DO, NOTE_M_MI, NOTE_M_SO, NOTE_REST,
    NOTE_M_MI, NOTE_M_SO, NOTE_REST, NOTE_M_DO, NOTE_REST};
const uint8_t music_victory_len = sizeof(music_victory) / sizeof(music_victory[0]);

// 死亡音效 - 下行阴沉旋律，循环播放
const uint16_t music_death[] = {NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_L_XI, NOTE_L_LA, NOTE_L_SO, NOTE_L_FA,
    NOTE_L_MI, NOTE_L_MI, NOTE_L_FA, NOTE_L_MI, NOTE_L_RE, NOTE_L_MI, NOTE_REST, NOTE_REST};
const uint8_t music_death_len = sizeof(music_death) / sizeof(music_death[0]);

// 8-bit 游戏主题曲 2 - 冒险风格（低八度）
const uint16_t music_main_theme_2[] = {
    // A段 - 明亮主旋律
    NOTE_M_DO, NOTE_L_XI, NOTE_M_DO, NOTE_L_SO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_REST, NOTE_L_XI,
    NOTE_M_DO, NOTE_L_XI, NOTE_L_LA, NOTE_L_SO, NOTE_REST, NOTE_REST,

    // B段 - 跳跃节奏
    NOTE_L_MI, NOTE_L_FA, NOTE_L_SO, NOTE_L_LA, NOTE_L_SO, NOTE_L_FA, NOTE_L_MI, NOTE_REST, NOTE_L_DO,
    NOTE_L_RE, NOTE_L_MI, NOTE_L_FA, NOTE_L_MI, NOTE_L_RE, NOTE_L_DO, NOTE_REST,

    // A段重复
    NOTE_M_DO, NOTE_L_XI, NOTE_M_DO, NOTE_L_SO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_REST, NOTE_L_XI,
    NOTE_M_DO, NOTE_L_XI, NOTE_L_LA, NOTE_L_SO, NOTE_REST, NOTE_REST,

    // C段 - 高潮
    NOTE_L_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_M_RE, NOTE_M_DO, NOTE_L_XI, NOTE_M_DO, NOTE_M_MI,
    NOTE_M_RE, NOTE_M_DO, NOTE_L_XI, NOTE_M_DO, NOTE_REST, NOTE_REST};
const uint8_t music_main_theme_2_len = sizeof(music_main_theme_2) / sizeof(music_main_theme_2[0]);

// 超级马里奥 (Super Mario Bros Theme)（低八度）
const uint16_t music_mario[] = {NOTE_M_RE, NOTE_M_RE, NOTE_REST, NOTE_M_DO, NOTE_M_RE, NOTE_REST, NOTE_M_SO,
    NOTE_REST, NOTE_L_SO, NOTE_REST, NOTE_REST, NOTE_REST,

    NOTE_M_DO, NOTE_REST, NOTE_REST, NOTE_L_SO, NOTE_REST, NOTE_REST, NOTE_L_MI, NOTE_REST, NOTE_REST,
    NOTE_REST, NOTE_L_LA, NOTE_REST, NOTE_L_XI, NOTE_REST, NOTE_L_LA, NOTE_REST,

    NOTE_L_SO, NOTE_M_MI, NOTE_M_SO, NOTE_M_LA, NOTE_REST, NOTE_M_FA, NOTE_M_SO, NOTE_REST, NOTE_M_MI,
    NOTE_M_DO, NOTE_M_RE, NOTE_L_XI, NOTE_REST, NOTE_REST, NOTE_REST};
const uint8_t music_mario_len = sizeof(music_mario) / sizeof(music_mario[0]);

// 龙猫 (Totoro) - 久石让
const uint16_t music_totoro[] = {
    // 开头
    NOTE_L_MI, NOTE_L_MI, NOTE_L_MI, NOTE_L_RE, NOTE_L_DO, NOTE_L_RE, NOTE_L_MI, NOTE_L_SO, NOTE_L_MI,
    NOTE_REST, NOTE_L_MI, NOTE_L_MI,

    NOTE_L_MI, NOTE_L_RE, NOTE_L_DO, NOTE_L_RE, NOTE_L_MI, NOTE_L_SO, NOTE_L_MI, NOTE_REST,

    NOTE_L_MI, NOTE_L_MI, NOTE_L_SO, NOTE_L_MI, NOTE_L_LA, NOTE_L_SO, NOTE_L_MI, NOTE_L_SO, NOTE_L_MI,
    NOTE_L_RE, NOTE_L_DO, NOTE_L_RE, NOTE_L_MI, NOTE_REST};
const uint8_t music_totoro_len = sizeof(music_totoro) / sizeof(music_totoro[0]);

const Music music_library[] = {
    {music_main_theme, music_main_theme_len, 60 * 3},
    {music_victory, music_victory_len, 60 * 6},
    {music_death, music_death_len, 60 * 4},
    {music_main_theme_2, music_main_theme_2_len, 60 * 5},
    {music_mario, music_mario_len, 60 * 6},
    {music_totoro, music_totoro_len, 60 * 3 + 20},
};
