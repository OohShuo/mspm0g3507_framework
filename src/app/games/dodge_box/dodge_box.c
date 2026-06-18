#include "dodge_box.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH        240
#define SCREEN_HEIGHT       320

/* 你要求的实际战斗地图：120 x 180。这里放在 240 x 320 屏幕中间偏下。 */
#define ARENA_W             120
#define ARENA_H             180
#define ARENA_X             ((SCREEN_WIDTH - ARENA_W) / 2)
#define ARENA_Y             66

#define PLAYER_SIZE         5
#define PLAYER_SPEED_PX_S   50u
#define DEFAULT_WARNING_MS  500u
#define DEFAULT_EXIST_MS    500u
#define LASER_SWEEP_DRAW_MS 500u
#define LASER_POS_TRACK_PLAYER 255u
#define LASER_TRACK_JITTER_PX 5

static uint16_t dim_color(uint16_t c) {
    // simple RGB565 dimming
    uint16_t r = (c >> 11) & 0x1F;
    uint16_t g = (c >> 5) & 0x3F;
    uint16_t b = (c) & 0x1F;
    r = r >> 1;
    g = g >> 1;
    b = b >> 1;
    return (r << 11) | (g << 5) | b;
}

#define MAX_DT_MS        50u
#define BULLET_MAX_COUNT 32u
#define BULLET_DIR_SCALE 256l

#define SAFETY_BYTES     ((ARENA_W * ARENA_H + 7) / 8)
#define ATTACK_COUNT     ((uint8_t)(sizeof(g_level) / sizeof(g_level[0])))

#define COLOR_BLACK      0x0000u
#define COLOR_WHITE      0xffffu
#define COLOR_RED        0xf800u
#define COLOR_GREEN      0x07e0u
#define COLOR_BLUE       0x001fu
#define COLOR_CYAN       0x07ffu
#define COLOR_YELLOW     0xffe0u
#define COLOR_MAGENTA    0xf81fu
#define COLOR_GRAY       0x8410u
#define COLOR_DARK       0x2104u
#define COLOR_DIM_RED    0x6000u
#define COLOR_DIM_CYAN   0x0339u
#define COLOR_DIM_YELLOW 0x6300u

typedef enum {
    attack_laser_h,
    attack_laser_v,
    attack_rect,
    attack_bullet,
} Attack_type;

typedef enum {
    laser_warn_dash_flash,
    laser_warn_sweep_ltr,
    laser_warn_sweep_rtl,
    laser_warn_sweep_ttb,
    laser_warn_sweep_btt,
} Laser_warn_style;

typedef enum {
    rect_warn_center_expand,
    rect_warn_block_flash,
    rect_warn_edge_grow,
    rect_warn_fill_from_left,
    rect_warn_fill_from_right,
    rect_warn_fill_from_top,
    rect_warn_fill_from_bottom,
} Rect_warn_style;

typedef enum {
    bullet_side_top,
    bullet_side_bottom,
    bullet_side_left,
    bullet_side_right,
} Bullet_side;

typedef struct {
    uint32_t start_ms; /* 提示开始时间。 */
    uint16_t warn_ms;  /* 提示持续时间。 */
    uint16_t exist_ms; /* 激光/区域：攻击存在持续时间；子弹：填 0，直到撞边完成。 */
    Attack_type type;
    uint16_t color;
    union {
        struct {
            uint8_t pos; /* horizontal: y, vertical: x, in arena coords */
            uint8_t thickness;
            Laser_warn_style style;
        } laser;
        struct {
            uint8_t x;
            uint8_t y;
            uint8_t w;
            uint8_t h;
            Rect_warn_style style;
        } rect;
        struct {
            Bullet_side side;
            uint8_t offset; /* top/bottom: x；left/right: y */
            uint8_t size;
            uint16_t speed_px_s;
            uint8_t alpha; /* 0~255，越大越保持上周期方向 */
        } bullet;
    } u;
} Attack;

typedef struct {
    uint8_t initialized;
    uint8_t active;
    uint8_t finished;
    int32_t x256;
    int32_t y256;
    int32_t prev_x256;
    int32_t prev_y256;
    int16_t dir_x; /* Q8 方向向量 */
    int16_t dir_y;
} Bullet_runtime;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} Rect;

typedef enum {
    game_state_ready,
    game_state_playing,
    game_state_failed,
    game_state_clear,
} Dodge_state;

static St7789* g_lcd = NULL;
static Dodge_state g_state = game_state_ready;
static uint32_t g_start_ms = 0;
static uint32_t g_last_ms = 0;
static uint32_t g_survive_ms = 0;
static uint8_t g_finished = 0;
/* 已完成攻击扫描索引：攻击完成后递增；全部完成后再等待 2s 判定胜利。 */
static uint8_t g_attack_scan_index = 0;
static uint8_t g_all_attacks_done = 0;
static uint32_t g_all_attacks_done_ms = 0;
static int32_t g_player_x256 = 0;
static int32_t g_player_y256 = 0;
static int16_t g_prev_player_x = 0;
static int16_t g_prev_player_y = 0;
static Bullet_runtime g_bullets[BULLET_MAX_COUNT];

/* bit = 1 表示危险；0 表示安全。内存 120*180/8 = 2700 bytes。 */
static uint8_t g_danger_map[SAFETY_BYTES];

