#include "needle.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "buzzer.h"
#include "game_graphics.h"

/* ── 屏幕常量 ── */
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define BAR_H         12
#define BAR_BOT       298

/* ── 圆盘参数 ── */
#define DISK_CX       120
#define DISK_CY       145
#define DISK_R        30
#define TIP_R         55
#define NEEDLE_SIZE   4
#define FLY_SPEED     5
#define MAX_NEEDLES   80
#define COLLIDE_ANGLE 10    /* 碰撞角度阈值 (256 圆) */
#define START_ANG_VEL 2
#define LAUNCH_Y      298

/* ── 颜色 ── */
#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_RED     0xf800u
#define COLOR_YELLOW  0xffe0u
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0x4208u
#define COLOR_MID     0x8410u
#define COLOR_LIGHT   0xc618u

/* ── sin 表 64 等分 0-90°，缩放 128 ── */
static const int8_t g_sin_table[65] = {
    0,   3,   6,   9,  13,  16,  19,  22,
   25,  28,  31,  34,  37,  40,  43,  46,
   49,  52,  55,  58,  60,  63,  66,  68,
   71,  74,  76,  79,  81,  84,  86,  88,
   91,  93,  95,  97,  99, 101, 103, 105,
  106, 108, 110, 111, 113, 114, 116, 117,
  118, 119, 121, 122, 122, 123, 124, 125,
  126, 126, 127, 127, 127, 128, 128, 128,
  128,
};

static const uint16_t g_needle_colors[] = {
    0xf800u, 0xfc00u, 0xffe0u, 0x07e0u, 0x07ffu, 0x001fu, 0x8010u,
};

/* ── 类型 ── */
typedef enum { needle_state_ready, needle_state_flying, needle_state_over } Needle_state;

/* ── 静态变量 ── */
static Game_hardware g_hardware;
static Needle_state g_state;
static uint8_t g_needle_angles[MAX_NEEDLES];  /* 针在圆盘上的固定角度 (0-255) */
static uint8_t g_prev_angles[MAX_NEEDLES];    /* 上一帧针的屏幕绝对角度 */
static uint8_t g_needle_count;
static uint8_t g_disk_angle;
static uint8_t g_ang_vel;
static int16_t g_fly_x, g_fly_y, g_fly_dx, g_fly_dy;
static int16_t g_launch_x;
static uint32_t g_score;

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
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (y + h > SCREEN_HEIGHT) { h = SCREEN_HEIGHT - y; }
    if (w <= 0 || h <= 0) { return; }
    Game_Graphics_Fill_Rect(g_hardware.lcd, x, y, w, h, c);
}

/* ── 获取针尖屏幕坐标 ── */

static void pos_from_angle(uint8_t angle, int16_t* out_x, int16_t* out_y) {
    *out_x = (int16_t)(DISK_CX + (int32_t)needle_cos(angle) * TIP_R / 128);
    *out_y = (int16_t)(DISK_CY - (int32_t)needle_sin(angle) * TIP_R / 128);
}

/* ── atan2 近似：计算 (dx, dy) 的角度 (0-255) ── */

static uint8_t atan2_approx(int32_t dx, int32_t dy) {
    /* dy 向上为负（屏幕坐标系），需要翻转 */
    dy = -dy;
    if (dx == 0 && dy == 0) { return 0; }

    const int32_t adx = dx < 0 ? -dx : dx;
    const int32_t ady = dy < 0 ? -dy : dy;
    int32_t raw;

    if (adx > ady) {
        raw = ady * 64 / adx;  /* 0..63 */
        if (dx >= 0 && dy >= 0) { return (uint8_t)raw; }                  /* Q1 */
        if (dx < 0 && dy >= 0) { return (uint8_t)(128 - raw); }           /* Q2 */
        if (dx < 0 && dy < 0) { return (uint8_t)(128 + raw); }            /* Q3 */
        return (uint8_t)(256 - raw);                                       /* Q4 */
    } else {
        raw = adx * 64 / ady;  /* 0..63 */
        if (dx >= 0 && dy >= 0) { return (uint8_t)(64 - raw); }           /* Q1 */
        if (dx < 0 && dy >= 0) { return (uint8_t)(64 + raw); }            /* Q2 */
        if (dx < 0 && dy < 0) { return (uint8_t)(192 - raw); }            /* Q3 */
        return (uint8_t)(192 + raw);                                       /* Q4 */
    }
}

