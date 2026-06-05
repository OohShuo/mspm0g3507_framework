#include <stdint.h>

// Tone frequency definitions
// 单位 微秒，频率 = 1,000,000 / 周期
#define NOTE_XL_DO 7645  // C3  131Hz  超低音
#define NOTE_XL_RE 6811  // D3  147Hz
#define NOTE_XL_MI 6067  // E3  165Hz
#define NOTE_XL_FA 5727  // F3  175Hz
#define NOTE_XL_SO 5102  // G3  196Hz
#define NOTE_XL_LA 4545  // A3  220Hz
#define NOTE_XL_XI 4049  // B3  247Hz
#define NOTE_L_DO 3822   // C4  262Hz
#define NOTE_L_RE 3405   // D4  294Hz
#define NOTE_L_MI 3033   // E4  330Hz
#define NOTE_L_FA 2863   // F4  349Hz
#define NOTE_L_SO 2551   // G4  392Hz
#define NOTE_L_GSH 2408  // G#4 415Hz
#define NOTE_L_LA 2272   // A4  440Hz
#define NOTE_L_XI 2024   // B4  494Hz
#define NOTE_L_FSH 2711  // F#4 370Hz
#define NOTE_L_CSH 3640  // C#4 277Hz
#define NOTE_M_DO 1911   // C5  523Hz
#define NOTE_M_RE 1702   // D5  587Hz
#define NOTE_M_DSH 1607  // D#5 622Hz
#define NOTE_M_MI 1526   // E5  659Hz
#define NOTE_M_FA 1431   // F5  698Hz
#define NOTE_M_SO 1275   // G5  784Hz
#define NOTE_M_LA 1136   // A5  880Hz
#define NOTE_M_XI 1012   // B5  988Hz
#define NOTE_H_DO 955    // C6  1047Hz
#define NOTE_H_RE 851    // D6  1175Hz
#define NOTE_H_MI 758    // E6  1319Hz
#define NOTE_H_FA 715    // F6  1399Hz
#define NOTE_H_CSH 721   // C#6 1387Hz
#define NOTE_H_SO 637    // G6  1568Hz
#define NOTE_H_LA 568    // A6  1760Hz
#define NOTE_H_XI 506    // B6  1976Hz
#define NOTE_X_DO 478    // C7  2093Hz
#define NOTE_REST 0
#define GLISS 0x8000  // 滑音标记（与 buzzer.h 中 NOTE_FLAG_GLISSANDO 一致）

// 8-bit 游戏机风格主题曲（低八度）
const uint16_t music1[] = {
    // A段 - 跳跃主旋律
    NOTE_L_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO,
    NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_M_MI,
    NOTE_M_DO, NOTE_L_SO, NOTE_M_DO, NOTE_M_MI,
    NOTE_M_RE, NOTE_M_DO, NOTE_L_XI, NOTE_M_DO,

    // B段 - 冲刺感
    NOTE_M_DO, NOTE_M_RE, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_L_XI, NOTE_L_LA, NOTE_L_XI,
    NOTE_M_DO, NOTE_REST, NOTE_M_DO, NOTE_REST,
    NOTE_L_SO, NOTE_L_LA, NOTE_L_XI, NOTE_M_DO,

    // A段重复
    NOTE_L_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO,
    NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_M_MI,
    NOTE_M_DO, NOTE_L_SO, NOTE_M_DO, NOTE_M_MI,
    NOTE_M_RE, NOTE_M_DO, NOTE_L_XI, NOTE_M_DO,

    // C段 - 高潮上行
    NOTE_L_DO, NOTE_L_RE, NOTE_L_MI, NOTE_L_FA,
    NOTE_L_SO, NOTE_L_LA, NOTE_L_XI, NOTE_M_DO,
    NOTE_M_DO, NOTE_M_RE, NOTE_M_MI, NOTE_M_FA,
    NOTE_M_SO, NOTE_REST, NOTE_REST,
};
const uint8_t music1_len = sizeof(music1) / sizeof(music1[0]);

// 胜利音效 - 上行欢快旋律，循环播放
const uint16_t music2[] = {
    NOTE_L_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO,
    NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_M_MI,
    NOTE_L_SO, NOTE_M_DO, NOTE_M_MI, NOTE_M_SO,
    NOTE_M_DO, NOTE_M_MI, NOTE_M_SO, NOTE_REST,
    NOTE_M_MI, NOTE_M_SO, NOTE_REST, NOTE_M_DO,
    NOTE_REST,
};
const uint8_t music2_len = sizeof(music2) / sizeof(music2[0]);

