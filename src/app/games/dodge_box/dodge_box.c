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
#define WARNING_MS          500u
#define DEFAULT_ACTIVE_MS   500u

static uint16_t dim_color(uint16_t c){
    // simple RGB565 dimming
    uint16_t r = (c >> 11) & 0x1F;
    uint16_t g = (c >> 5) & 0x3F;
    uint16_t b = (c) & 0x1F;
    r = r >> 1;
    g = g >> 1;
    b = b >> 1;
    return (r<<11)|(g<<5)|b;
}

#define MAX_DT_MS           50u
#define BULLET_MAX_COUNT    32u
#define BULLET_DIR_SCALE    256l

#define SAFETY_BYTES        ((ARENA_W * ARENA_H + 7) / 8)
#define ATTACK_COUNT        ((uint8_t)(sizeof(g_level) / sizeof(g_level[0])))

#define COLOR_BLACK         0x0000u
#define COLOR_WHITE         0xffffu
#define COLOR_RED           0xf800u
#define COLOR_GREEN         0x07e0u
#define COLOR_BLUE          0x001fu
#define COLOR_CYAN          0x07ffu
#define COLOR_YELLOW        0xffe0u
#define COLOR_MAGENTA       0xf81fu
#define COLOR_GRAY          0x8410u
#define COLOR_DARK          0x2104u
#define COLOR_DIM_RED       0x6000u
#define COLOR_DIM_CYAN      0x0339u
#define COLOR_DIM_YELLOW    0x6300u

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
} Rect_warn_style;

typedef enum {
    bullet_side_top,
    bullet_side_bottom,
    bullet_side_left,
    bullet_side_right,
} Bullet_side;