/* 一个完整单关卡。时间配置为：开始 ms、提示持续 ms、攻击存在持续 ms。
 * 原有关卡节奏等价迁移：旧提示均为 500ms，旧 end_ms 转为 exist_ms。
 * 子弹没有固定 end_ms，exist_ms 填 0，撞到边框后才完成。
 */
static const Attack g_level[] = {
    // phase 1
    {500u, 1500u, 0u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 0u, 120u, 180u, rect_warn_block_flash}}},

    {2500+200u, 1000u, 300u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {90u, 2u, laser_warn_dash_flash}}},
    {2500+200u, 1000u, 300u, attack_laser_v, COLOR_DIM_CYAN, {.laser = {60u, 2u, laser_warn_dash_flash}}},

    {2500+2000u, 1000u, 300u, attack_rect, COLOR_DIM_CYAN, {.rect = {40u, 60u, 40u, 60u, rect_warn_center_expand}}},

    {2500+3500u, 700u, 6000u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 0u, 120u, 60u, rect_warn_fill_from_top}}},
    {2500+3500u, 700u, 6000u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 0u, 30u, 180u, rect_warn_fill_from_left}}},
    {2500+3500u, 700u, 6000u, attack_rect, COLOR_DIM_CYAN, {.rect = {90u, 0u, 30u, 180u, rect_warn_fill_from_right}}},
    {2500+3500u, 700u, 6000u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 120u, 120u, 60u, rect_warn_fill_from_bottom}}},

    {2500+4200u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 30u, 4u, 50u, 250u}}},
    {2500+4500u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 40u, 4u, 50u, 250u}}},
    {2500+4800u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 50u, 4u, 50u, 250u}}},
    {2500+5100u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 60u, 4u, 50u, 250u}}},

    {2500+6600u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 30u, 4u, 80u, 250u}}},
    {2500+6900u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 40u, 4u, 80u, 250u}}},
    {2500+7200u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 50u, 4u, 80u, 250u}}},
    {2500+7500u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 60u, 4u, 80u, 250u}}},

    {2500+10000u, 500u, 4400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {90u, 3u, laser_warn_sweep_ltr}}},
    {2500+10300u, 500u, 3900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {80u, 3u, laser_warn_sweep_rtl}}},
    {2500+10300u, 500u, 3900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {100u, 3u, laser_warn_sweep_rtl}}},
    {2500+10600u, 500u, 3400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {70u, 3u, laser_warn_sweep_ltr}}},
    {2500+10600u, 500u, 3400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {110u, 3u, laser_warn_sweep_ltr}}},
    {2500+10900u, 500u, 2900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {60u, 3u, laser_warn_sweep_rtl}}},
    {2500+10900u, 500u, 2900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {120u, 3u, laser_warn_sweep_rtl}}},
    {2500+11200u, 500u, 2400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {50u, 3u, laser_warn_sweep_ltr}}},
    {2500+11200u, 500u, 2400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {130u, 3u, laser_warn_sweep_ltr}}},
    {2500+11500u, 500u, 1900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {40u, 3u, laser_warn_sweep_rtl}}},
    {2500+11500u, 500u, 1900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {140u, 3u, laser_warn_sweep_rtl}}},
    {2500+11800u, 500u, 1400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {30u, 3u, laser_warn_sweep_ltr}}},
    {2500+11800u, 500u, 1400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {150u, 3u, laser_warn_sweep_ltr}}},
    {2500+12100u, 500u, 900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {20u, 3u, laser_warn_sweep_rtl}}},
    {2500+12100u, 500u, 900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {160u, 3u, laser_warn_sweep_rtl}}},

    {2500+12500u, 500u, 500u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 0u, 50u, 15u, rect_warn_fill_from_left}}},
    {2500+12500u, 500u, 500u, attack_rect, COLOR_DIM_CYAN, {.rect = {70u, 0u, 50u, 15u, rect_warn_fill_from_right}}},
    {2500+12500u, 500u, 500u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 165u, 50u, 15u, rect_warn_fill_from_left}}},
    {2500+12500u, 500u, 500u, attack_rect, COLOR_DIM_CYAN, {.rect = {70u, 165u, 50u, 15u, rect_warn_fill_from_right}}},

    {2500+13000u, 2500u, 1000u, attack_rect, COLOR_DIM_CYAN, {.rect = {55u, 0u, 10u, 80u, rect_warn_fill_from_top}}},
    {2500+13000u, 2500u, 1000u, attack_rect, COLOR_DIM_CYAN, {.rect = {55u, 100u, 10u, 80u, rect_warn_fill_from_bottom}}},
    {2500+13000u, 2500u, 1000u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 85u, 50u, 10u, rect_warn_fill_from_left}}},
    {2500+13000u, 2500u, 1000u, attack_rect, COLOR_DIM_CYAN, {.rect = {70u, 85u, 50u, 10u, rect_warn_fill_from_right}}},

    {2500+15500u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 2u, 4u, 80u, 255u}}},
    {2500+15500u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 112u, 4u, 80u, 255u}}},
    {2500+15500u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 2u, 4u, 80u, 255u}}},
    {2500+15500u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 112u, 4u, 80u, 255u}}},

    // phase 2
    {19500u, 1500u, 0u, attack_rect, COLOR_DIM_YELLOW, {.rect = {0u, 0u, 120u, 180u, rect_warn_block_flash}}},

    {21500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {22000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {22500u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {23000u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {23500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {24000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {24500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {25000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {25500u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {26000u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {26500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {27000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {27500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {28000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {28500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {29000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {29500u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {30000u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {30500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {31000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {31500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {32000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {32500u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {33000u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {33500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {34000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {34500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {35000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {35500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {36000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {36500u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {37000u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {37500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {38000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {38500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {39000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {39500u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {40000u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {40500u, 750u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {41000u, 750u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},

};

/* 追身激光运行时位置缓存：
 * 配置 pos = LASER_POS_TRACK_PLAYER 时，攻击开始后只解析一次。
 * 这样 warning 和 active 使用同一个位置，不会每帧抖动。
 */
static uint8_t g_laser_track_pos[ATTACK_COUNT];
static uint8_t g_laser_track_pos_valid[ATTACK_COUNT];
static uint32_t g_rng_state = 0x4d595df4u;

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

static uint32_t rng_next(void) {
    /* 轻量 LCG，不依赖 rand()/stdlib，适合 MCU。 */
    g_rng_state = g_rng_state * 1664525u + 1013904223u;
    return g_rng_state;
}

static int16_t rng_jitter_pm5(void) {
    return (int16_t)((int32_t)(rng_next() % (uint32_t)(LASER_TRACK_JITTER_PX * 2 + 1)) - LASER_TRACK_JITTER_PX);
}

static uint8_t laser_pos_is_tracking(const Attack* a) {
    return (uint8_t)((a->type == attack_laser_h || a->type == attack_laser_v) &&
                     a->u.laser.pos == LASER_POS_TRACK_PLAYER);
}

static uint8_t resolve_laser_pos(uint8_t index, const Attack* a) {
    if (!laser_pos_is_tracking(a)) { return a->u.laser.pos; }

    if (!g_laser_track_pos_valid[index]) {
        const int16_t th = a->u.laser.thickness;
        int16_t base;
        int16_t max_pos;

        if (a->type == attack_laser_h) {
            /* 水平激光追玩家 y 轴中心，并让激光厚度居中覆盖该位置附近。 */
            base = (int16_t)((g_player_y256 >> 8) + PLAYER_SIZE / 2 - th / 2);
            max_pos = ARENA_H - th;
        } else {
            /* 垂直激光追玩家 x 轴中心，并让激光厚度居中覆盖该位置附近。 */
            base = (int16_t)((g_player_x256 >> 8) + PLAYER_SIZE / 2 - th / 2);
            max_pos = ARENA_W - th;
        }

        base = (int16_t)(base + rng_jitter_pm5());
        g_laser_track_pos[index] = (uint8_t)clamp_i16(base, 0, max_pos);
        g_laser_track_pos_valid[index] = 1;
    }

    return g_laser_track_pos[index];
}

static uint32_t isqrt_u32(uint32_t x) {
    uint32_t op = x;
    uint32_t res = 0;
    uint32_t one = 1ul << 30;
    while (one > op) { one >>= 2; }
    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return res;
}

static void normalize_to_q8(int32_t dx, int32_t dy, int16_t* out_x, int16_t* out_y) {
    const uint32_t mag2 = (uint32_t)(dx * dx + dy * dy);
    if (mag2 == 0u) {
        *out_x = 0;
        *out_y = BULLET_DIR_SCALE;
        return;
    }
    uint32_t mag = isqrt_u32(mag2);
    if (mag == 0u) { mag = 1u; }
    *out_x = (int16_t)(dx * BULLET_DIR_SCALE / (int32_t)mag);
    *out_y = (int16_t)(dy * BULLET_DIR_SCALE / (int32_t)mag);
}

static uint16_t warning_color(uint16_t color) {
    /* 所有提示色：使用对应攻击颜色降亮度。
     * 不再用固定灰色/固定暗红，避免不同攻击提示风格不一致。
     */
    return dim_color(color);
}

static uint8_t laser_is_sweep_style(Laser_warn_style style) {
    return (uint8_t)(style == laser_warn_sweep_ltr || style == laser_warn_sweep_rtl ||
                     style == laser_warn_sweep_ttb || style == laser_warn_sweep_btt);
}

static uint32_t phase_total_nonzero(uint32_t total_ms) { return (total_ms == 0u) ? 1u : total_ms; }

static uint32_t attack_warn_ms(const Attack* a) { return (uint32_t)a->warn_ms; }

static uint32_t attack_exist_ms(const Attack* a) {
    if (a->type == attack_bullet) { return 0u; }
    return (uint32_t)a->exist_ms;
}

static uint32_t attack_end_ms(const Attack* a) {
    return a->start_ms + attack_warn_ms(a) + attack_exist_ms(a);
}

static int16_t warning_thickness(int16_t active_thickness) {
    /* 激光提示线必须比实体激光细，并尽量保持在实体激光中心。 */
    if (active_thickness <= 1) { return 1; }
    if (active_thickness <= 3) { return 1; }
    return (int16_t)(active_thickness - 2);
}

static int16_t centered_warn_pos(int16_t active_pos, int16_t active_thickness, int16_t warn_th) {
    return (int16_t)(active_pos + (active_thickness - warn_th) / 2);
}

static void bullet_spawn_rect(const Attack* a, int16_t* x, int16_t* y, int16_t* w, int16_t* h) {
    const int16_t size = a->u.bullet.size;
    const int16_t off = a->u.bullet.offset;
    *w = size;
    *h = size;
    if (a->u.bullet.side == bullet_side_top) {
        *x = clamp_i16(off, 0, ARENA_W - size);
        *y = 0;
    } else if (a->u.bullet.side == bullet_side_bottom) {
        *x = clamp_i16(off, 0, ARENA_W - size);
        *y = ARENA_H - size;
    } else if (a->u.bullet.side == bullet_side_left) {
        *x = 0;
        *y = clamp_i16(off, 0, ARENA_H - size);
    } else {
        *x = ARENA_W - size;
        *y = clamp_i16(off, 0, ARENA_H - size);
    }
}

static void arena_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) { return; }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > ARENA_W) { w = ARENA_W - x; }
    if (y + h > ARENA_H) { h = ARENA_H - y; }
    if (w <= 0 || h <= 0) { return; }
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X + x, ARENA_Y + y, w, h, color);
}

static void arena_border(void) {
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X - 2, ARENA_Y - 2, ARENA_W + 4, 2, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X - 2, ARENA_Y + ARENA_H, ARENA_W + 4, 2, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X - 2, ARENA_Y - 2, 2, ARENA_H + 4, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X + ARENA_W, ARENA_Y - 2, 2, ARENA_H + 4, COLOR_WHITE);
}

static void set_danger_pixel(uint8_t x, uint8_t y) {
    const uint16_t bit = (uint16_t)y * ARENA_W + x;
    g_danger_map[bit >> 3] |= (uint8_t)(1u << (bit & 7u));
}

static uint8_t is_danger_pixel(uint8_t x, uint8_t y) {
    const uint16_t bit = (uint16_t)y * ARENA_W + x;
    return (uint8_t)((g_danger_map[bit >> 3] >> (bit & 7u)) & 1u);
}

static void mark_danger_rect(int16_t x, int16_t y, int16_t w, int16_t h) {
    x = clamp_i16(x, 0, ARENA_W);
    y = clamp_i16(y, 0, ARENA_H);
    if (x + w > ARENA_W) { w = ARENA_W - x; }
    if (y + h > ARENA_H) { h = ARENA_H - y; }
    for (int16_t yy = y; yy < y + h; yy++) {
        for (int16_t xx = x; xx < x + w; xx++) { set_danger_pixel((uint8_t)xx, (uint8_t)yy); }
    }
}

static uint8_t player_hits_danger(void) {
    const int16_t px = (int16_t)(g_player_x256 >> 8);
    const int16_t py = (int16_t)(g_player_y256 >> 8);
    for (int16_t y = py; y < py + PLAYER_SIZE; y++) {
        for (int16_t x = px; x < px + PLAYER_SIZE; x++) {
            if (x >= 0 && x < ARENA_W && y >= 0 && y < ARENA_H && is_danger_pixel((uint8_t)x, (uint8_t)y)) {
                return 1;
            }
        }
    }
    return 0;
}

static void draw_player(uint16_t color) {
    const int16_t x = (int16_t)(g_player_x256 >> 8);
    const int16_t y = (int16_t)(g_player_y256 >> 8);
    arena_fill(x, y, PLAYER_SIZE, PLAYER_SIZE, color);
}

static void draw_text_centered(int16_t y, const char* text, uint8_t scale, uint16_t color) {
    int16_t len = 0;
    while (text[len] != '\0') { len++; }
    Game_Graphics_Draw_Text(g_lcd, (SCREEN_WIDTH - len * 6 * scale) / 2, y, text, scale, color);
}

static void draw_hline_dashed(int16_t y, int16_t thickness, uint16_t color, uint8_t phase) {
    for (int16_t x = 0; x < ARENA_W; x += 10) {
        if (((x / 10) & 1) == phase) { arena_fill(x, y, 7, thickness, color); }
    }
}

static void draw_vline_dashed(int16_t x, int16_t thickness, uint16_t color, uint8_t phase) {
    for (int16_t y = 0; y < ARENA_H; y += 10) {
        if (((y / 10) & 1) == phase) { arena_fill(x, y, thickness, 7, color); }
    }
}

static void draw_rect_border(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    arena_fill(x, y, w, 1, color);
    arena_fill(x, y + h - 1, w, 1, color);
    arena_fill(x, y, 1, h, color);
    arena_fill(x + w - 1, y, 1, h, color);
}

static void draw_rect_edge_grow(
    int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, uint32_t phase_ms, uint32_t total_ms) {
    const uint32_t total = phase_total_nonzero(total_ms);
    const uint32_t p = phase_ms > total ? total : phase_ms;
    const int16_t hw = (int16_t)((uint32_t)w * p / total / 2u);
    const int16_t hh = (int16_t)((uint32_t)h * p / total / 2u);
    arena_fill(x, y, hw, 1, color);
    arena_fill(x + w - hw, y, hw, 1, color);
    arena_fill(x, y + h - 1, hw, 1, color);
    arena_fill(x + w - hw, y + h - 1, hw, 1, color);
    arena_fill(x, y, 1, hh, color);
    arena_fill(x, y + h - hh, 1, hh, color);
    arena_fill(x + w - 1, y, 1, hh, color);
    arena_fill(x + w - 1, y + h - hh, 1, hh, color);
}

static void draw_rect_direction_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color,
    uint32_t phase_ms, uint32_t total_ms, Rect_warn_style style) {
    const uint32_t total = phase_total_nonzero(total_ms);
    const uint32_t p = phase_ms > total ? total : phase_ms;
    const int16_t fw = (int16_t)((uint32_t)w * p / total);
    const int16_t fh = (int16_t)((uint32_t)h * p / total);

    if (style == rect_warn_fill_from_left) {
        arena_fill(x, y, fw, h, color);
    } else if (style == rect_warn_fill_from_right) {
        arena_fill(x + w - fw, y, fw, h, color);
    } else if (style == rect_warn_fill_from_top) {
        arena_fill(x, y, w, fh, color);
    } else {
        arena_fill(x, y + h - fh, w, fh, color);
    }
    draw_rect_border(x, y, w, h, color);
}

static void render_attack_warning(uint8_t index, const Attack* a, uint32_t elapsed) {
    const uint32_t warn_ms = attack_warn_ms(a);
    const uint32_t total = phase_total_nonzero(warn_ms);
    const uint16_t wc = warning_color(a->color);
    if (a->type == attack_laser_h || a->type == attack_laser_v) {
        const uint8_t is_h = (a->type == attack_laser_h);
        const int16_t active_pos = resolve_laser_pos(index, a);
        const int16_t active_th = a->u.laser.thickness;
        const int16_t warn_th = warning_thickness(active_th);
        const int16_t warn_pos = centered_warn_pos(active_pos, active_th, warn_th);
        const Laser_warn_style style = a->u.laser.style;
        if (style == laser_warn_dash_flash) {
            /* 在配置的提示时间内闪两次：亮/灭/亮/灭。提示线比激光细。 */
            const uint32_t seg = phase_total_nonzero(total / 4u);
            if (((elapsed / seg) & 1u) == 0u) {
                if (is_h) {
                    draw_hline_dashed(
                        warn_pos, warn_th, wc, (uint8_t)((elapsed / phase_total_nonzero(total / 2u)) & 1u));
                } else {
                    draw_vline_dashed(
                        warn_pos, warn_th, wc, (uint8_t)((elapsed / phase_total_nonzero(total / 2u)) & 1u));
                }
            }

        } else {
            /* 扫描速度由长度 / warn_ms 得出。active 后会重画完整提示线，再由实体激光覆盖。 */
            if (is_h) {
                int16_t len = (int16_t)((uint32_t)ARENA_W * elapsed / total);
                if (len > ARENA_W) { len = ARENA_W; }
                if (style == laser_warn_sweep_rtl) {
                    arena_fill(ARENA_W - len, warn_pos, len, warn_th, wc);
                } else {
                    arena_fill(0, warn_pos, len, warn_th, wc);
                }
            } else {
                int16_t len = (int16_t)((uint32_t)ARENA_H * elapsed / total);
                if (len > ARENA_H) { len = ARENA_H; }
                if (style == laser_warn_sweep_btt) {
                    arena_fill(warn_pos, ARENA_H - len, warn_th, len, wc);
                } else {
                    arena_fill(warn_pos, 0, warn_th, len, wc);
                }
            }
        }
    } else {
        const int16_t x = a->u.rect.x;
        const int16_t y = a->u.rect.y;
        const int16_t w = a->u.rect.w;
        const int16_t h = a->u.rect.h;
        const Rect_warn_style style = a->u.rect.style;
        if (style == rect_warn_center_expand) {
            const int16_t cw = (int16_t)((uint32_t)w * elapsed / total);
            const int16_t ch = (int16_t)((uint32_t)h * elapsed / total);
            draw_rect_border(x, y, w, h, wc);
            arena_fill(x + (w - cw) / 2, y + (h - ch) / 2, cw, ch, wc);
        } else if (style == rect_warn_block_flash) {
            const uint32_t seg = phase_total_nonzero(total / 4u);
            if (((elapsed / seg) & 1u) == 0u) {
                arena_fill(x, y, w, h, wc);
            } else {
                draw_rect_border(x, y, w, h, wc);
            }
        } else if (style == rect_warn_edge_grow) {
            draw_rect_edge_grow(x, y, w, h, wc, elapsed, warn_ms);
        } else {
            draw_rect_direction_fill(x, y, w, h, wc, elapsed, warn_ms, style);
        }
    }
}

static void render_laser_sweep_residue(uint8_t index, const Attack* a) {
    if (!(a->type == attack_laser_h || a->type == attack_laser_v)) { return; }
    if (!laser_is_sweep_style(a->u.laser.style)) { return; }

    const uint16_t wc = warning_color(a->color);
    const int16_t active_th = a->u.laser.thickness;
    const int16_t warn_th = warning_thickness(active_th);
    const int16_t active_pos = resolve_laser_pos(index, a);
    const int16_t warn_pos = centered_warn_pos(active_pos, active_th, warn_th);

    if (a->type == attack_laser_h) {
        arena_fill(0, warn_pos, ARENA_W, warn_th, wc);
    } else {
        arena_fill(warn_pos, 0, warn_th, ARENA_H, wc);
    }
}

static uint32_t laser_sweep_draw_total_ms(const Attack* a) {
    /* sweep 激光的实体“划出”阶段固定 500ms；exist_ms 包含这 500ms。
     * 因此：前 500ms 按路径逐步划出，之后保持完整激光，直到 exist_ms 结束。
     * 若某条激光 exist_ms 配得小于 500ms，则退化为在 exist_ms 内尽量划出。
     */
    const uint32_t exist_ms = phase_total_nonzero(attack_exist_ms(a));
    return (exist_ms < LASER_SWEEP_DRAW_MS) ? exist_ms : LASER_SWEEP_DRAW_MS;
}

static void render_attack_active(uint8_t index, const Attack* a, uint32_t active_ms) {
    if (a->type == attack_laser_h) {
        int16_t len = ARENA_W;
        int16_t x = 0;
        if (a->u.laser.style == laser_warn_sweep_ltr || a->u.laser.style == laser_warn_sweep_rtl) {
            const uint32_t draw_total = laser_sweep_draw_total_ms(a);
            const uint32_t draw_ms = (active_ms > draw_total) ? draw_total : active_ms;
            len = (int16_t)((uint32_t)ARENA_W * draw_ms / draw_total);
            if (len > ARENA_W) { len = ARENA_W; }
            if (a->u.laser.style == laser_warn_sweep_rtl) { x = ARENA_W - len; }
        }
        const int16_t pos = resolve_laser_pos(index, a);
        arena_fill(x, pos, len, a->u.laser.thickness, a->color);
        mark_danger_rect(x, pos, len, a->u.laser.thickness);
    } else if (a->type == attack_laser_v) {
        int16_t len = ARENA_H;
        int16_t y = 0;
        if (a->u.laser.style == laser_warn_sweep_ttb || a->u.laser.style == laser_warn_sweep_btt) {
            const uint32_t draw_total = laser_sweep_draw_total_ms(a);
            const uint32_t draw_ms = (active_ms > draw_total) ? draw_total : active_ms;
            len = (int16_t)((uint32_t)ARENA_H * draw_ms / draw_total);
            if (len > ARENA_H) { len = ARENA_H; }
            if (a->u.laser.style == laser_warn_sweep_btt) { y = ARENA_H - len; }
        }
        const int16_t pos = resolve_laser_pos(index, a);
        arena_fill(pos, y, a->u.laser.thickness, len, a->color);
        mark_danger_rect(pos, y, a->u.laser.thickness, len);
    } else if (a->type == attack_rect) {
        arena_fill(a->u.rect.x, a->u.rect.y, a->u.rect.w, a->u.rect.h, a->color);
        mark_danger_rect(a->u.rect.x, a->u.rect.y, a->u.rect.w, a->u.rect.h);
    }
}

static uint8_t attack_is_finished(const Attack* a, uint8_t index, uint32_t now_rel) {
    if (a->type == attack_bullet) { return g_bullets[index].finished; }
    return (uint8_t)(now_rel >= attack_end_ms(a));
}

static void draw_bullet_spawn_hint(const Attack* a, uint32_t elapsed) {
    /* 子弹提示只在子弹出生的小方块处闪烁，不再闪整条边。
     * 提示色使用子弹颜色的暗色版本。
     */
    const uint32_t warn_ms = attack_warn_ms(a);
    const uint32_t seg = phase_total_nonzero(phase_total_nonzero(warn_ms) / 4u);
    if (((elapsed / seg) & 1u) != 0u) { return; }
    int16_t x, y, w, h;
    bullet_spawn_rect(a, &x, &y, &w, &h);
    const uint16_t wc = warning_color(a->color);
    arena_fill(x - 1, y - 1, w + 2, h + 2, wc);
    if (w > 2 && h > 2) { arena_fill(x, y, w, h, COLOR_BLACK); }
}

static void bullet_spawn(uint8_t index, const Attack* a) {
    Bullet_runtime* b = &g_bullets[index];
    const int16_t size = a->u.bullet.size;
    const int16_t off = a->u.bullet.offset;
    if (a->u.bullet.side == bullet_side_top) {
        b->x256 = (clamp_i16(off, 0, ARENA_W - size)) << 8;
        b->y256 = 1 << 8;
    } else if (a->u.bullet.side == bullet_side_bottom) {
        b->x256 = (clamp_i16(off, 0, ARENA_W - size)) << 8;
        b->y256 = (ARENA_H - size - 1) << 8;
    } else if (a->u.bullet.side == bullet_side_left) {
        b->x256 = 1 << 8;
        b->y256 = (clamp_i16(off, 0, ARENA_H - size)) << 8;
    } else {
        b->x256 = (ARENA_W - size - 1) << 8;
        b->y256 = (clamp_i16(off, 0, ARENA_H - size)) << 8;
    }
    b->prev_x256 = b->x256;
    b->prev_y256 = b->y256;

    const int32_t player_cx = g_player_x256 + (PLAYER_SIZE << 7);
    const int32_t player_cy = g_player_y256 + (PLAYER_SIZE << 7);
    const int32_t bullet_cx = b->x256 + (size << 7);
    const int32_t bullet_cy = b->y256 + (size << 7);
    normalize_to_q8((player_cx - bullet_cx) >> 8, (player_cy - bullet_cy) >> 8, &b->dir_x, &b->dir_y);
    b->initialized = 1;
    b->active = 1;
    b->finished = 0;
}

static void render_and_update_bullet(uint8_t index, const Attack* a, uint32_t dt_ms) {
    Bullet_runtime* b = &g_bullets[index];
    const int16_t size = a->u.bullet.size;
    if (!b->initialized) { bullet_spawn(index, a); }
    if (!b->active || b->finished) { return; }

    b->prev_x256 = b->x256;
    b->prev_y256 = b->y256;

    const int32_t player_cx = g_player_x256 + (PLAYER_SIZE << 7);
    const int32_t player_cy = g_player_y256 + (PLAYER_SIZE << 7);
    const int32_t bullet_cx = b->x256 + (size << 7);
    const int32_t bullet_cy = b->y256 + (size << 7);
    int16_t target_x;
    int16_t target_y;
    normalize_to_q8((player_cx - bullet_cx) >> 8, (player_cy - bullet_cy) >> 8, &target_x, &target_y);

    const uint16_t alpha = a->u.bullet.alpha;
    int32_t new_x = ((int32_t)alpha * b->dir_x + (int32_t)(255u - alpha) * target_x) / 255;
    int32_t new_y = ((int32_t)alpha * b->dir_y + (int32_t)(255u - alpha) * target_y) / 255;
    normalize_to_q8(new_x, new_y, &b->dir_x, &b->dir_y);

    const int32_t step256 = (int32_t)a->u.bullet.speed_px_s * 256 * (int32_t)dt_ms / 1000;
    b->x256 += step256 * b->dir_x / BULLET_DIR_SCALE;
    b->y256 += step256 * b->dir_y / BULLET_DIR_SCALE;

    const int16_t x = (int16_t)(b->x256 >> 8);
    const int16_t y = (int16_t)(b->y256 >> 8);
    if (x <= 0 || y <= 0 || x + size >= ARENA_W || y + size >= ARENA_H) {
        b->active = 0;
        b->finished = 1;
        return;
    }

    /* 子弹最后一层攻击物渲染：白色包边 + 内部攻击色，避免融入其他攻击。 */
    arena_fill(x - 1, y - 1, size + 2, size + 2, COLOR_WHITE);
    arena_fill(x, y, size, size, a->color);
    mark_danger_rect(x - 1, y - 1, size + 2, size + 2);
}

static void clear_dynamic_regions(uint32_t now_rel) {
    (void)now_rel;
    /* 回退为全局 arena 刷新：每帧清空战斗区域并重绘当前状态。
     * 这样避免 dirty rect 在攻击互相覆盖、子弹穿过区域时产生拖影。
     * 只刷新 arena，不重绘标题/状态栏。
     */
    arena_fill(0, 0, ARENA_W, ARENA_H, COLOR_BLACK);
    arena_border();
}

static void render_attacks_and_map(uint32_t now_rel, uint32_t dt_ms) {
    memset(g_danger_map, 0, sizeof(g_danger_map));

    /* 第一层：激光/区域提示与实体攻击。 */
    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (a->type == attack_bullet) { continue; }
        if (now_rel < a->start_ms) { continue; }

        const uint32_t elapsed = now_rel - a->start_ms;
        const uint32_t warn_ms = attack_warn_ms(a);
        const uint32_t exist_ms = attack_exist_ms(a);

        if (elapsed < warn_ms) {
            render_attack_warning(i, a, elapsed);
        } else if (elapsed < warn_ms + exist_ms) {
            /* 全局刷新会擦掉上一帧，所以 sweep 激光进入 active 后，
             * 每帧先重画完整暗色提示线，再画实体激光，让实体激光自然覆盖提示线。
             */
            render_laser_sweep_residue(i, a);
            render_attack_active(i, a, elapsed - warn_ms);
        }
    }

    /* 第二层：子弹提示和子弹本体。这样子弹不会被其它攻击覆盖；玩家稍后最后渲染。 */
    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (a->type != attack_bullet) { continue; }
        if (now_rel < a->start_ms) { continue; }

        const uint32_t elapsed = now_rel - a->start_ms;
        const uint32_t warn_ms = attack_warn_ms(a);

        if (elapsed < warn_ms) {
            draw_bullet_spawn_hint(a, elapsed);
        } else {
            render_and_update_bullet(i, a, dt_ms);
        }
    }
}

static void update_attack_finish_scan(uint32_t now_rel) {
    /*
     * 只从当前扫描索引向后检查。
     * 激光/区域以 start_ms + warn_ms + exist_ms 为完成点；子弹撞到边框消失后才算完成。
     */
    while (g_attack_scan_index < ATTACK_COUNT) {
        const Attack* a = &g_level[g_attack_scan_index];
        if (!attack_is_finished(a, g_attack_scan_index, now_rel)) { break; }
        g_attack_scan_index++;
    }

    if (!g_all_attacks_done && g_attack_scan_index >= ATTACK_COUNT) {
        g_all_attacks_done = 1;
        g_all_attacks_done_ms = now_rel;
    }
}

static void render_static_screen(void) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_text_centered(14, "DODGE BOX", 2, COLOR_CYAN);
    Game_Graphics_Draw_Text(g_lcd, 24, 42, "MOVE: JOYSTICK", 1, COLOR_GRAY);
    Game_Graphics_Draw_Text(g_lcd, 144, 42, "HOLD: BACK", 1, COLOR_GRAY);
    arena_fill(0, 0, ARENA_W, ARENA_H, COLOR_BLACK);
    arena_border();
}

static void draw_status(uint32_t now_rel) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 253, SCREEN_WIDTH, 35, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_lcd, 60, 257, "TIME", 1, COLOR_GRAY);
    Game_Graphics_Draw_U32(g_lcd, 96, 257, now_rel / 1000u, 2, 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 112, 257, "s", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 64, 274, "SURVIVE TO CLEAR", 1, COLOR_DARK);
}

static void reset_game(void) {
    g_state = game_state_playing;
    g_finished = 0;
    g_attack_scan_index = 0;
    g_all_attacks_done = 0;
    g_all_attacks_done_ms = 0;
    g_start_ms = Bsp_Get_Tick_Ms();
    g_rng_state = 0x4d595df4u ^ g_start_ms;
    g_last_ms = g_start_ms;
    g_survive_ms = 0;
    g_player_x256 = ((ARENA_W - PLAYER_SIZE) / 2) << 8;
    g_player_y256 = ((ARENA_H - PLAYER_SIZE) / 2) << 8;
    g_prev_player_x = (int16_t)(g_player_x256 >> 8);
    g_prev_player_y = (int16_t)(g_player_y256 >> 8);
    memset(g_danger_map, 0, sizeof(g_danger_map));
    memset(g_bullets, 0, sizeof(g_bullets));
    memset(g_laser_track_pos, 0, sizeof(g_laser_track_pos));
    memset(g_laser_track_pos_valid, 0, sizeof(g_laser_track_pos_valid));
    render_static_screen();
    draw_status(0);
    draw_player(COLOR_WHITE);
}

void Dodge_Box_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    reset_game();
}

static void move_player(const Game_input* input, uint32_t dt_ms) {
    int16_t dx = 0;
    int16_t dy = 0;
    if (input->direction == game_direction_left) {
        dx = -1;
    } else if (input->direction == game_direction_right) {
        dx = 1;
    } else if (input->direction == game_direction_up) {
        dy = -1;
    } else if (input->direction == game_direction_down) {
        dy = 1;
    }

    if (dx == 0 && dy == 0) { return; }

    const int32_t step256 = (int32_t)PLAYER_SPEED_PX_S * 256 * (int32_t)dt_ms / 1000;
    g_player_x256 += (int32_t)dx * step256;
    g_player_y256 += (int32_t)dy * step256;

    if (g_player_x256 < 0) { g_player_x256 = 0; }
    if (g_player_y256 < 0) { g_player_y256 = 0; }
    if (g_player_x256 > (ARENA_W - PLAYER_SIZE) * 256) { g_player_x256 = (ARENA_W - PLAYER_SIZE) * 256; }
    if (g_player_y256 > (ARENA_H - PLAYER_SIZE) * 256) { g_player_y256 = (ARENA_H - PLAYER_SIZE) * 256; }
}

Game_result Dodge_Box_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state == game_state_failed || g_state == game_state_clear) {
        if (input->confirm_pressed || input->direction_pressed) { reset_game(); }
        return game_result_running;
    }

    const uint32_t now = Bsp_Get_Tick_Ms();
    uint32_t dt_ms = now - g_last_ms;
    if (dt_ms > MAX_DT_MS) { dt_ms = MAX_DT_MS; }
    g_last_ms = now;

    const uint32_t now_rel = now - g_start_ms;
    g_prev_player_x = (int16_t)(g_player_x256 >> 8);
    g_prev_player_y = (int16_t)(g_player_y256 >> 8);

    move_player(input, dt_ms);
    clear_dynamic_regions(now_rel);
    render_attacks_and_map(now_rel, dt_ms);
    update_attack_finish_scan(now_rel);

    if (player_hits_danger()) {
        g_state = game_state_failed;
        g_finished = 1;
        draw_player(COLOR_RED);
        Game_Graphics_Fill_Rect(g_lcd, 0, 253, SCREEN_WIDTH, 35, COLOR_BLACK);
        draw_text_centered(258, "FAILED - PRESS", 1, COLOR_RED);
        return game_result_running;
    }

    draw_player(COLOR_WHITE);

    if ((now_rel / 1000u) != (g_survive_ms / 1000u)) { draw_status(now_rel); }
    g_survive_ms = now_rel;

    if (g_all_attacks_done && (now_rel - g_all_attacks_done_ms >= 2000u)) {
        g_state = game_state_clear;
        g_finished = 1;
        Game_Graphics_Fill_Rect(g_lcd, 0, 253, SCREEN_WIDTH, 35, COLOR_BLACK);
        draw_text_centered(258, "CLEAR - PRESS", 1, COLOR_GREEN);
    }

    return game_result_running;
}

uint32_t Dodge_Box_Get_Score(void) { return g_survive_ms / 100u; }

uint8_t Dodge_Box_Is_Finished(void) { return g_finished; }