// 死亡音效 - 下行阴沉旋律，循环播放
const uint16_t music3[] = {
    NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_L_XI,
    NOTE_L_LA, NOTE_L_SO, NOTE_L_FA, NOTE_L_MI,
    NOTE_L_MI, NOTE_L_FA, NOTE_L_MI, NOTE_L_RE,
    NOTE_L_MI, NOTE_REST, NOTE_REST,
};
const uint8_t music3_len = sizeof(music3) / sizeof(music3[0]);

// 致爱丽丝 (Für Elise) - 备用
const uint16_t music4[] = {
    NOTE_M_MI, NOTE_M_DSH, NOTE_M_MI, NOTE_M_DSH,
    NOTE_M_MI, NOTE_L_XI, NOTE_M_RE, NOTE_M_DO,
    NOTE_L_LA, NOTE_REST, NOTE_REST, NOTE_REST,
    NOTE_L_DO, NOTE_L_MI, NOTE_L_LA, NOTE_L_XI,
    NOTE_L_MI, NOTE_REST, NOTE_REST, NOTE_REST,
    NOTE_L_MI, NOTE_L_GSH, NOTE_L_MI, NOTE_L_XI,
    NOTE_M_DO, NOTE_REST, NOTE_REST, NOTE_REST,
    NOTE_L_LA, NOTE_REST, NOTE_REST, NOTE_REST,
    NOTE_L_XI, NOTE_M_DO, NOTE_M_RE, NOTE_M_DO,
    NOTE_L_XI, NOTE_L_LA,
    NOTE_M_DO, NOTE_L_MI, NOTE_L_LA, NOTE_L_XI,
    NOTE_M_MI, NOTE_M_DSH, NOTE_M_MI, NOTE_M_DSH,
    NOTE_M_MI, NOTE_L_XI, NOTE_M_RE, NOTE_M_DO,
    NOTE_L_LA};
const uint8_t music4_len = sizeof(music4) / sizeof(music4[0]);

// 8-bit 游戏主题曲 2 - 冒险风格（低八度）
const uint16_t music5[] = {
    // A段 - 明亮主旋律
    NOTE_M_DO, NOTE_L_XI, NOTE_M_DO, NOTE_L_SO,
    NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_REST,
    NOTE_L_XI, NOTE_M_DO, NOTE_L_XI, NOTE_L_LA,
    NOTE_L_SO, NOTE_REST, NOTE_REST,

    // B段 - 跳跃节奏
    NOTE_L_MI, NOTE_L_FA, NOTE_L_SO, NOTE_L_LA,
    NOTE_L_SO, NOTE_L_FA, NOTE_L_MI, NOTE_REST,
    NOTE_L_DO, NOTE_L_RE, NOTE_L_MI, NOTE_L_FA,
    NOTE_L_MI, NOTE_L_RE, NOTE_L_DO, NOTE_REST,

    // A段重复
    NOTE_M_DO, NOTE_L_XI, NOTE_M_DO, NOTE_L_SO,
    NOTE_L_MI, NOTE_L_SO, NOTE_M_DO, NOTE_REST,
    NOTE_L_XI, NOTE_M_DO, NOTE_L_XI, NOTE_L_LA,
    NOTE_L_SO, NOTE_REST, NOTE_REST,

    // C段 - 高潮
    NOTE_L_DO, NOTE_L_MI, NOTE_L_SO, NOTE_M_DO,
    NOTE_M_RE, NOTE_M_DO, NOTE_L_XI, NOTE_M_DO,
    NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_L_XI,
    NOTE_M_DO, NOTE_REST, NOTE_REST,
};
const uint8_t music5_len = sizeof(music5) / sizeof(music5[0]);