/* ── 绘制圆盘（静态，不旋转） ── */

static void draw_disk(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, DISK_CX - 24, DISK_CY - 24, 48, 48, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_hardware.lcd, DISK_CX - 18, DISK_CY - 18, 36, 36, COLOR_MID);
    Game_Graphics_Fill_Rect(g_hardware.lcd, DISK_CX - 10, DISK_CY - 10, 20, 20, COLOR_LIGHT);
    Game_Graphics_Fill_Rect(g_hardware.lcd, DISK_CX - 2, DISK_CY - 2, 4, 4, COLOR_WHITE);
}

/* ── 绘制/擦除针尖 ── */

static void draw_tip_at_angle(uint8_t angle, uint16_t color) {
    int16_t tx, ty;
    pos_from_angle(angle, &tx, &ty);
    Game_Graphics_Fill_Rect(g_hardware.lcd, tx - NEEDLE_SIZE / 2, ty - NEEDLE_SIZE / 2,
        NEEDLE_SIZE, NEEDLE_SIZE, color);
}

/* ── 绘制所有针（旋转后） ── */

static void redraw_all_needles(void) {
    uint8_t i;
    /* 擦旧 */
    for (i = 0; i < g_needle_count; i++) {
        draw_tip_at_angle(g_prev_angles[i], COLOR_BLACK);
    }
    /* 画新 */
    for (i = 0; i < g_needle_count; i++) {
        const uint8_t abs_angle = (uint8_t)(g_needle_angles[i] + g_disk_angle);
        g_prev_angles[i] = abs_angle;
        draw_tip_at_angle(abs_angle, g_needle_colors[i % 7]);
    }
}

/* ── 画/擦飞行针 ── */

static void draw_fly_tip(uint16_t color) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, g_fly_x - 2, g_fly_y - 2, 4, 4, color);
}

/* ── UI ── */

static void draw_bars(void) {
    St7789* l = g_hardware.lcd;
    bar_fill(0, 0, SCREEN_WIDTH, BAR_H, COLOR_BLACK);
    Game_Graphics_Draw_Text(l, 2, 2, "NEEDLE", 1, COLOR_WHITE);
    bar_fill(0, BAR_H - 1, SCREEN_WIDTH, 1, COLOR_DARK);
    bar_fill(0, BAR_BOT, SCREEN_WIDTH, SCREEN_HEIGHT - BAR_BOT, COLOR_BLACK);
    bar_fill(0, BAR_BOT, SCREEN_WIDTH, 1, COLOR_DARK);
    Game_Graphics_Draw_Text(l, 56, BAR_BOT + 3, "HOLD TO BACK", 1, COLOR_GRAY);
}

static void draw_score(void) {
    bar_fill(SCREEN_WIDTH - 48, 2, 48, 8, COLOR_BLACK);
    Game_Graphics_Draw_U32(g_hardware.lcd, SCREEN_WIDTH - 48, 2, g_score, 5, 1, COLOR_CYAN);
}

/* ── 刷新底栏提示 ── */

static void draw_bottom_text(const char* s, uint16_t color) {
    bar_fill(0, LAUNCH_Y, SCREEN_WIDTH, SCREEN_HEIGHT - LAUNCH_Y, COLOR_BLACK);
    bar_fill(0, BAR_BOT, SCREEN_WIDTH, 1, COLOR_DARK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 40, BAR_BOT + 3, s, 1, color);
}

/* ── 重启 ── */

static void restart_game(void) {
    g_state = needle_state_ready;
    g_needle_count = 0;
    g_disk_angle = 0;
    g_ang_vel = START_ANG_VEL;
    g_launch_x = DISK_CX;
    g_score = 0;

    /* 初始 4 根针，均匀分布 */
    g_needle_angles[0] = 0;
    g_needle_angles[1] = 64;
    g_needle_angles[2] = 128;
    g_needle_angles[3] = 192;
    g_needle_count = 4;
    for (uint8_t i = 0; i < g_needle_count; i++) {
        g_prev_angles[i] = g_needle_angles[i];
    }

    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_bars();
    draw_disk();
    redraw_all_needles();
    draw_score();
    draw_bottom_text("PRESS TO LAUNCH", COLOR_WHITE);

    Buzzer_Play_Music(g_hardware.buzzer, music_idx_racing_theme, 1);
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
    const int32_t dy = DISK_CY - LAUNCH_Y;  /* 负值：向上 */
    /* 缩放方向向量到 FLY_SPEED 长度 */
    if (dx == 0 && dy == 0) {
        g_fly_dx = 0;
        g_fly_dy = FLY_SPEED;
        g_fly_x = g_launch_x;
        g_fly_y = LAUNCH_Y;
        g_state = needle_state_flying;
        return;
    }
    /* 用 dx, dy 的比例计算各分量（dy 主导，因为垂直距离大） */
    g_fly_dy = -FLY_SPEED;
    g_fly_dx = (int16_t)(dx * FLY_SPEED / (-dy));
    g_fly_x = g_launch_x;
    g_fly_y = LAUNCH_Y;
    g_state = needle_state_flying;

    /* 清除底部提示 */
    draw_bottom_text("", COLOR_BLACK);
}