typedef struct {
    uint32_t start_ms;
    uint32_t end_ms; /* 激光/区域：消失时间；子弹：无 end_ms，填 0。 */
    Attack_type type;
    uint16_t color;
    union {
        struct {
            uint8_t pos;       /* horizontal: y, vertical: x, in arena coords */
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
            uint8_t offset;  /* top/bottom: x；left/right: y */
            uint8_t size;
            uint16_t speed_px_s;
            uint8_t alpha;   /* 0~255，越大越保持上周期方向 */
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

/* 一个完整单关卡。start_ms 是提示开始时间；提示 0.5s，攻击 0.5s。 */
static const Attack g_level[] = {
    {  300u, 1300u, attack_laser_h, COLOR_RED,     {.laser = { 30u, 3u, laser_warn_dash_flash}}},
    {  900u, 1900u, attack_laser_v, COLOR_CYAN,    {.laser = { 82u, 3u, laser_warn_sweep_ttb}}},
    { 1400u,    0u, attack_bullet,  COLOR_YELLOW,  {.bullet = { bullet_side_left,   36u, 4u,  72u, 250u }}},
    { 1700u, 3000u, attack_rect,    COLOR_YELLOW,  {.rect  = { 18u, 45u, 40u, 36u, rect_warn_center_expand}}},
    { 2300u, 3550u, attack_laser_h, COLOR_MAGENTA, {.laser = {126u, 3u, laser_warn_sweep_ltr}}},
    { 2900u,    0u, attack_bullet,  COLOR_CYAN,    {.bullet = { bullet_side_top,    92u, 4u,  82u, 250u }}},
    { 3200u, 4300u, attack_rect,    COLOR_RED,     {.rect  = { 62u, 84u, 42u, 44u, rect_warn_block_flash}}},
    { 3900u, 5050u, attack_laser_v, COLOR_GREEN,   {.laser = { 34u, 3u, laser_warn_sweep_btt}}},
    { 4550u,    0u, attack_bullet,  COLOR_RED,     {.bullet = { bullet_side_right, 128u, 4u,  86u, 250u }}},
    { 4750u, 5900u, attack_rect,    COLOR_CYAN,    {.rect  = { 22u,132u, 72u, 28u, rect_warn_edge_grow}}},
    { 5350u, 6550u, attack_laser_h, COLOR_RED,     {.laser = { 78u, 3u, laser_warn_sweep_rtl}}},
    { 6000u, 7200u, attack_rect,    COLOR_YELLOW,  {.rect  = { 40u, 28u, 54u, 34u, rect_warn_center_expand}}},
    { 6650u,    0u, attack_bullet,  COLOR_MAGENTA, {.bullet = { bullet_side_bottom, 50u, 5u,  90u, 250u }}},
    { 6900u, 8100u, attack_laser_v, COLOR_MAGENTA, {.laser = {104u, 3u, laser_warn_dash_flash}}},
    { 7600u, 8750u, attack_rect,    COLOR_RED,     {.rect  = { 12u, 92u, 96u, 24u, rect_warn_edge_grow}}},
    { 8250u, 9400u, attack_laser_h, COLOR_CYAN,    {.laser = {154u, 3u, laser_warn_sweep_ltr}}},
};

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

static int32_t abs_i32(int32_t v) { return (v < 0) ? -v : v; }

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

static uint8_t rect_overlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                            int16_t bx, int16_t by, int16_t bw, int16_t bh) {
    return (uint8_t)(ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by);
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
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
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

static void draw_rect_edge_grow(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, uint32_t phase_ms) {
    const uint32_t p = phase_ms > WARNING_MS ? WARNING_MS : phase_ms;
    const int16_t hw = (int16_t)((uint32_t)w * p / WARNING_MS / 2u);
    const int16_t hh = (int16_t)((uint32_t)h * p / WARNING_MS / 2u);
    arena_fill(x, y, hw, 1, color);
    arena_fill(x + w - hw, y, hw, 1, color);
    arena_fill(x, y + h - 1, hw, 1, color);
    arena_fill(x + w - hw, y + h - 1, hw, 1, color);
    arena_fill(x, y, 1, hh, color);
    arena_fill(x, y + h - hh, 1, hh, color);
    arena_fill(x + w - 1, y, 1, hh, color);
    arena_fill(x + w - 1, y + h - hh, 1, hh, color);
}

static void render_attack_warning(const Attack* a, uint32_t elapsed) {
    const uint16_t wc = warning_color(a->color);
    if (a->type == attack_laser_h || a->type == attack_laser_v) {
        const uint8_t is_h = (a->type == attack_laser_h);
        const int16_t active_pos = a->u.laser.pos;
        const int16_t active_th = a->u.laser.thickness;
        const int16_t warn_th = warning_thickness(active_th);
        const int16_t warn_pos = centered_warn_pos(active_pos, active_th, warn_th);
        const Laser_warn_style style = a->u.laser.style;
        if (style == laser_warn_dash_flash) {
            /* 0.5s 内闪两次：四个 125ms 段，亮/灭/亮/灭。提示线比激光细。 */
            if (((elapsed / 125u) & 1u) == 0u) {
                if (is_h) { draw_hline_dashed(warn_pos, warn_th, wc, (uint8_t)((elapsed / 250u) & 1u)); }
                else { draw_vline_dashed(warn_pos, warn_th, wc, (uint8_t)((elapsed / 250u) & 1u)); }
            }
        } else {
            /* 扫描速度由长度 / 0.5s 得出。sweep 提示会保留，由后续实体激光自然覆盖。 */
            if (is_h) {
                int16_t len = (int16_t)((uint32_t)ARENA_W * elapsed / WARNING_MS);
                if (len > ARENA_W) { len = ARENA_W; }
                if (style == laser_warn_sweep_rtl) { arena_fill(ARENA_W - len, warn_pos, len, warn_th, wc); }
                else { arena_fill(0, warn_pos, len, warn_th, wc); }
            } else {
                int16_t len = (int16_t)((uint32_t)ARENA_H * elapsed / WARNING_MS);
                if (len > ARENA_H) { len = ARENA_H; }
                if (style == laser_warn_sweep_btt) { arena_fill(warn_pos, ARENA_H - len, warn_th, len, wc); }
                else { arena_fill(warn_pos, 0, warn_th, len, wc); }
            }
        }
    } else {
        const int16_t x = a->u.rect.x;
        const int16_t y = a->u.rect.y;
        const int16_t w = a->u.rect.w;
        const int16_t h = a->u.rect.h;
        if (a->u.rect.style == rect_warn_center_expand) {
            const int16_t cw = (int16_t)((uint32_t)w * elapsed / WARNING_MS);
            const int16_t ch = (int16_t)((uint32_t)h * elapsed / WARNING_MS);
            draw_rect_border(x, y, w, h, wc);
            arena_fill(x + (w - cw) / 2, y + (h - ch) / 2, cw, ch, wc);
        } else if (a->u.rect.style == rect_warn_block_flash) {
            if (((elapsed / 125u) & 1u) == 0u) { arena_fill(x, y, w, h, wc); }
            else { draw_rect_border(x, y, w, h, wc); }
        } else {
            draw_rect_edge_grow(x, y, w, h, wc, elapsed);
        }
    }
}

static void render_attack_active(const Attack* a, uint32_t active_ms) {
    const uint32_t active_total = (a->end_ms > a->start_ms + WARNING_MS) ?
                                  (a->end_ms - a->start_ms - WARNING_MS) : DEFAULT_ACTIVE_MS;
    if (a->type == attack_laser_h) {
        int16_t len = ARENA_W;
        int16_t x = 0;
        if (a->u.laser.style == laser_warn_sweep_ltr || a->u.laser.style == laser_warn_sweep_rtl) {
            len = (int16_t)((uint32_t)ARENA_W * active_ms / active_total);
            if (len > ARENA_W) { len = ARENA_W; }
            if (a->u.laser.style == laser_warn_sweep_rtl) { x = ARENA_W - len; }
        }
        arena_fill(x, a->u.laser.pos, len, a->u.laser.thickness, a->color);
        mark_danger_rect(x, a->u.laser.pos, len, a->u.laser.thickness);
    } else if (a->type == attack_laser_v) {
        int16_t len = ARENA_H;
        int16_t y = 0;
        if (a->u.laser.style == laser_warn_sweep_ttb || a->u.laser.style == laser_warn_sweep_btt) {
            len = (int16_t)((uint32_t)ARENA_H * active_ms / active_total);
            if (len > ARENA_H) { len = ARENA_H; }
            if (a->u.laser.style == laser_warn_sweep_btt) { y = ARENA_H - len; }
        }
        arena_fill(a->u.laser.pos, y, a->u.laser.thickness, len, a->color);
        mark_danger_rect(a->u.laser.pos, y, a->u.laser.thickness, len);
    } else if (a->type == attack_rect) {
        arena_fill(a->u.rect.x, a->u.rect.y, a->u.rect.w, a->u.rect.h, a->color);
        mark_danger_rect(a->u.rect.x, a->u.rect.y, a->u.rect.w, a->u.rect.h);
    }
}

static Rect attack_bounds(const Attack* a) {
    Rect r;
    if (a->type == attack_laser_h) {
        r.x = 0; r.y = a->u.laser.pos; r.w = ARENA_W; r.h = a->u.laser.thickness;
    } else if (a->type == attack_laser_v) {
        r.x = a->u.laser.pos; r.y = 0; r.w = a->u.laser.thickness; r.h = ARENA_H;
    } else if (a->type == attack_rect) {
        r.x = a->u.rect.x; r.y = a->u.rect.y; r.w = a->u.rect.w; r.h = a->u.rect.h;
    } else {
        r.x = 0; r.y = 0; r.w = ARENA_W; r.h = ARENA_H;
    }
    return r;
}

static uint8_t attack_is_finished(const Attack* a, uint8_t index, uint32_t now_rel) {
    if (a->type == attack_bullet) { return g_bullets[index].finished; }
    return (uint8_t)(now_rel >= a->end_ms);
}

static uint8_t bullet_warning_active(const Attack* a, uint32_t now_rel) {
    return (uint8_t)(a->type == attack_bullet && now_rel >= a->start_ms && now_rel < a->start_ms + WARNING_MS);
}

static void clear_arena_border_pixels(void) {
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X - 2, ARENA_Y - 2, ARENA_W + 4, 2, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X - 2, ARENA_Y + ARENA_H, ARENA_W + 4, 2, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X - 2, ARENA_Y - 2, 2, ARENA_H + 4, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_lcd, ARENA_X + ARENA_W, ARENA_Y - 2, 2, ARENA_H + 4, COLOR_BLACK);
}

static void draw_bullet_spawn_hint(const Attack* a, uint32_t elapsed) {
    /* 子弹提示只在子弹出生的小方块处闪烁，不再闪整条边。
     * 提示色使用子弹颜色的暗色版本。
     */
    if (((elapsed / 125u) & 1u) != 0u) { return; }
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
    /* 增量清除：只清玩家旧位置、会变化的提示/攻击区域、子弹旧位置。
     * 注意：sweep 激光提示线在 active 期间不清除，让实体激光自然覆盖。
     */
    arena_fill(g_prev_player_x - 1, g_prev_player_y - 1, PLAYER_SIZE + 2, PLAYER_SIZE + 2, COLOR_BLACK);

    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (a->type == attack_bullet) {
            if (now_rel + 40u >= a->start_ms && now_rel < a->start_ms + WARNING_MS + 80u) {
                int16_t x, y, w, h;
                bullet_spawn_rect(a, &x, &y, &w, &h);
                arena_fill(x - 2, y - 2, w + 4, h + 4, COLOR_BLACK);
            }
            if (g_bullets[i].initialized) {
                const int16_t px = (int16_t)(g_bullets[i].prev_x256 >> 8);
                const int16_t py = (int16_t)(g_bullets[i].prev_y256 >> 8);
                const int16_t cx = (int16_t)(g_bullets[i].x256 >> 8);
                const int16_t cy = (int16_t)(g_bullets[i].y256 >> 8);
                const int16_t sz = g_level[i].u.bullet.size;
                arena_fill(px - 2, py - 2, sz + 4, sz + 4, COLOR_BLACK);
                arena_fill(cx - 2, cy - 2, sz + 4, sz + 4, COLOR_BLACK);
            }
            continue;
        }

        if (now_rel + 40u < a->start_ms || now_rel > a->end_ms + 40u) { continue; }

        if (a->type == attack_laser_h || a->type == attack_laser_v) {
            const uint32_t warn_end = a->start_ms + WARNING_MS;
            const uint8_t is_sweep = laser_is_sweep_style(a->u.laser.style);
            if (is_sweep && now_rel >= warn_end && now_rel <= a->end_ms) {
                /* 不清 sweep warning/active 区域，保留提示线让激光自然覆盖。 */
                continue;
            }
        }

        Rect r = attack_bounds(a);
        arena_fill(r.x - 1, r.y - 1, r.w + 2, r.h + 2, COLOR_BLACK);
    }
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
        if (elapsed < WARNING_MS) {
            render_attack_warning(a, elapsed);
        } else if (now_rel < a->end_ms) {
            render_attack_active(a, elapsed - WARNING_MS);
        }
    }

    /* 第二层：子弹提示和子弹本体。这样子弹不会被其它攻击覆盖；玩家稍后最后渲染。 */
    for (uint8_t i = 0; i < ATTACK_COUNT; i++) {
        const Attack* a = &g_level[i];
        if (a->type != attack_bullet) { continue; }
        if (now_rel < a->start_ms) { continue; }
        const uint32_t elapsed = now_rel - a->start_ms;
        if (elapsed < WARNING_MS) {
            draw_bullet_spawn_hint(a, elapsed);
        } else {
            render_and_update_bullet(i, a, dt_ms);
        }
    }
}

static void update_attack_finish_scan(uint32_t now_rel) {
    /*
     * 只从当前扫描索引向后检查。
     * 激光/区域以 end_ms 为完成点；子弹没有 end_ms，碰到边框消失后才算完成。
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
    g_last_ms = g_start_ms;
    g_survive_ms = 0;
    g_player_x256 = ((ARENA_W - PLAYER_SIZE) / 2) << 8;
    g_player_y256 = ((ARENA_H - PLAYER_SIZE) / 2) << 8;
    g_prev_player_x = (int16_t)(g_player_x256 >> 8);
    g_prev_player_y = (int16_t)(g_player_y256 >> 8);
    memset(g_danger_map, 0, sizeof(g_danger_map));
    memset(g_bullets, 0, sizeof(g_bullets));
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
    if (input->direction == game_direction_left) { dx = -1; }
    else if (input->direction == game_direction_right) { dx = 1; }
    else if (input->direction == game_direction_up) { dy = -1; }
    else if (input->direction == game_direction_down) { dy = 1; }

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
