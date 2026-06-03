#include <stdint.h>

// Tone frequency definitions
#define NOTE_L_DO 3822  // 单位 微秒，频率 = 1,000,000 / 周期
#define NOTE_L_RE 3405
#define NOTE_L_MI 3033
#define NOTE_L_FA 2863
#define NOTE_L_SO 2551
#define NOTE_L_LA 2272
#define NOTE_L_XI 2024
#define NOTE_M_DO 1911
#define NOTE_M_RE 1702
#define NOTE_M_MI 1526
#define NOTE_M_FA 1431
#define NOTE_M_SO 1275
#define NOTE_M_LA 1136
#define NOTE_M_XI 1012
#define NOTE_H_DO 955
#define NOTE_H_RE 851
#define NOTE_H_MI 758
#define NOTE_H_FA 715
#define NOTE_H_SO 637
#define NOTE_H_LA 568
#define NOTE_H_XI 506
#define NOTE_REST 0

const uint16_t music1[] = {NOTE_H_DO, NOTE_H_DO, NOTE_H_DO, 0, NOTE_H_DO, 0, NOTE_M_XI, NOTE_H_DO, NOTE_H_DO,
    0, NOTE_H_RE, 0, NOTE_H_MI, 0, NOTE_H_FA, 0, NOTE_H_MI, NOTE_H_MI, 0, NOTE_H_MI, NOTE_H_MI, 0, NOTE_M_XI,
    0, NOTE_M_XI, NOTE_M_XI, NOTE_M_XI, 0, 0, NOTE_M_LA, NOTE_M_LA, NOTE_M_LA, 0, NOTE_M_LA, 0, NOTE_M_SO,
    NOTE_M_LA, NOTE_M_LA, 0, NOTE_H_FA, 0, NOTE_H_MI, 0, NOTE_H_RE, 0, NOTE_H_RE, NOTE_H_RE, 0, NOTE_H_DO,
    NOTE_H_DO, 0, NOTE_H_RE, 0, NOTE_H_MI, NOTE_H_MI, NOTE_H_MI};
const uint8_t music1_len = sizeof(music1) / sizeof(music1[0]);