/* ── Update ── */

Game_result Needle_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    /* ══ 每帧：旋转圆盘 ══ */
    g_disk_angle = (uint8_t)(g_disk_angle + g_ang_vel);
    if (g_needle_count > 0) {
        redraw_all_needles();
    }

    St7789* lcd = g_hardware.lcd;
    (void)lcd;

    /* ── Game Over ── */
    if (g_state == needle_state_over) {
        if (input->direction_pressed) {
            Buzzer_Stop(g_hardware.buzzer);
            restart_game();
        }
        return game_result_running;
    }

    /* ── Flying：移动飞行针 + 碰撞检测 ── */
    if (g_state == needle_state_flying) {
        /* 擦旧 */
        draw_fly_tip(COLOR_BLACK);

        /* 移动 */
        g_fly_x += g_fly_dx;
        g_fly_y += g_fly_dy;

        /* 画新 */
        draw_fly_tip(COLOR_WHITE);

        /* 检查是否到达圆盘附近 */
        const int32_t dx = g_fly_x - DISK_CX;
        const int32_t dy = g_fly_y - DISK_CY;
        const int32_t dist2 = dx * dx + dy * dy;

        if (dist2 <= (TIP_R + 6) * (TIP_R + 6)) {
            /* 计算飞行针接近圆心的角度 */
            const uint8_t fly_angle = atan2_approx(dx, dy);

            /* 碰撞检测 */
            uint8_t collision = 0;
            for (uint8_t i = 0; i < g_needle_count; i++) {
                const uint8_t needle_abs = (uint8_t)(g_needle_angles[i] + g_disk_angle);
                int16_t diff = (int16_t)fly_angle - (int16_t)needle_abs;
                if (diff < 0) { diff = (int16_t)(-diff); }
                if (diff > 128) { diff = (int16_t)(256 - diff); }
                if (diff < COLLIDE_ANGLE) { collision = 1; break; }
            }

            if (collision || g_needle_count >= MAX_NEEDLES) {
                g_state = needle_state_over;
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_life_lost);
                Buzzer_Play_Music(g_hardware.buzzer, music_idx_defeat, 0);
                draw_bottom_text("GAME OVER  PUSH RESTART", COLOR_RED);
            } else {
                /* 成功插入 */
                g_needle_angles[g_needle_count] = (uint8_t)((fly_angle - g_disk_angle) & 0xFFu);
                g_prev_angles[g_needle_count] = fly_angle;
                g_needle_count++;
                g_score++;
                draw_score();

                if ((g_score % 5) == 0 && g_ang_vel < 8) { g_ang_vel++; }

                g_state = needle_state_ready;
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_menu_select);
                draw_bottom_text("PRESS TO LAUNCH", COLOR_WHITE);
            }
        }
        return game_result_running;
    }

    /* ── Ready：输入处理 ── */
    if (g_state == needle_state_ready) {
        /* 摇杆左右摆动发射位置 */
        if (input->direction == game_direction_left && g_launch_x > 30) {
            g_launch_x -= 4;
            draw_bottom_text("PRESS TO LAUNCH", COLOR_WHITE);
        } else if (input->direction == game_direction_right && g_launch_x < 210) {
            g_launch_x += 4;
            draw_bottom_text("PRESS TO LAUNCH", COLOR_WHITE);
        }

        if (input->confirm_pressed) {
            draw_bottom_text("", COLOR_BLACK);
            launch_needle();
        }
    }

    return game_result_running;
}

/* ── Score / Finished ── */

uint32_t Needle_Get_Score(void) { return g_score; }

uint8_t Needle_Is_Finished(void) { return g_state == needle_state_over; }