// 卡农 (Canon in D) - Pachelbel 经典旋律（低八度）
const uint16_t music6[] = {
    // F#4 E4 D4 C#4 | D4 E4 F#4 D4
    NOTE_L_FSH, NOTE_L_MI, NOTE_L_RE, NOTE_L_CSH,
    NOTE_L_RE, NOTE_L_MI, NOTE_L_FSH, NOTE_L_RE,
    NOTE_REST,

    // E4 D4 C#4 B3 | A3 B3 C#4 D4
    NOTE_L_MI, NOTE_L_RE, NOTE_L_CSH, NOTE_XL_XI,
    NOTE_XL_LA, NOTE_XL_XI, NOTE_L_CSH, NOTE_L_RE,
    NOTE_REST, NOTE_REST,

    // 主题重复
    NOTE_L_FSH, NOTE_L_MI, NOTE_L_RE, NOTE_L_CSH,
    NOTE_L_RE, NOTE_L_MI, NOTE_L_FSH, NOTE_L_RE,
    NOTE_REST,

    // E4 D4 C#4 B3 | A3 B3 C#4 A3
    NOTE_L_MI, NOTE_L_RE, NOTE_L_CSH, NOTE_XL_XI,
    NOTE_XL_LA, NOTE_XL_XI, NOTE_L_CSH, NOTE_XL_LA,
    NOTE_REST, NOTE_REST,
};
const uint8_t music6_len = sizeof(music6) / sizeof(music6[0]);

// 二次元风格主题曲 3 - 带滑音的动漫风
const uint16_t music7[] = {
    // A段 - 旋律线，用滑音连接大跳音程
    NOTE_M_LA, NOTE_M_XI, NOTE_H_DO, NOTE_M_XI,
    NOTE_M_LA | GLISS, NOTE_H_RE, NOTE_H_DO, NOTE_M_XI,
    NOTE_M_LA, NOTE_M_SO, NOTE_M_LA, NOTE_M_XI,
    NOTE_M_MI, NOTE_REST, NOTE_REST,

    // B段 - 上升感
    NOTE_M_MI, NOTE_M_SO, NOTE_M_LA, NOTE_H_DO,
    NOTE_H_RE | GLISS, NOTE_H_MI, NOTE_H_RE, NOTE_H_DO,
    NOTE_M_XI, NOTE_H_DO, NOTE_M_XI, NOTE_M_LA,
    NOTE_M_SO, NOTE_REST, NOTE_REST,

    // A段重复
    NOTE_M_LA, NOTE_M_XI, NOTE_H_DO, NOTE_M_XI,
    NOTE_M_LA | GLISS, NOTE_H_RE, NOTE_H_DO, NOTE_M_XI,
    NOTE_M_LA, NOTE_M_SO, NOTE_M_LA, NOTE_M_XI,
    NOTE_M_MI, NOTE_REST, NOTE_REST,

    // C段 - 高潮！大幅滑音
    NOTE_M_DO | GLISS, NOTE_M_MI, NOTE_M_SO | GLISS, NOTE_H_DO,
    NOTE_H_RE | GLISS, NOTE_H_MI, NOTE_H_FA | GLISS, NOTE_H_MI,
    NOTE_H_RE, NOTE_H_DO, NOTE_M_XI, NOTE_H_DO,
    NOTE_H_RE | GLISS, NOTE_H_MI, NOTE_REST,
};
const uint8_t music7_len = sizeof(music7) / sizeof(music7[0]);

// 欢乐颂 (Ode to Joy) - Beethoven（低八度）
const uint16_t music8[] = {
    NOTE_M_MI, NOTE_M_MI, NOTE_M_FA, NOTE_M_SO,
    NOTE_M_SO, NOTE_M_FA, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_M_DO, NOTE_M_RE, NOTE_M_MI,
    NOTE_M_MI, NOTE_REST, NOTE_M_RE, NOTE_M_RE, NOTE_REST,

    NOTE_M_MI, NOTE_M_MI, NOTE_M_FA, NOTE_M_SO,
    NOTE_M_SO, NOTE_M_FA, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_M_DO, NOTE_M_RE, NOTE_M_MI,
    NOTE_M_RE, NOTE_REST, NOTE_M_DO, NOTE_M_DO, NOTE_REST,

    NOTE_M_RE, NOTE_M_RE, NOTE_M_MI, NOTE_M_DO,
    NOTE_M_RE, NOTE_M_MI, NOTE_M_FA, NOTE_M_MI, NOTE_M_DO,
    NOTE_M_RE, NOTE_M_MI, NOTE_M_FA, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_M_RE, NOTE_M_SO, NOTE_REST,

    NOTE_M_MI, NOTE_M_MI, NOTE_M_FA, NOTE_M_SO,
    NOTE_M_SO, NOTE_M_FA, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_M_DO, NOTE_M_RE, NOTE_M_MI,
    NOTE_M_RE, NOTE_REST, NOTE_M_DO, NOTE_M_DO,
};
const uint8_t music8_len = sizeof(music8) / sizeof(music8[0]);

