#include "needle.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "buzzer.h"
#include "game_graphics.h"

/* ── 屏幕常量 ── */
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
/* ── 圆盘参数 ── */
#define DISK_CX       120
#define DISK_CY       145
#define TIP_R         55
#define TIP_SIZE      4
#define FLY_SPEED     5
#define MAX_NEEDLES   80
#define COLLIDE_ANGLE 7 /* 碰撞角度阈值，越小越宽松 */
#define LAUNCH_Y      GAME_AREA_BOTTOM

/* 转速参数：定点 1/256 单位/帧，线性从 4 针到 20 针 */
#define ANG_VEL_INIT  341 /* 341/256 ≈ 1.33（原 2 的 2/3） */
#define ANG_VEL_MAX   512 /* 512/256 = 2 */
#define NEEDLE_FULL   20  /* 到达最大转速的针数 */

/* ── 颜色 ── */
#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_RED     0xf800u
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0x4208u
#define COLOR_LIGHT   0xc618u

/* ── sin 表 64 等分 0-90°，缩放 128 ── */
static const uint8_t g_sin_table[65] = {0, 3, 6, 9, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49, 52,
    55, 58, 60, 63, 66, 68, 71, 74, 76, 79, 81, 84, 86, 88, 91, 93, 95, 97, 99, 101, 103, 105, 106, 108, 110,
    111, 113, 114, 116, 117, 118, 119, 121, 122, 122, 123, 124, 125, 126, 126, 127, 127, 127, 128, 128, 128,
    128};

static const uint16_t g_needle_colors[] = {0xf800u, 0xfc00u, 0xffe0u, 0x07e0u, 0x07ffu, 0x001fu, 0x8010u};

/* ── 类型 ── */
typedef enum { needle_state_ready, needle_state_flying, needle_state_over } Needle_state;

/* ── 静态变量 ── */
static Game_hardware g_hardware;
static Needle_state g_state;
static uint8_t g_needle_angles[MAX_NEEDLES]; /* 针在圆盘上的固定角度 (0-255) */
static uint8_t g_prev_angles[MAX_NEEDLES];   /* 上一帧针的屏幕绝对角度 */
static uint8_t g_needle_count;
static uint8_t g_disk_angle;
static uint16_t g_ang_vel;
static uint16_t g_disk_accum;
static int16_t g_fly_x, g_fly_y, g_fly_dx, g_fly_dy;
static int16_t g_launch_x;
static uint32_t g_score;
static uint32_t g_last_update_ms;

#define BASE_TICK_MS 20u

/* ── sin/cos 查找 ── */

static int16_t needle_sin(uint8_t angle) {
    const uint8_t q = angle >> 6;
    const uint8_t i = angle & 0x3Fu;
    if (q == 0) { return g_sin_table[i]; }
    if (q == 1) { return g_sin_table[64 - i]; }
    if (q == 2) { return (int16_t)(-g_sin_table[i]); }
    return (int16_t)(-g_sin_table[64 - i]);
}

static int16_t needle_cos(uint8_t angle) { return needle_sin((uint8_t)(angle + 64)); }

/* ── 填充裁剪 ── */

static void bar_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (y + h > SCREEN_HEIGHT) { h = SCREEN_HEIGHT - y; }
    if (w <= 0 || h <= 0) { return; }
    Game_Graphics_Fill_Rect(g_hardware.lcd, x, y, w, h, c);
}

/* ── 针尖屏幕坐标（+64 四舍五入，消除 LUT 平坦区的像素重叠） ── */

static int16_t round_div_128(int32_t val) {
    if (val >= 0) { return (int16_t)((val + 64) / 128); }
    return (int16_t)((val - 64) / 128);
}

static void pos_from_angle(uint8_t angle, int16_t* out_x, int16_t* out_y) {
    *out_x = (int16_t)(DISK_CX + round_div_128((int32_t)needle_cos(angle) * TIP_R));
    *out_y = (int16_t)(DISK_CY - round_div_128((int32_t)needle_sin(angle) * TIP_R));
}

