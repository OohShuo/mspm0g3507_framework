#include "dodge_box.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH           240
#define SCREEN_HEIGHT          320

/* 你要求的实际战斗地图：120 x 180。这里放在 240 x 320 屏幕中间偏下。 */
#define ARENA_W                120
#define ARENA_H                180
#define ARENA_X                ((SCREEN_WIDTH - ARENA_W) / 2)
#define ARENA_Y                (GAME_TOP_BAR_H + (GAME_AREA_H - ARENA_H) / 2)

#define PLAYER_SIZE            5
#define PLAYER_SPEED_PX_S      50u
#define DEFAULT_WARNING_MS     500u
#define DEFAULT_EXIST_MS       500u
#define LASER_SWEEP_DRAW_MS    500u
#define LASER_POS_TRACK_PLAYER 255u
#define LASER_TRACK_JITTER_PX  5
#define ATTACK_FADE_MS         100u
// #define GOD_MODE

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

static uint16_t light_color(uint16_t c) {
    uint16_t r = (c >> 11) & 0x1F;
    uint16_t g = (c >> 5) & 0x3F;
    uint16_t b = c & 0x1F;
    r = (uint16_t)(r + (31u - r) / 2u);
    g = (uint16_t)(g + (63u - g) / 2u);
    b = (uint16_t)(b + (31u - b) / 2u);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

#define MAX_DT_MS            50u
#define BULLET_MAX_COUNT     24u
#define BULLET_DIR_SCALE     256l
#define BULLET_DIR_UPDATE_MS 20u

#define ATTACK_COUNT         ((uint8_t)(sizeof(g_level) / sizeof(g_level[0])))

#define COLOR_BLACK          0x0000u
#define COLOR_WHITE          0xffffu
#define COLOR_RED            0xf800u
#define COLOR_GREEN          0x07e0u
#define COLOR_BLUE           0x001fu
#define COLOR_CYAN           0x07ffu
#define COLOR_YELLOW         0xffe0u
#define COLOR_MAGENTA        0xf81fu
#define COLOR_GRAY           0x8410u
#define COLOR_DARK           0xA514u
#define COLOR_DIM_RED        0x6000u
#define COLOR_DIM_CYAN       0x0339u
#define COLOR_DIM_YELLOW     0x6300u

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
    uint8_t just_finished;
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

#define DIRTY_RECT_MAX 32u

typedef struct {
    Rect rects[DIRTY_RECT_MAX];
    uint8_t count;
} Rect_cache;

typedef enum {
    game_state_ready,
    game_state_playing,
    game_state_failed,
    game_state_clear,
} Dodge_state;

static St7789* g_lcd = NULL;
static Vib_motor* g_vib_motor = NULL;
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
static uint32_t g_bullet_dir_accum = 0;

static Rect_cache g_final_dirty_cache;
static uint8_t g_clip_enabled = 0;
static Rect g_clip_rect = {0, 0, ARENA_W, ARENA_H};

// clang-format off
static const Attack g_level[] = {
    // phase 1
    {500u, 1500u, 0u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 0u, 120u, 180u, rect_warn_block_flash}}},

    {2700u, 1000u, 300u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {90u, 2u, laser_warn_dash_flash}}},
    {2700u, 1000u, 300u, attack_laser_v, COLOR_DIM_CYAN, {.laser = {60u, 2u, laser_warn_dash_flash}}},

    {4500u, 1000u, 300u, attack_rect, COLOR_DIM_CYAN, {.rect = {40u, 60u, 40u, 60u, rect_warn_center_expand}}},

    {6000u, 700u, 6000u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 0u, 120u, 60u, rect_warn_fill_from_top}}},
    {6000u, 700u, 6000u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 0u, 30u, 180u, rect_warn_fill_from_left}}},
    {6000u, 700u, 6000u, attack_rect, COLOR_DIM_CYAN, {.rect = {90u, 0u, 30u, 180u, rect_warn_fill_from_right}}},
    {6000u, 700u, 6000u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 120u, 120u, 60u, rect_warn_fill_from_bottom}}},

    {6700u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 30u, 4u, 50u, 250u}}},
    {7000u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 40u, 4u, 50u, 250u}}},
    {7300u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 50u, 4u, 50u, 250u}}},
    {7600u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 60u, 4u, 50u, 250u}}},

    {9100u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 30u, 4u, 80u, 250u}}},
    {9400u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 40u, 4u, 80u, 250u}}},
    {9700u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 50u, 4u, 80u, 250u}}},
    {10000u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 60u, 4u, 80u, 250u}}},

    {12500u, 500u, 4400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {90u, 3u, laser_warn_sweep_ltr}}},
    {12800u, 500u, 3900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {80u, 3u, laser_warn_sweep_rtl}}},
    {12800u, 500u, 3900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {100u, 3u, laser_warn_sweep_rtl}}},
    {13100u, 500u, 3400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {70u, 3u, laser_warn_sweep_ltr}}},
    {13100u, 500u, 3400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {110u, 3u, laser_warn_sweep_ltr}}},
    {13400u, 500u, 2900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {60u, 3u, laser_warn_sweep_rtl}}},
    {13400u, 500u, 2900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {120u, 3u, laser_warn_sweep_rtl}}},
    {13700u, 500u, 2400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {50u, 3u, laser_warn_sweep_ltr}}},
    {13700u, 500u, 2400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {130u, 3u, laser_warn_sweep_ltr}}},
    {14000u, 500u, 1900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {40u, 3u, laser_warn_sweep_rtl}}},
    {14000u, 500u, 1900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {140u, 3u, laser_warn_sweep_rtl}}},
    {14300u, 500u, 1400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {30u, 3u, laser_warn_sweep_ltr}}},
    {14300u, 500u, 1400u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {150u, 3u, laser_warn_sweep_ltr}}},
    {14600u, 500u, 900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {20u, 3u, laser_warn_sweep_rtl}}},
    {14600u, 500u, 900u, attack_laser_h, COLOR_DIM_CYAN, {.laser = {160u, 3u, laser_warn_sweep_rtl}}},

    {15000u, 1000u, 500u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 0u, 50u, 15u, rect_warn_fill_from_left}}},
    {15000u, 1000u, 500u, attack_rect, COLOR_DIM_CYAN, {.rect = {70u, 0u, 50u, 15u, rect_warn_fill_from_right}}},
    {15000u, 1000u, 500u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 165u, 50u, 15u, rect_warn_fill_from_left}}},
    {15000u, 1000u, 500u, attack_rect, COLOR_DIM_CYAN, {.rect = {70u, 165u, 50u, 15u, rect_warn_fill_from_right}}},

    {16000u, 2500u, 1000u, attack_rect, COLOR_DIM_CYAN, {.rect = {55u, 0u, 10u, 80u, rect_warn_fill_from_top}}},
    {16000u, 2500u, 1000u, attack_rect, COLOR_DIM_CYAN, {.rect = {55u, 100u, 10u, 80u, rect_warn_fill_from_bottom}}},
    {16000u, 2500u, 1000u, attack_rect, COLOR_DIM_CYAN, {.rect = {0u, 85u, 50u, 10u, rect_warn_fill_from_left}}},
    {16000u, 2500u, 1000u, attack_rect, COLOR_DIM_CYAN, {.rect = {70u, 85u, 50u, 10u, rect_warn_fill_from_right}}},

    {18500u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 2u, 4u, 80u, 255u}}},
    {18500u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_top, 112u, 4u, 80u, 255u}}},
    {18500u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 2u, 4u, 80u, 255u}}},
    {18500u, 500u, 0u, attack_bullet, COLOR_DIM_CYAN, {.bullet = {bullet_side_bottom, 112u, 4u, 80u, 255u}}},

    // phase 2
    {20000u, 1500u, 0u, attack_rect, COLOR_DIM_YELLOW, {.rect = {0u, 0u, 120u, 180u, rect_warn_block_flash}}},

    {22000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {22500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {23000u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {23500u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {24000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {24500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {25000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {25500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {26000u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {26500u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {27000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {27500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {28000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {28500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {29000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {29500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {30000u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {30500u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {31000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {31500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {32000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {32500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {33000u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {33500u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {34000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {34500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {35000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {35500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {36000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {36500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {37000u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {37500u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {38000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {38500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {39000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {39500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},
    {40000u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ttb}}},
    {40500u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_rtl}}},
    {41000u, 500u, 1000u, attack_laser_h, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_ltr}}},
    {41500u, 500u, 1000u, attack_laser_v, COLOR_DIM_YELLOW, {.laser = {LASER_POS_TRACK_PLAYER, 2u, laser_warn_sweep_btt}}},

    // phase 3
    {45000u, 1500u, 0u, attack_rect, COLOR_RED, {.rect = {0u, 0u, 120u, 180u, rect_warn_block_flash}}},

    {46500u, 1000u, 300u, attack_laser_v, COLOR_RED, {.laser = {30u, 2u, laser_warn_dash_flash}}},
    {46500u, 1000u, 300u, attack_laser_v, COLOR_RED, {.laser = {60u, 2u, laser_warn_dash_flash}}},
    {46500u, 1000u, 300u, attack_laser_v, COLOR_RED, {.laser = {90u, 2u, laser_warn_dash_flash}}},
    {46500u, 1000u, 300u, attack_laser_h, COLOR_RED, {.laser = {30u, 2u, laser_warn_dash_flash}}},
    {46500u, 1000u, 300u, attack_laser_h, COLOR_RED, {.laser = {60u, 2u, laser_warn_dash_flash}}},
    {46500u, 1000u, 300u, attack_laser_h, COLOR_RED, {.laser = {90u, 2u, laser_warn_dash_flash}}},
    {46500u, 1000u, 300u, attack_laser_h, COLOR_RED, {.laser = {120u, 2u, laser_warn_dash_flash}}},
    {46500u, 1000u, 300u, attack_laser_h, COLOR_RED, {.laser = {150u, 2u, laser_warn_dash_flash}}},
    
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {0u, 0u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {48u, 0u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {96u, 0u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {24u, 40u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {72u, 40u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {0u, 80u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {48u, 80u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {96u, 80u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {24u, 120u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {72u, 120u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {0u, 160u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {48u, 160u, 24u, 20u, rect_warn_edge_grow}}},
    {48000u, 1000u, 25000u, attack_rect, COLOR_RED, {.rect = {96u, 160u, 24u, 20u, rect_warn_edge_grow}}},

    {49000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_left, 45u, 4u, 40u, 250u}}},
    {50000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_top, 60u, 4u, 60u, 254u}}},
    {51000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_right, 135u, 4u, 60u, 254u}}},
    {52000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_top, 30u, 4u, 40u, 250u}}},
    {53000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_bottom, 90u, 4u, 60u, 254u}}},
    {54000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_top, 90u, 4u, 40u, 250u}}},
    {55000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_left, 90u, 4u, 60u, 254u}}},
    {56000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_bottom, 60u, 4u, 40u, 250u}}},
    {57000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_right, 45u, 4u, 60u, 254u}}},
    {58000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_left, 135u, 4u, 40u, 250u}}},
    {59000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_right, 90u, 4u, 40u, 250u}}},
    {60000u, 500u, 0u, attack_bullet, COLOR_RED, {.bullet = {bullet_side_bottom, 30u, 4u, 60u, 254u}}},
    
};
// clang-format on

static uint8_t g_laser_track_pos[ATTACK_COUNT];
static uint8_t g_laser_track_pos_valid[ATTACK_COUNT];
static uint32_t g_rng_state = 0x4d595df4u;

static uint8_t bullet_slot_from_attack_index(uint8_t attack_index) {
    uint8_t slot = 0;
    for (uint8_t i = 0; i < attack_index && i < ATTACK_COUNT; i++) {
        if (g_level[i].type == attack_bullet) { slot++; }
    }
    return slot;
}

static Bullet_runtime* bullet_runtime_for_attack(uint8_t attack_index) {
    const uint8_t slot = bullet_slot_from_attack_index(attack_index);
    if (slot >= BULLET_MAX_COUNT) { return NULL; }
    return &g_bullets[slot];
}

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

static uint32_t rng_next(void) {
    g_rng_state = g_rng_state * 1664525u + 1013904223u;
    return g_rng_state;
}

static int16_t rng_jitter_pm5(void) {
    return (
        int16_t)((int32_t)(rng_next() % (uint32_t)(LASER_TRACK_JITTER_PX * 2 + 1)) - LASER_TRACK_JITTER_PX);
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
            base = (int16_t)((g_player_y256 >> 8) + PLAYER_SIZE / 2 - th / 2);
            max_pos = ARENA_H - th;
        } else {
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

static uint16_t warning_color(uint16_t color) { return dim_color(color); }

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

static uint16_t active_attack_color(const Attack* a, uint32_t active_ms) {
    const uint32_t exist_ms = attack_exist_ms(a);
    if (exist_ms > ATTACK_FADE_MS && active_ms >= exist_ms - ATTACK_FADE_MS) { return light_color(a->color); }
    return a->color;
}

static uint32_t attack_end_ms(const Attack* a) {
    return a->start_ms + attack_warn_ms(a) + attack_exist_ms(a);
}

static int16_t warning_thickness(int16_t active_thickness) {
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

    if (g_clip_enabled) {
        const int16_t cx0 = g_clip_rect.x;
        const int16_t cy0 = g_clip_rect.y;
        const int16_t cx1 = (int16_t)(g_clip_rect.x + g_clip_rect.w);
        const int16_t cy1 = (int16_t)(g_clip_rect.y + g_clip_rect.h);
        int16_t x0 = x;
        int16_t y0 = y;
        int16_t x1 = (int16_t)(x + w);
        int16_t y1 = (int16_t)(y + h);

        if (x0 < cx0) { x0 = cx0; }
        if (y0 < cy0) { y0 = cy0; }
        if (x1 > cx1) { x1 = cx1; }
        if (y1 > cy1) { y1 = cy1; }
        if (x1 <= x0 || y1 <= y0) { return; }
        x = x0;
        y = y0;
        w = (int16_t)(x1 - x0);
        h = (int16_t)(y1 - y0);
    }

    Game_Graphics_Fill_Rect(g_lcd, ARENA_X + x, ARENA_Y + y, w, h, color);
}

static void arena_border(void) {
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X - 2, ARENA_Y - 2, ARENA_W + 4, 2, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X - 2, ARENA_Y + ARENA_H, ARENA_W + 4, 2, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X - 2, ARENA_Y - 2, 2, ARENA_H + 4, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X + ARENA_W, ARENA_Y - 2, 2, ARENA_H + 4, COLOR_WHITE);
}

static int16_t rect_right(const Rect* r) { return (int16_t)(r->x + r->w); }
static int16_t rect_bottom(const Rect* r) { return (int16_t)(r->y + r->h); }

static uint8_t rect_intersects_or_touches(const Rect* a, const Rect* b) {
    return (uint8_t)(a->x <= rect_right(b) && b->x <= rect_right(a) && a->y <= rect_bottom(b) &&
                     b->y <= rect_bottom(a));
}

static Rect rect_union_make(const Rect* a, const Rect* b) {
    Rect r;
    const int16_t x0 = (a->x < b->x) ? a->x : b->x;
    const int16_t y0 = (a->y < b->y) ? a->y : b->y;
    const int16_t x1 = (rect_right(a) > rect_right(b)) ? rect_right(a) : rect_right(b);
    const int16_t y1 = (rect_bottom(a) > rect_bottom(b)) ? rect_bottom(a) : rect_bottom(b);
    r.x = x0;
    r.y = y0;
    r.w = (int16_t)(x1 - x0);
    r.h = (int16_t)(y1 - y0);
    return r;
}

static uint8_t rect_merge_is_worthwhile(const Rect* a, const Rect* b, const Rect* u) {
    const uint32_t area_a = (uint32_t)a->w * (uint32_t)a->h;
    const uint32_t area_b = (uint32_t)b->w * (uint32_t)b->h;
    const uint32_t area_u = (uint32_t)u->w * (uint32_t)u->h;
    return (uint8_t)(area_u * 4u <= (area_a + area_b) * 5u);
}

static uint8_t rect_clip_to_arena(Rect* r) {
    int16_t x0 = r->x;
    int16_t y0 = r->y;
    int16_t x1 = (int16_t)(r->x + r->w);
    int16_t y1 = (int16_t)(r->y + r->h);

    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }
    if (x1 > ARENA_W) { x1 = ARENA_W; }
    if (y1 > ARENA_H) { y1 = ARENA_H; }
    if (x1 <= x0 || y1 <= y0) { return 0; }

    r->x = x0;
    r->y = y0;
    r->w = (int16_t)(x1 - x0);
    r->h = (int16_t)(y1 - y0);
    return 1;
}

static void dirty_cache_clear(Rect_cache* cache) { cache->count = 0; }

static void dirty_cache_add_rect(Rect_cache* cache, Rect r) {
    if (!rect_clip_to_arena(&r)) { return; }

    for (uint8_t i = 0; i < cache->count; i++) {
        if (rect_intersects_or_touches(&cache->rects[i], &r)) {
            Rect u = rect_union_make(&cache->rects[i], &r);
            if (!rect_merge_is_worthwhile(&cache->rects[i], &r, &u)) { continue; }
            cache->rects[i] = u;

            uint8_t j = (uint8_t)(i + 1u);
            while (j < cache->count) {
                if (rect_intersects_or_touches(&cache->rects[i], &cache->rects[j])) {
                    Rect uj = rect_union_make(&cache->rects[i], &cache->rects[j]);
                    if (rect_merge_is_worthwhile(&cache->rects[i], &cache->rects[j], &uj)) {
                        cache->rects[i] = uj;
                        cache->rects[j] = cache->rects[cache->count - 1u];
                        cache->count--;
                    } else {
                        j++;
                    }
                } else {
                    j++;
                }
            }
            return;
        }
    }

    if (cache->count < DIRTY_RECT_MAX) {
        cache->rects[cache->count++] = r;
    } else {
        cache->count = 1;
        cache->rects[0].x = 0;
        cache->rects[0].y = 0;
        cache->rects[0].w = ARENA_W;
        cache->rects[0].h = ARENA_H;
    }
}

static void dirty_cache_add_xywh(Rect_cache* cache, int16_t x, int16_t y, int16_t w, int16_t h) {
    Rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    dirty_cache_add_rect(cache, r);
}

static void dirty_all_layers_clear(void) { dirty_cache_clear(&g_final_dirty_cache); }

static uint8_t rect_overlaps_strict(const Rect* a, const Rect* b) {
    return (uint8_t)(a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h);
}

static void player_rect(Rect* out) {
    out->x = (int16_t)(g_player_x256 >> 8);
    out->y = (int16_t)(g_player_y256 >> 8);
    out->w = PLAYER_SIZE;
    out->h = PLAYER_SIZE;
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
    const uint32_t exist_ms = phase_total_nonzero(attack_exist_ms(a));
    return (exist_ms < LASER_SWEEP_DRAW_MS) ? exist_ms : LASER_SWEEP_DRAW_MS;
}

static void render_attack_active(uint8_t index, const Attack* a, uint32_t active_ms) {
    const uint16_t color = active_attack_color(a, active_ms);

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
        arena_fill(x, pos, len, a->u.laser.thickness, color);
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
        arena_fill(pos, y, a->u.laser.thickness, len, color);
    } else if (a->type == attack_rect) {
        arena_fill(a->u.rect.x, a->u.rect.y, a->u.rect.w, a->u.rect.h, color);
    }
}

static void attack_active_rect(uint8_t index, const Attack* a, uint32_t active_ms, Rect* out) {
    out->x = 0;
    out->y = 0;
    out->w = 0;
    out->h = 0;

    if (a->type == attack_laser_h) {
        int16_t len = ARENA_W;
        int16_t x = 0;
        if (a->u.laser.style == laser_warn_sweep_ltr || a->u.laser.style == laser_warn_sweep_rtl) {
            const uint32_t draw_total = laser_sweep_draw_total_ms(a);
            const uint32_t draw_ms = (active_ms > draw_total) ? draw_total : active_ms;
            len = (int16_t)((uint32_t)ARENA_W * draw_ms / draw_total);
            if (len > ARENA_W) { len = ARENA_W; }
            if (a->u.laser.style == laser_warn_sweep_rtl) { x = (int16_t)(ARENA_W - len); }
        }
        out->x = x;
        out->y = resolve_laser_pos(index, a);
        out->w = len;
        out->h = a->u.laser.thickness;
    } else if (a->type == attack_laser_v) {
        int16_t len = ARENA_H;
        int16_t y = 0;
        if (a->u.laser.style == laser_warn_sweep_ttb || a->u.laser.style == laser_warn_sweep_btt) {
            const uint32_t draw_total = laser_sweep_draw_total_ms(a);
            const uint32_t draw_ms = (active_ms > draw_total) ? draw_total : active_ms;
            len = (int16_t)((uint32_t)ARENA_H * draw_ms / draw_total);
            if (len > ARENA_H) { len = ARENA_H; }
            if (a->u.laser.style == laser_warn_sweep_btt) { y = (int16_t)(ARENA_H - len); }
        }
        out->x = resolve_laser_pos(index, a);
        out->y = y;
        out->w = a->u.laser.thickness;
        out->h = len;
    } else if (a->type == attack_rect) {
        out->x = a->u.rect.x;
        out->y = a->u.rect.y;
        out->w = a->u.rect.w;
        out->h = a->u.rect.h;
    }
}

static uint8_t attack_draw_bounds_at(uint8_t index, const Attack* a, uint32_t now_rel, Rect* out) {
    out->x = 0;
    out->y = 0;
    out->w = 0;
    out->h = 0;

    if (a->type == attack_bullet || now_rel < a->start_ms) { return 0; }

    const uint32_t elapsed = now_rel - a->start_ms;
    const uint32_t warn_ms = attack_warn_ms(a);
    const uint32_t exist_ms = attack_exist_ms(a);

    if (elapsed < warn_ms) {
        if (a->type == attack_rect) {
            out->x = a->u.rect.x;
            out->y = a->u.rect.y;
            out->w = a->u.rect.w;
            out->h = a->u.rect.h;
            return 1;
        }

        const int16_t active_pos = resolve_laser_pos(index, a);
        const int16_t active_th = a->u.laser.thickness;
        const int16_t warn_th = warning_thickness(active_th);
        const int16_t warn_pos = centered_warn_pos(active_pos, active_th, warn_th);

        if (a->type == attack_laser_h) {
            out->x = 0;
            out->y = warn_pos;
            out->w = ARENA_W;
            out->h = warn_th;
        } else {
            out->x = warn_pos;
            out->y = 0;
            out->w = warn_th;
            out->h = ARENA_H;
        }
        return 1;
    }

    if (elapsed < warn_ms + exist_ms) {
        if (a->type == attack_laser_h || a->type == attack_laser_v) {
            const int16_t active_pos = resolve_laser_pos(index, a);
            const int16_t active_th = a->u.laser.thickness;
            if (a->type == attack_laser_h) {
                out->x = 0;
                out->y = active_pos;
                out->w = ARENA_W;
                out->h = active_th;
            } else {
                out->x = active_pos;
                out->y = 0;
                out->w = active_th;
                out->h = ARENA_H;
            }
            return 1;
        }

        out->x = a->u.rect.x;
        out->y = a->u.rect.y;
        out->w = a->u.rect.w;
        out->h = a->u.rect.h;
        return 1;
    }

    return 0;
}

static void dirty_cache_add_border(Rect_cache* cache, int16_t x, int16_t y, int16_t w, int16_t h) {
    dirty_cache_add_xywh(cache, x, y, w, 1);
    dirty_cache_add_xywh(cache, x, (int16_t)(y + h - 1), w, 1);
    dirty_cache_add_xywh(cache, x, y, 1, h);
    dirty_cache_add_xywh(cache, (int16_t)(x + w - 1), y, 1, h);
}

static uint8_t rect_contains_rect(const Rect* outer, const Rect* inner) {
    return (uint8_t)(inner->x >= outer->x && inner->y >= outer->y && rect_right(inner) <= rect_right(outer) &&
                     rect_bottom(inner) <= rect_bottom(outer));
}

static void dirty_cache_add_rect_growth(Rect_cache* cache, const Rect* old_r, const Rect* new_r) {
    if (new_r->w <= 0 || new_r->h <= 0) { return; }
    if (old_r->w <= 0 || old_r->h <= 0 || !rect_contains_rect(new_r, old_r)) {
        dirty_cache_add_rect(cache, *new_r);
        return;
    }

    const int16_t nx0 = new_r->x;
    const int16_t ny0 = new_r->y;
    const int16_t nx1 = rect_right(new_r);
    const int16_t ny1 = rect_bottom(new_r);
    const int16_t ox0 = old_r->x;
    const int16_t oy0 = old_r->y;
    const int16_t ox1 = rect_right(old_r);
    const int16_t oy1 = rect_bottom(old_r);

    if (oy0 > ny0) { dirty_cache_add_xywh(cache, nx0, ny0, new_r->w, (int16_t)(oy0 - ny0)); }
    if (ny1 > oy1) { dirty_cache_add_xywh(cache, nx0, oy1, new_r->w, (int16_t)(ny1 - oy1)); }

    if (ox0 > nx0 && oy1 > oy0) {
        dirty_cache_add_xywh(cache, nx0, oy0, (int16_t)(ox0 - nx0), (int16_t)(oy1 - oy0));
    }
    if (nx1 > ox1 && oy1 > oy0) {
        dirty_cache_add_xywh(cache, ox1, oy0, (int16_t)(nx1 - ox1), (int16_t)(oy1 - oy0));
    }
}

static void rect_warning_fill_rect_at(const Attack* a, uint32_t elapsed, Rect* out) {
    const uint32_t total = phase_total_nonzero(attack_warn_ms(a));
    const uint32_t p = (elapsed > total) ? total : elapsed;
    const int16_t x = a->u.rect.x;
    const int16_t y = a->u.rect.y;
    const int16_t w = a->u.rect.w;
    const int16_t h = a->u.rect.h;

    out->x = x;
    out->y = y;
    out->w = 0;
    out->h = 0;

    if (a->u.rect.style == rect_warn_center_expand) {
        const int16_t cw = (int16_t)((uint32_t)w * p / total);
        const int16_t ch = (int16_t)((uint32_t)h * p / total);
        out->x = (int16_t)(x + (w - cw) / 2);
        out->y = (int16_t)(y + (h - ch) / 2);
        out->w = cw;
        out->h = ch;
    } else if (a->u.rect.style == rect_warn_fill_from_left) {
        out->w = (int16_t)((uint32_t)w * p / total);
        out->h = h;
    } else if (a->u.rect.style == rect_warn_fill_from_right) {
        out->w = (int16_t)((uint32_t)w * p / total);
        out->h = h;
        out->x = (int16_t)(x + w - out->w);
    } else if (a->u.rect.style == rect_warn_fill_from_top) {
        out->w = w;
        out->h = (int16_t)((uint32_t)h * p / total);
    } else if (a->u.rect.style == rect_warn_fill_from_bottom) {
        out->w = w;
        out->h = (int16_t)((uint32_t)h * p / total);
        out->y = (int16_t)(y + h - out->h);
    }
}

static uint8_t rect_block_flash_on_at(const Attack* a, uint32_t elapsed) {
    const uint32_t total = phase_total_nonzero(attack_warn_ms(a));
    const uint32_t seg = phase_total_nonzero(total / 4u);
    return (uint8_t)(((elapsed / seg) & 1u) == 0u);
}

static uint8_t laser_dash_visible_at(const Attack* a, uint32_t elapsed) {
    const uint32_t total = phase_total_nonzero(attack_warn_ms(a));
    const uint32_t seg = phase_total_nonzero(total / 4u);
    (void)a;
    return (uint8_t)(((elapsed / seg) & 1u) == 0u);
}

static uint8_t laser_dash_phase_at(const Attack* a, uint32_t elapsed) {
    const uint32_t total = phase_total_nonzero(attack_warn_ms(a));
    return (uint8_t)((elapsed / phase_total_nonzero(total / 2u)) & 1u);
}

static void laser_warning_bounds(uint8_t index, const Attack* a, Rect* out) {
    const int16_t active_pos = resolve_laser_pos(index, a);
    const int16_t active_th = a->u.laser.thickness;
    const int16_t warn_th = warning_thickness(active_th);
    const int16_t warn_pos = centered_warn_pos(active_pos, active_th, warn_th);
    if (a->type == attack_laser_h) {
        out->x = 0;
        out->y = warn_pos;
        out->w = ARENA_W;
        out->h = warn_th;
    } else {
        out->x = warn_pos;
        out->y = 0;
        out->w = warn_th;
        out->h = ARENA_H;
    }
}

static void laser_warning_sweep_rect_at(uint8_t index, const Attack* a, uint32_t elapsed, Rect* out) {
    Rect line;
    laser_warning_bounds(index, a, &line);
    const uint32_t total = phase_total_nonzero(attack_warn_ms(a));
    const uint32_t p = (elapsed > total) ? total : elapsed;

    if (a->type == attack_laser_h) {
        int16_t len = (int16_t)((uint32_t)ARENA_W * p / total);
        if (len > ARENA_W) { len = ARENA_W; }
        out->y = line.y;
        out->h = line.h;
        out->w = len;
        out->x = (a->u.laser.style == laser_warn_sweep_rtl) ? (int16_t)(ARENA_W - len) : 0;
    } else {
        int16_t len = (int16_t)((uint32_t)ARENA_H * p / total);
        if (len > ARENA_H) { len = ARENA_H; }
        out->x = line.x;
        out->w = line.w;
        out->h = len;
        out->y = (a->u.laser.style == laser_warn_sweep_btt) ? (int16_t)(ARENA_H - len) : 0;
    }
}

static void add_laser_active_growth_dirty(
    Rect_cache* cache, uint8_t index, const Attack* a, uint32_t prev_active_ms, uint32_t now_active_ms) {
    const uint32_t draw_total = laser_sweep_draw_total_ms(a);
    if (prev_active_ms >= draw_total) { return; }
    if (now_active_ms > draw_total) { now_active_ms = draw_total; }
    if (now_active_ms <= prev_active_ms) { return; }

    Rect old_r;
    Rect new_r;
    attack_active_rect(index, a, prev_active_ms, &old_r);
    attack_active_rect(index, a, now_active_ms, &new_r);
    dirty_cache_add_rect_growth(cache, &old_r, &new_r);
}

static void add_attack_fade_dirty_if_needed(
    Rect_cache* cache, uint8_t index, const Attack* a, uint32_t prev_rel, uint32_t now_rel) {
    const uint32_t exist_ms = attack_exist_ms(a);
    if (exist_ms <= ATTACK_FADE_MS) { return; }

    const uint32_t warn_end = a->start_ms + attack_warn_ms(a);
    const uint32_t fade_rel = warn_end + exist_ms - ATTACK_FADE_MS;
    const uint32_t end = warn_end + exist_ms;

    if (!(prev_rel < fade_rel && now_rel >= fade_rel && now_rel < end)) { return; }

    Rect r;
    attack_active_rect(index, a, exist_ms - ATTACK_FADE_MS, &r);
    dirty_cache_add_rect(cache, r);
}

static void collect_rect_warning_dirty_transition(Rect_cache* cache, const Attack* a, uint32_t prev_elapsed,
    uint32_t now_elapsed, uint8_t entered_warning) {
    const int16_t x = a->u.rect.x;
    const int16_t y = a->u.rect.y;
    const int16_t w = a->u.rect.w;
    const int16_t h = a->u.rect.h;
    const Rect_warn_style style = a->u.rect.style;

    if (entered_warning) { dirty_cache_add_border(cache, x, y, w, h); }

    if (style == rect_warn_center_expand || style == rect_warn_fill_from_left ||
        style == rect_warn_fill_from_right || style == rect_warn_fill_from_top ||
        style == rect_warn_fill_from_bottom) {
        Rect old_fill;
        Rect new_fill;
        rect_warning_fill_rect_at(a, prev_elapsed, &old_fill);
        rect_warning_fill_rect_at(a, now_elapsed, &new_fill);
        dirty_cache_add_rect_growth(cache, &old_fill, &new_fill);
    } else if (style == rect_warn_block_flash) {
        if (entered_warning ||
            rect_block_flash_on_at(a, prev_elapsed) != rect_block_flash_on_at(a, now_elapsed)) {
            dirty_cache_add_xywh(cache, x, y, w, h);
        }
    } else {
        dirty_cache_add_border(cache, x, y, w, h);
    }
}

static void collect_laser_warning_dirty_transition(Rect_cache* cache, uint8_t index, const Attack* a,
    uint32_t prev_elapsed, uint32_t now_elapsed, uint8_t entered_warning) {
    if (a->u.laser.style == laser_warn_dash_flash) {
        if (entered_warning ||
            laser_dash_visible_at(a, prev_elapsed) != laser_dash_visible_at(a, now_elapsed) ||
            laser_dash_phase_at(a, prev_elapsed) != laser_dash_phase_at(a, now_elapsed)) {
            Rect r;
            laser_warning_bounds(index, a, &r);
            dirty_cache_add_rect(cache, r);
        }
    } else {
        Rect old_r;
        Rect new_r;
        laser_warning_sweep_rect_at(index, a, prev_elapsed, &old_r);
        laser_warning_sweep_rect_at(index, a, now_elapsed, &new_r);
        dirty_cache_add_rect_growth(cache, &old_r, &new_r);
    }
}

static void collect_attack_dirty_transition(Rect_cache* cache, uint32_t prev_rel, uint32_t now_rel) {
    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (a->type == attack_bullet) { continue; }

        const uint32_t start = a->start_ms;
        const uint32_t warn_ms = attack_warn_ms(a);
        const uint32_t warn_end = start + warn_ms;
        const uint32_t end = attack_end_ms(a);

        if (now_rel < start || prev_rel >= end) { continue; }

        const uint8_t entered_warning = (uint8_t)(prev_rel < start && now_rel >= start);
        const uint8_t entered_active = (uint8_t)(prev_rel < warn_end && now_rel >= warn_end);
        const uint8_t ended = (uint8_t)(prev_rel < end && now_rel >= end);

        if (prev_rel < warn_end && now_rel >= start) {
            uint32_t prev_e = 0u;
            uint32_t now_e = 0u;
            if (prev_rel > start) { prev_e = prev_rel - start; }
            now_e = (now_rel < warn_end) ? (now_rel - start) : warn_ms;
            if (now_e > prev_e || entered_warning) {
                if (a->type == attack_rect) {
                    collect_rect_warning_dirty_transition(cache, a, prev_e, now_e, entered_warning);
                } else {
                    collect_laser_warning_dirty_transition(cache, i, a, prev_e, now_e, entered_warning);
                }
            }
        }

        if (entered_active) {
            Rect r;
            if (attack_draw_bounds_at(i, a, warn_end, &r)) { dirty_cache_add_rect(cache, r); }
        }

        if (!entered_active && now_rel >= warn_end && prev_rel < end) {
            if ((a->type == attack_laser_h || a->type == attack_laser_v) &&
                laser_is_sweep_style(a->u.laser.style)) {
                uint32_t prev_active = (prev_rel > warn_end) ? (prev_rel - warn_end) : 0u;
                uint32_t now_active = (now_rel < end) ? (now_rel - warn_end) : attack_exist_ms(a);
                add_laser_active_growth_dirty(cache, i, a, prev_active, now_active);
            }
        }

        add_attack_fade_dirty_if_needed(cache, i, a, prev_rel, now_rel);

        if (ended) {
            Rect r;
            const uint32_t last_t = (end > 0u) ? (end - 1u) : end;
            if (attack_draw_bounds_at(i, a, last_t, &r)) { dirty_cache_add_rect(cache, r); }
        }
    }
}

static uint8_t attack_is_finished(const Attack* a, uint8_t index, uint32_t now_rel) {
    if (a->type == attack_bullet) {
        Bullet_runtime* b = bullet_runtime_for_attack(index);
        return (uint8_t)(b != NULL && b->finished);
    }
    return (uint8_t)(now_rel >= attack_end_ms(a));
}

static void draw_bullet_spawn_hint(const Attack* a, uint32_t elapsed) {
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
    Bullet_runtime* b = bullet_runtime_for_attack(index);
    if (b == NULL) { return; }
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

static void update_bullet_runtime(uint8_t index, const Attack* a, uint32_t dt_ms) {
    Bullet_runtime* b = bullet_runtime_for_attack(index);
    if (b == NULL) { return; }
    const int16_t size = a->u.bullet.size;

    b->just_finished = 0;
    if (!b->initialized) { bullet_spawn(index, a); }
    if (!b->active || b->finished) { return; }

    b->prev_x256 = b->x256;
    b->prev_y256 = b->y256;

    /* Direction update — fixed 50 Hz, independent of task frequency */
    g_bullet_dir_accum += dt_ms;
    if (g_bullet_dir_accum >= BULLET_DIR_UPDATE_MS) {
        g_bullet_dir_accum -= BULLET_DIR_UPDATE_MS;

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
    }

    /* Position update — always dt-scaled */
    const int32_t step256 = (int32_t)a->u.bullet.speed_px_s * 256 * (int32_t)dt_ms / 1000;
    b->x256 += step256 * b->dir_x / BULLET_DIR_SCALE;
    b->y256 += step256 * b->dir_y / BULLET_DIR_SCALE;

    const int16_t x = (int16_t)(b->x256 >> 8);
    const int16_t y = (int16_t)(b->y256 >> 8);
    if (x <= 0 || y <= 0 || x + size >= ARENA_W || y + size >= ARENA_H) {
        b->active = 0;
        b->finished = 1;
        b->just_finished = 1;
    }
}

static void bullet_runtime_rect(const Bullet_runtime* b, const Attack* a, uint8_t use_prev, Rect* out) {
    const int16_t size = a->u.bullet.size;
    const int16_t x = (int16_t)(((use_prev ? b->prev_x256 : b->x256) >> 8) - 1);
    const int16_t y = (int16_t)(((use_prev ? b->prev_y256 : b->y256) >> 8) - 1);
    out->x = x;
    out->y = y;
    out->w = (int16_t)(size + 2);
    out->h = (int16_t)(size + 2);
}

static void draw_bullet_runtime(uint8_t index, const Attack* a) {
    const Bullet_runtime* b = bullet_runtime_for_attack(index);
    if (b == NULL) { return; }
    const int16_t size = a->u.bullet.size;
    if (!b->active || b->finished) { return; }

    const int16_t x = (int16_t)(b->x256 >> 8);
    const int16_t y = (int16_t)(b->y256 >> 8);

    /* 子弹最高攻击层：白色包边 + 内部攻击色，避免融入激光/矩形。 */
    arena_fill(x - 1, y - 1, size + 2, size + 2, COLOR_WHITE);
    arena_fill(x, y, size, size, a->color);
}

static void collect_bullet_dirty(uint32_t prev_rel, uint32_t now_rel, uint32_t dt_ms) {
    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (a->type != attack_bullet) { continue; }

        Bullet_runtime* b = bullet_runtime_for_attack(i);
        if (b == NULL) { continue; }

        const uint32_t warn_ms = attack_warn_ms(a);

        if (prev_rel >= a->start_ms && prev_rel < a->start_ms + warn_ms) {
            int16_t x, y, w, h;
            bullet_spawn_rect(a, &x, &y, &w, &h);
            dirty_cache_add_xywh(&g_final_dirty_cache, x - 1, y - 1, w + 2, h + 2);
        }
        if (now_rel >= a->start_ms && now_rel < a->start_ms + warn_ms) {
            int16_t x, y, w, h;
            bullet_spawn_rect(a, &x, &y, &w, &h);
            dirty_cache_add_xywh(&g_final_dirty_cache, x - 1, y - 1, w + 2, h + 2);
        }

        if (now_rel >= a->start_ms + warn_ms && !b->finished) {
            update_bullet_runtime(i, a, (now_rel == prev_rel) ? 0u : dt_ms);
        } else {
            b->just_finished = 0;
        }

        if (b->initialized && (b->active || b->just_finished)) {
            Rect old_r;
            Rect new_r;
            bullet_runtime_rect(b, a, 1, &old_r);
            bullet_runtime_rect(b, a, 0, &new_r);
            dirty_cache_add_rect(&g_final_dirty_cache, old_r);
            dirty_cache_add_rect(&g_final_dirty_cache, new_r);
        }
    }
}

static void collect_player_dirty(void) {
    const int16_t px = (int16_t)(g_player_x256 >> 8);
    const int16_t py = (int16_t)(g_player_y256 >> 8);
    if (px == g_prev_player_x && py == g_prev_player_y) { return; }
    dirty_cache_add_xywh(&g_final_dirty_cache, g_prev_player_x, g_prev_player_y, PLAYER_SIZE, PLAYER_SIZE);
    dirty_cache_add_xywh(&g_final_dirty_cache, px, py, PLAYER_SIZE, PLAYER_SIZE);
}

static uint8_t player_hits_active_attacks(uint32_t now_rel) {
    Rect pr;
    player_rect(&pr);

    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (a->type == attack_bullet || now_rel < a->start_ms) { continue; }

        const uint32_t elapsed = now_rel - a->start_ms;
        const uint32_t warn_ms = attack_warn_ms(a);
        const uint32_t exist_ms = attack_exist_ms(a);
        if (elapsed >= warn_ms && elapsed < warn_ms + exist_ms) {
            Rect ar;
            attack_active_rect(i, a, elapsed - warn_ms, &ar);
            if (rect_overlaps_strict(&pr, &ar)) { return 1; }
        }
    }

    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (a->type != attack_bullet) { continue; }

        const Bullet_runtime* b = bullet_runtime_for_attack(i);
        if (b == NULL || !b->active || b->finished) { continue; }

        Rect br;
        bullet_runtime_rect(b, a, 0, &br);
        if (rect_overlaps_strict(&pr, &br)) { return 1; }
    }

    return 0;
}

static void collect_layer_dirty_and_update_state(uint32_t prev_rel, uint32_t now_rel, uint32_t dt_ms) {
    dirty_all_layers_clear();

    collect_attack_dirty_transition(&g_final_dirty_cache, prev_rel, now_rel);
    collect_bullet_dirty(prev_rel, now_rel, dt_ms);
    collect_player_dirty();
}

static void render_rect_layer(uint32_t now_rel) {
    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (a->type != attack_rect || now_rel < a->start_ms) { continue; }

        const uint32_t elapsed = now_rel - a->start_ms;
        const uint32_t warn_ms = attack_warn_ms(a);
        const uint32_t exist_ms = attack_exist_ms(a);
        if (elapsed < warn_ms) {
            render_attack_warning(i, a, elapsed);
        } else if (elapsed < warn_ms + exist_ms) {
            render_attack_active(i, a, elapsed - warn_ms);
        }
    }
}

static void render_laser_layer(uint32_t now_rel) {
    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (!(a->type == attack_laser_h || a->type == attack_laser_v) || now_rel < a->start_ms) { continue; }

        const uint32_t elapsed = now_rel - a->start_ms;
        const uint32_t warn_ms = attack_warn_ms(a);
        const uint32_t exist_ms = attack_exist_ms(a);
        if (elapsed < warn_ms) {
            render_attack_warning(i, a, elapsed);
        } else if (elapsed < warn_ms + exist_ms) {
            render_laser_sweep_residue(i, a);
            render_attack_active(i, a, elapsed - warn_ms);
        }
    }
}

static void render_bullet_layer(uint32_t now_rel) {
    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (a->type != attack_bullet || now_rel < a->start_ms) { continue; }

        const uint32_t elapsed = now_rel - a->start_ms;
        const uint32_t warn_ms = attack_warn_ms(a);
        if (elapsed < warn_ms) {
            draw_bullet_spawn_hint(a, elapsed);
        } else {
            draw_bullet_runtime(i, a);
        }
    }
}

static void render_dirty_scene(uint32_t now_rel, uint16_t player_color) {
    for (uint8_t i = 0; i < g_final_dirty_cache.count; i++) {
        g_clip_rect = g_final_dirty_cache.rects[i];
        g_clip_enabled = 1;

        arena_fill(g_clip_rect.x, g_clip_rect.y, g_clip_rect.w, g_clip_rect.h, COLOR_BLACK);
        render_rect_layer(now_rel);
        render_laser_layer(now_rel);
        render_bullet_layer(now_rel);
        draw_player(player_color);
    }

    g_clip_enabled = 0;
}

static void update_attack_finish_scan(uint32_t now_rel) {
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
    Game_Graphics_Clear_Game_Area(g_lcd);
    Game_Graphics_Draw_Text(g_lcd, 24, 42, "MOVE: JOYSTICK", 1, COLOR_GRAY);
    Game_Graphics_Draw_Text(g_lcd, 156, 42, "X/B: PAUSE", 1, COLOR_GRAY);
    arena_fill(0, 0, ARENA_W, ARENA_H, COLOR_BLACK);
    arena_border();
}
static void reset_game(void) {
    g_state = game_state_playing;
    g_finished = 0;
    g_attack_scan_index = 0;
    g_all_attacks_done = 0;
    g_all_attacks_done_ms = 0;
    g_start_ms = Game_Runtime_Get_Tick_Ms();
    g_rng_state = 0x4d595df4u ^ g_start_ms;
    g_last_ms = g_start_ms;
    g_survive_ms = 0;
    g_player_x256 = ((ARENA_W - PLAYER_SIZE) / 2) << 8;
    g_player_y256 = ((ARENA_H - PLAYER_SIZE) / 2) << 8;
    g_prev_player_x = (int16_t)(g_player_x256 >> 8);
    g_prev_player_y = (int16_t)(g_player_y256 >> 8);
    memset(g_bullets, 0, sizeof(g_bullets));
    g_bullet_dir_accum = 0;
    memset(g_laser_track_pos, 0, sizeof(g_laser_track_pos));
    memset(g_laser_track_pos_valid, 0, sizeof(g_laser_track_pos_valid));
    dirty_all_layers_clear();
    g_clip_enabled = 0;
    render_static_screen();
    draw_player(COLOR_WHITE);
}

void Dodge_Box_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    g_vib_motor = hardware->vib_motor;
    reset_game();
}

static void move_player(const Game_input* input, uint32_t dt_ms) {
    if (!input->stick_active) { return; }

    const int32_t step256 = (int32_t)PLAYER_SPEED_PX_S * 256 * (int32_t)dt_ms / 1000;
    g_player_x256 += (int32_t)(input->axis_x * (float)step256);
    g_player_y256 += (int32_t)(input->axis_y * (float)step256);

    if (g_player_x256 < 0) { g_player_x256 = 0; }
    if (g_player_y256 < 0) { g_player_y256 = 0; }
    if (g_player_x256 > (ARENA_W - PLAYER_SIZE) * 256) { g_player_x256 = (ARENA_W - PLAYER_SIZE) * 256; }
    if (g_player_y256 > (ARENA_H - PLAYER_SIZE) * 256) { g_player_y256 = (ARENA_H - PLAYER_SIZE) * 256; }
}

Game_result Dodge_Box_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state == game_state_failed || g_state == game_state_clear) {
        if (input->a_pressed) { reset_game(); }
        return game_result_running;
    }

    const uint32_t now = Game_Runtime_Get_Tick_Ms();
    const uint32_t prev_rel = g_survive_ms;
    uint32_t dt_ms = now - g_last_ms;
    if (dt_ms > MAX_DT_MS) { dt_ms = MAX_DT_MS; }
    g_last_ms = now;

    const uint32_t now_rel = now - g_start_ms;
    g_prev_player_x = (int16_t)(g_player_x256 >> 8);
    g_prev_player_y = (int16_t)(g_player_y256 >> 8);

    move_player(input, dt_ms);
    collect_layer_dirty_and_update_state(prev_rel, now_rel, dt_ms);
    update_attack_finish_scan(now_rel);

#ifndef GOD_MODE
    if (player_hits_active_attacks(now_rel)) {
        g_state = game_state_failed;
        g_finished = 1;
        Vib_Motor_Play_Effect(g_vib_motor, vib_effect_hit_heavy);
        render_dirty_scene(now_rel, COLOR_RED);
        Game_Graphics_Fill_Rect(g_lcd, 0, 258, SCREEN_WIDTH, 14, COLOR_BLACK);
        draw_text_centered(260, "FAILED - PRESS", 1, COLOR_RED);
        return game_result_running;
    }
#endif

    render_dirty_scene(now_rel, COLOR_WHITE);

    g_survive_ms = now_rel;

    if (g_all_attacks_done && (now_rel - g_all_attacks_done_ms >= 2000u)) {
        g_state = game_state_clear;
        g_finished = 1;
        Vib_Motor_Play_Effect(g_vib_motor, vib_effect_victory);
        Game_Graphics_Fill_Rect(g_lcd, 0, 258, SCREEN_WIDTH, 14, COLOR_BLACK);
        draw_text_centered(260, "CLEAR - PRESS", 1, COLOR_GREEN);
    }

    return game_result_running;
}

uint32_t Dodge_Box_Get_Score(void) { return g_survive_ms / 100u; }

uint8_t Dodge_Box_Is_Finished(void) { return g_finished; }