// 小星星 (Twinkle Twinkle Little Star)（低八度）
const uint16_t music9[] = {
    NOTE_M_DO, NOTE_M_DO, NOTE_M_SO, NOTE_M_SO,
    NOTE_M_LA, NOTE_M_LA, NOTE_M_SO, NOTE_REST,
    NOTE_M_FA, NOTE_M_FA, NOTE_M_MI, NOTE_M_MI,
    NOTE_M_RE, NOTE_M_RE, NOTE_M_DO, NOTE_REST,

    NOTE_M_SO, NOTE_M_SO, NOTE_M_FA, NOTE_M_FA,
    NOTE_M_MI, NOTE_M_MI, NOTE_M_RE, NOTE_REST,
    NOTE_M_SO, NOTE_M_SO, NOTE_M_FA, NOTE_M_FA,
    NOTE_M_MI, NOTE_M_MI, NOTE_M_RE, NOTE_REST,

    NOTE_M_DO, NOTE_M_DO, NOTE_M_SO, NOTE_M_SO,
    NOTE_M_LA, NOTE_M_LA, NOTE_M_SO, NOTE_REST,
    NOTE_M_FA, NOTE_M_FA, NOTE_M_MI, NOTE_M_MI,
    NOTE_M_RE, NOTE_M_RE, NOTE_M_DO,
};
const uint8_t music9_len = sizeof(music9) / sizeof(music9[0]);

// 超级马里奥 (Super Mario Bros Theme)（低八度）
const uint16_t music10[] = {
    NOTE_M_RE, NOTE_M_RE, NOTE_REST, NOTE_M_DO,
    NOTE_M_RE, NOTE_REST, NOTE_M_SO, NOTE_REST,
    NOTE_L_SO, NOTE_REST, NOTE_REST, NOTE_REST,

    NOTE_M_DO, NOTE_REST, NOTE_REST, NOTE_L_SO,
    NOTE_REST, NOTE_REST, NOTE_L_MI, NOTE_REST,
    NOTE_REST, NOTE_REST, NOTE_L_LA, NOTE_REST,
    NOTE_L_XI, NOTE_REST, NOTE_L_LA, NOTE_REST,

    NOTE_L_SO, NOTE_M_MI, NOTE_M_SO,
    NOTE_M_LA, NOTE_REST, NOTE_M_FA, NOTE_M_SO,
    NOTE_REST, NOTE_M_MI, NOTE_M_DO, NOTE_M_RE,
    NOTE_L_XI, NOTE_REST, NOTE_REST, NOTE_REST,
};
const uint8_t music10_len = sizeof(music10) / sizeof(music10[0]);

// 喀秋莎 (Katyusha)（低八度）
const uint16_t music11[] = {
    NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_M_RE,
    NOTE_M_MI, NOTE_M_FA, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_REST, NOTE_M_DO, NOTE_M_RE,
    NOTE_M_MI, NOTE_M_FA, NOTE_M_MI, NOTE_M_RE,

    NOTE_M_DO, NOTE_REST, NOTE_M_RE, NOTE_M_MI,
    NOTE_M_FA, NOTE_M_MI, NOTE_M_RE, NOTE_M_DO,
    NOTE_M_RE, NOTE_REST, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_M_RE, NOTE_M_MI, NOTE_REST,

    NOTE_M_FA, NOTE_M_MI, NOTE_M_RE, NOTE_M_DO,
    NOTE_M_RE, NOTE_M_MI, NOTE_M_FA, NOTE_M_MI,
    NOTE_M_RE, NOTE_M_DO, NOTE_REST, NOTE_M_DO,
    NOTE_M_RE, NOTE_M_MI, NOTE_M_FA, NOTE_REST,
};
const uint8_t music11_len = sizeof(music11) / sizeof(music11[0]);