/* ── atan2 近似 ── */

static uint8_t atan2_approx(int32_t dx, int32_t dy) {
    dy = -dy;
    if (dx == 0 && dy == 0) { return 0; }

    const int32_t adx = dx < 0 ? -dx : dx;
    const int32_t ady = dy < 0 ? -dy : dy;
    int32_t raw;
    uint8_t base;

    if (adx > ady) {
        raw = ady * 32 / adx;
        if (raw > 31) { raw = 31; }
        if (dx >= 0 && dy >= 0) {
            base = 0;
        } else if (dx < 0 && dy >= 0) {
            base = 96;
            raw = 31 - raw;
        } else if (dx < 0 && dy < 0) {
            base = 128;
        } else {
            base = 224;
            raw = 31 - raw;
        }
    } else {
        raw = adx * 32 / ady;
        if (raw > 31) { raw = 31; }
        if (dx >= 0 && dy >= 0) {
            base = 32;
            raw = 31 - raw;
        } else if (dx < 0 && dy >= 0) {
            base = 64;
        } else if (dx < 0 && dy < 0) {
            base = 160;
            raw = 31 - raw;
        } else {
            base = 192;
        }
    }
    return (uint8_t)(base + raw);
}

/* ── 绘制圆盘（只画一次，不旋转） ── */

static void draw_disk(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, DISK_CX - 24, DISK_CY - 24, 48, 48, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_hardware.lcd, DISK_CX - 18, DISK_CY - 18, 36, 36, COLOR_GRAY);
    Game_Graphics_Fill_Rect(g_hardware.lcd, DISK_CX - 10, DISK_CY - 10, 20, 20, COLOR_LIGHT);
    Game_Graphics_Fill_Rect(g_hardware.lcd, DISK_CX - 2, DISK_CY - 2, 4, 4, COLOR_WHITE);
}

/* ── 单个针尖绘制 ── */

static void draw_tip_at_angle(uint8_t angle, uint16_t color) {
    int16_t tx, ty;
    pos_from_angle(angle, &tx, &ty);
    Game_Graphics_Fill_Rect(g_hardware.lcd, tx - TIP_SIZE / 2, ty - TIP_SIZE / 2, TIP_SIZE, TIP_SIZE, color);
}

/* ── 旋转刷新：逐针原子擦旧绘新，避免批量擦绘的顺序污染 ── */

static void rotate_needles(void) {
    uint8_t i;

    for (i = 0; i < g_needle_count; i++) {
        /* 先擦除这根针的旧位置 */
        draw_tip_at_angle(g_prev_angles[i], COLOR_BLACK);

        /* 计算新绝对角度并立即绘制，不留间隙 */
        const uint8_t abs_angle = g_needle_angles[i] + g_disk_angle;
        g_prev_angles[i] = abs_angle;
        draw_tip_at_angle(abs_angle, g_needle_colors[i % 7]);
    }
}

/* ── 飞行针 ── */

static void draw_fly_tip(uint16_t color) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, g_fly_x - 2, g_fly_y - 2, 4, 4, color);
}

/* ── 瞄准线（竖线指示发射位置） ── */

static int16_t g_old_launch_x;

static void draw_aim_guide(void) {
    const int16_t ref_y = DISK_CY + TIP_R + 6;
    const int16_t line_h = LAUNCH_Y - ref_y - 4;

    /* 先刷一块背景色方块，再画细线 */
    if (g_old_launch_x != 0) { bar_fill(g_old_launch_x - 4, ref_y + 2, 8, line_h, COLOR_BLACK); }
    g_old_launch_x = g_launch_x;

    bar_fill(g_launch_x - 4, ref_y + 2, 8, line_h, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_hardware.lcd, g_launch_x - 1, ref_y + 2, 2, line_h, COLOR_GRAY);
}