// 天空之城 (Castle in the Sky) - 久石让（低八度）
const uint16_t music12[] = {
    NOTE_M_DO, NOTE_REST, NOTE_L_SO, NOTE_REST,
    NOTE_M_DO, NOTE_M_RE, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_REST, NOTE_L_LA, NOTE_REST,

    NOTE_M_DO, NOTE_M_RE, NOTE_M_MI, NOTE_M_FA,
    NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_REST,
    NOTE_L_SO, NOTE_REST, NOTE_M_DO, NOTE_REST,

    NOTE_M_DO, NOTE_M_RE, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_REST, NOTE_M_DO, NOTE_REST,
    NOTE_M_RE, NOTE_M_MI, NOTE_M_FA, NOTE_REST,
    NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_REST,
};
const uint8_t music12_len = sizeof(music12) / sizeof(music12[0]);

// 二次元风格主题曲 4 - 情感向，大跳滑音
const uint16_t music13[] = {
    // A段 - 温柔开场
    NOTE_M_LA, NOTE_M_XI, NOTE_H_DO, NOTE_H_RE,
    NOTE_H_DO | GLISS, NOTE_M_XI, NOTE_M_LA, NOTE_REST,
    NOTE_M_LA, NOTE_H_DO, NOTE_H_RE, NOTE_H_MI,
    NOTE_H_RE | GLISS, NOTE_H_DO, NOTE_M_XI, NOTE_REST,

    // B段 - 情感上升
    NOTE_M_SO, NOTE_M_LA, NOTE_M_XI, NOTE_H_DO,
    NOTE_H_RE | GLISS, NOTE_H_MI, NOTE_H_FA, NOTE_REST,
    NOTE_H_MI, NOTE_H_RE, NOTE_H_DO, NOTE_M_XI,
    NOTE_H_DO | GLISS, NOTE_H_RE, NOTE_REST, NOTE_REST,

    // A段重复
    NOTE_M_LA, NOTE_M_XI, NOTE_H_DO, NOTE_H_RE,
    NOTE_H_DO | GLISS, NOTE_M_XI, NOTE_M_LA, NOTE_REST,
    NOTE_M_LA, NOTE_H_DO, NOTE_H_RE, NOTE_H_MI,
    NOTE_H_RE | GLISS, NOTE_H_DO, NOTE_M_XI, NOTE_REST,

    // C段 - 高潮！
    NOTE_M_DO | GLISS, NOTE_M_MI, NOTE_M_SO | GLISS, NOTE_H_DO,
    NOTE_H_RE | GLISS, NOTE_H_MI, NOTE_H_FA | GLISS, NOTE_H_MI,
    NOTE_H_RE, NOTE_H_DO, NOTE_M_XI, NOTE_H_DO,
    NOTE_H_RE | GLISS, NOTE_H_MI, NOTE_REST,
};
const uint8_t music13_len = sizeof(music13) / sizeof(music13[0]);

// 低音经典合奏 - 欢乐颂 + 小星星 + 天空之城 + 喀秋莎
const uint16_t music14[] = {
    // 欢乐颂
    NOTE_M_MI, NOTE_M_MI, NOTE_M_FA, NOTE_M_SO,
    NOTE_M_SO, NOTE_M_FA, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_M_DO, NOTE_M_RE, NOTE_M_MI,
    NOTE_M_MI, NOTE_REST, NOTE_M_RE, NOTE_M_RE, NOTE_REST,

    NOTE_M_MI, NOTE_M_MI, NOTE_M_FA, NOTE_M_SO,
    NOTE_M_SO, NOTE_M_FA, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_M_DO, NOTE_M_RE, NOTE_M_MI,
    NOTE_M_RE, NOTE_REST, NOTE_M_DO, NOTE_M_DO, NOTE_REST,

    // 小星星
    NOTE_M_DO, NOTE_M_DO, NOTE_M_SO, NOTE_M_SO,
    NOTE_M_LA, NOTE_M_LA, NOTE_M_SO, NOTE_REST,
    NOTE_M_FA, NOTE_M_FA, NOTE_M_MI, NOTE_M_MI,
    NOTE_M_RE, NOTE_M_RE, NOTE_M_DO, NOTE_REST,

    NOTE_M_SO, NOTE_M_SO, NOTE_M_FA, NOTE_M_FA,
    NOTE_M_MI, NOTE_M_MI, NOTE_M_RE, NOTE_REST,
    NOTE_M_SO, NOTE_M_SO, NOTE_M_FA, NOTE_M_FA,
    NOTE_M_MI, NOTE_M_MI, NOTE_M_RE, NOTE_REST,

    // 天空之城
    NOTE_M_DO, NOTE_REST, NOTE_L_SO, NOTE_REST,
    NOTE_M_DO, NOTE_M_RE, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_REST, NOTE_L_LA, NOTE_REST,

    NOTE_M_DO, NOTE_M_RE, NOTE_M_MI, NOTE_M_FA,
    NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_REST,
    NOTE_L_SO, NOTE_REST, NOTE_M_DO, NOTE_REST,

    NOTE_M_DO, NOTE_M_RE, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_REST, NOTE_M_DO, NOTE_REST,
    NOTE_M_RE, NOTE_M_MI, NOTE_M_FA, NOTE_REST,
    NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_REST,

    // 喀秋莎
    NOTE_M_MI, NOTE_M_RE, NOTE_M_DO, NOTE_M_RE,
    NOTE_M_MI, NOTE_M_FA, NOTE_M_MI, NOTE_M_RE,
    NOTE_M_DO, NOTE_REST, NOTE_M_DO, NOTE_M_RE,
    NOTE_M_MI, NOTE_M_FA, NOTE_M_MI, NOTE_M_RE,

    NOTE_M_DO, NOTE_REST, NOTE_M_RE, NOTE_M_MI,
    NOTE_M_FA, NOTE_M_MI, NOTE_M_RE, NOTE_M_DO,
    NOTE_M_RE, NOTE_REST,
};
const uint8_t music14_len = sizeof(music14) / sizeof(music14[0]);