/* ── UI ── */

static void draw_score(void) {
    bar_fill(198, 2, 40, 8, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_U32(g_hardware.lcd, 203, 2, g_score, 5, 1, COLOR_CYAN);
}

static void draw_bottom_text(const char* s, uint16_t color) {
    bar_fill(0, LAUNCH_Y, SCREEN_WIDTH, SCREEN_HEIGHT - LAUNCH_Y, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_Text(g_hardware.lcd, 40, LAUNCH_Y + 3, s, 1, color);
}

/* ── 重启 ── */

static void restart_game(void) {
    uint8_t i;

    g_state = needle_state_ready;
    g_needle_count = 0;
    g_disk_angle = 0;
    g_disk_accum = 0;
    g_ang_vel = ANG_VEL_INIT;
    g_launch_x = DISK_CX;
    g_old_launch_x = 0;
    g_score = 0;
    g_last_update_ms = Bsp_Get_Tick_Ms();

    g_needle_angles[0] = 0;
    g_needle_angles[1] = 64;
    g_needle_angles[2] = 128;
    g_needle_angles[3] = 192;
    g_needle_count = 4;
    for (i = 0; i < g_needle_count; i++) { g_prev_angles[i] = g_needle_angles[i]; }

    Game_Graphics_Clear_Game_Area(g_hardware.lcd);
    draw_disk();
    /* 初态直接画针，不做擦除 */
    for (i = 0; i < g_needle_count; i++) { draw_tip_at_angle(g_prev_angles[i], g_needle_colors[i % 7]); }
    draw_score();
    draw_aim_guide();
    draw_bottom_text("PRESS TO LAUNCH", COLOR_WHITE);
}

/* ── Init ── */

void Needle_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

/* ── 发射 ── */

static void launch_needle(void) {
    const int32_t dx = DISK_CX - g_launch_x;
    const int32_t dy = DISK_CY - LAUNCH_Y;
    if (dx == 0 && dy == 0) {
        g_fly_dx = 0;
        g_fly_dy = (int16_t)(-FLY_SPEED);
    } else {
        g_fly_dy = (int16_t)(-FLY_SPEED);
        g_fly_dx = (int16_t)(dx * FLY_SPEED / (-dy));
    }
    g_fly_x = g_launch_x;
    g_fly_y = LAUNCH_Y;
    g_state = needle_state_flying;
    Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_needle_launch);
    draw_bottom_text("", COLOR_BLACK);
}

/* ── Update ── */

Game_result Needle_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    /* ══ 时间驱动的 dt 计算（旋转 + 飞行共用） ══ */
    const uint32_t now = Bsp_Get_Tick_Ms();
    uint32_t dt = now - g_last_update_ms;
    if (dt > 100u) { dt = 100u; }
    g_last_update_ms = now;

    /* ══ 旋转圆盘（定点累加，按 dt 缩放） ══ */
    g_disk_accum += (uint32_t)g_ang_vel * dt / BASE_TICK_MS;
    g_disk_angle = (uint8_t)(g_disk_accum >> 8);
    rotate_needles();

    St7789* lcd = g_hardware.lcd;
    (void)lcd;

    /* ── Game Over ── */
    if (g_state == needle_state_over) {
        if (input->direction_pressed) { restart_game(); }
        return game_result_running;
    }

    /* ── Flying ── */
    if (g_state == needle_state_flying) {
        /* 旧位置：先填黑擦干净，再画浅灰细线修复 */
        Game_Graphics_Fill_Rect(g_hardware.lcd, g_fly_x - 2, g_fly_y - 2, 4, 4, COLOR_BLACK);
        Game_Graphics_Fill_Rect(g_hardware.lcd, g_fly_x - 1, g_fly_y - 2, 2, 4, COLOR_GRAY);

        g_fly_x += (int16_t)((int32_t)g_fly_dx * (int32_t)dt / (int32_t)BASE_TICK_MS);
        g_fly_y += (int16_t)((int32_t)g_fly_dy * (int32_t)dt / (int32_t)BASE_TICK_MS);

        draw_fly_tip(COLOR_WHITE);

        const int32_t dx = g_fly_x - DISK_CX;
        const int32_t dy = g_fly_y - DISK_CY;
        const int32_t dist2 = dx * dx + dy * dy;

        if (dist2 <= (TIP_R + 6) * (TIP_R + 6)) {
            const uint8_t fly_angle = atan2_approx(dx, dy);
            uint8_t collision = 0;
            uint8_t i;

            for (i = 0; i < g_needle_count; i++) {
                const uint8_t needle_abs = (uint8_t)(g_needle_angles[i] + g_disk_angle);
                int16_t diff = (int16_t)fly_angle - (int16_t)needle_abs;
                if (diff < 0) { diff = (int16_t)(-diff); }
                if (diff > 128) { diff = (int16_t)(256 - diff); }
                if (diff < COLLIDE_ANGLE) {
                    collision = 1;
                    break;
                }
            }

            if (collision || g_needle_count >= MAX_NEEDLES) {
                /* 擦除最后一帧的白色方块，修复残留 */
                Game_Graphics_Fill_Rect(g_hardware.lcd, g_fly_x - 2, g_fly_y - 2, 4, 4, COLOR_BLACK);
                Game_Graphics_Fill_Rect(g_hardware.lcd, g_fly_x - 1, g_fly_y - 2, 2, 4, COLOR_GRAY);
                g_state = needle_state_over;
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_life_lost);
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_defeat);
                draw_bottom_text("GAME OVER  PUSH RESTART", COLOR_RED);
            } else {
                /* 擦除最后一帧的白色方块，修复残留 */
                Game_Graphics_Fill_Rect(g_hardware.lcd, g_fly_x - 2, g_fly_y - 2, 4, 4, COLOR_BLACK);
                Game_Graphics_Fill_Rect(g_hardware.lcd, g_fly_x - 1, g_fly_y - 2, 2, 4, COLOR_GRAY);
                /* ── 插入新针，立即绘制到实际位置 ── */
                g_needle_angles[g_needle_count] = (uint8_t)((fly_angle - g_disk_angle) & 0xFFu);
                g_prev_angles[g_needle_count] = fly_angle;
                draw_tip_at_angle(fly_angle, g_needle_colors[g_needle_count % 7]);
                g_needle_count++;
                g_score++;
                draw_score();

                /* 线性增速：针越多转越快，20 针封顶 */
                if (g_needle_count >= NEEDLE_FULL) {
                    g_ang_vel = ANG_VEL_MAX;
                } else {
                    g_ang_vel = ANG_VEL_INIT + (uint16_t)((g_needle_count - 4) *
                                                          (ANG_VEL_MAX - ANG_VEL_INIT) / (NEEDLE_FULL - 4));
                }

                g_state = needle_state_ready;
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_needle_stick);
                draw_aim_guide();
                draw_bottom_text("PRESS TO LAUNCH", COLOR_WHITE);
            }
        }
        return game_result_running;
    }

    /* ── Ready ── */
    if (g_state == needle_state_ready) {
        if (input->direction == game_direction_left && g_launch_x > 30) {
            g_launch_x -= 4;
            draw_aim_guide();
        } else if (input->direction == game_direction_right && g_launch_x < 210) {
            g_launch_x += 4;
            draw_aim_guide();
        }
        if (input->confirm_pressed) {
            /* 擦除瞄准线 */
            g_old_launch_x = 0;
            launch_needle();
        }
    }

    return game_result_running;
}

/* ── Score / Finished ── */

uint32_t Needle_Get_Score(void) { return g_score; }

uint8_t Needle_Is_Finished(void) { return g_state == needle_state_over; }