// 哈利波特 (Hedwig's Theme) - John Williams
const uint16_t music15[] = {
    // 开头：神秘的重复音
    NOTE_L_MI, NOTE_L_LA, NOTE_L_XI,
    NOTE_M_DO, NOTE_L_XI, NOTE_L_LA,
    NOTE_M_MI, NOTE_M_RE | GLISS, NOTE_L_XI,
    NOTE_M_DO, NOTE_L_XI, NOTE_L_LA,
    NOTE_L_MI | GLISS, NOTE_L_SO, NOTE_L_LA,

    // 主题
    NOTE_L_MI, NOTE_L_LA, NOTE_L_XI,
    NOTE_M_DO, NOTE_L_XI, NOTE_L_LA,
    NOTE_M_MI, NOTE_M_FA, NOTE_M_MI,
    NOTE_M_RE | GLISS, NOTE_L_XI,
    NOTE_M_DO, NOTE_L_XI, NOTE_L_LA,
};
const uint8_t music15_len = sizeof(music15) / sizeof(music15[0]);

// 龙猫 (Totoro) - 久石让
const uint16_t music16[] = {
    // 开头
    NOTE_L_MI, NOTE_L_MI, NOTE_L_MI, NOTE_L_RE,
    NOTE_L_DO, NOTE_L_RE, NOTE_L_MI, NOTE_L_SO,
    NOTE_L_MI, NOTE_REST, NOTE_L_MI, NOTE_L_MI,

    NOTE_L_MI, NOTE_L_RE, NOTE_L_DO, NOTE_L_RE,
    NOTE_L_MI, NOTE_L_SO, NOTE_L_MI, NOTE_REST,

    NOTE_L_MI, NOTE_L_MI, NOTE_L_SO, NOTE_L_MI,
    NOTE_L_LA, NOTE_L_SO, NOTE_L_MI, NOTE_L_SO,
    NOTE_L_MI, NOTE_L_RE, NOTE_L_DO, NOTE_L_RE,
    NOTE_L_MI, NOTE_REST,
};
const uint8_t music16_len = sizeof(music16) / sizeof(music16[0]);

// 生日快乐 (Happy Birthday)
const uint16_t music17[] = {
    NOTE_L_SO, NOTE_L_SO, NOTE_L_LA, NOTE_L_SO, NOTE_M_DO, NOTE_L_XI,
    NOTE_L_SO, NOTE_L_SO, NOTE_L_LA, NOTE_L_SO, NOTE_M_RE, NOTE_M_DO,
    NOTE_L_SO, NOTE_L_SO, NOTE_L_SO, NOTE_L_MI, NOTE_M_DO, NOTE_L_XI, NOTE_L_LA,
    NOTE_M_FA, NOTE_M_FA, NOTE_M_MI, NOTE_M_DO, NOTE_M_RE, NOTE_M_DO,
};
const uint8_t music17_len = sizeof(music17) / sizeof(music17[0]);
