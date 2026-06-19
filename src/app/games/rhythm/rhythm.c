#include "rhythm.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define BAR_H         12
#define BAR_BOT       298
#define PLAY_TOP      BAR_H
#define PLAY_BOT      BAR_BOT
#define HIT_Y         246
#define NOTE_SPAWN_Y  48
#define FEEDBACK_Y    24
#define NOTE_COUNT    5u

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_YELLOW  0xffe0u
#define COLOR_GREEN   0x07e0u
#define COLOR_RED     0xf800u
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0x4208u

typedef enum { rhythm_state_ready, rhythm_state_playing, rhythm_state_over } Rhythm_state;
typedef struct {
    int16_t y;
    uint8_t dir;
    uint8_t active;
} Note;

static Game_hardware g_hardware;
static Rhythm_state g_state;
static Note g_notes[NOTE_COUNT];
static uint32_t g_score;
static uint32_t g_rand_state;
static uint32_t g_last_tick_ms;
static uint32_t g_spawn_ms;
static uint16_t g_combo;
static uint16_t g_speed;
static uint16_t g_spawn_interval;
static uint16_t g_move_acc;
static uint8_t g_lives;
static uint8_t g_need_center;
static uint8_t g_flash_frames;
static uint8_t g_feedback_frames;
static uint8_t g_feedback_kind;
static Buzzer_note g_tone_notes[3];
static Music g_tone_music = {g_tone_notes, 0};
static const int16_t g_lane_x[4] = {42, 94, 146, 198};
static const uint16_t g_dir_tone[5] = {0, 784, 523, 659, 1047};

static uint32_t fast_rand(void) {
    g_rand_state = g_rand_state * 1103515245u + 12345u;
    return (g_rand_state >> 16) & 0x7fffu;
}

static void fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) {
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

static void draw_arrow(int16_t x, int16_t y, uint8_t dir, uint16_t color) {
    fill(x - 10, y - 10, 21, 21, COLOR_BLACK);
    if (color == COLOR_BLACK) { return; }
    if (dir == game_direction_up) {
        fill(x - 3, y - 1, 7, 12, color);
        for (int8_t i = 0; i < 6; i++) { fill(x - i, y - 7 + i, i * 2 + 1, 1, color); }
    } else if (dir == game_direction_down) {
        fill(x - 3, y - 10, 7, 12, color);
        for (int8_t i = 0; i < 6; i++) { fill(x - i, y + 7 - i, i * 2 + 1, 1, color); }
    } else if (dir == game_direction_left) {
        fill(x - 1, y - 3, 12, 7, color);
        for (int8_t i = 0; i < 6; i++) { fill(x - 7 + i, y - i, 1, i * 2 + 1, color); }
    } else {
        fill(x - 10, y - 3, 12, 7, color);
        for (int8_t i = 0; i < 6; i++) { fill(x + 7 - i, y - i, 1, i * 2 + 1, color); }
    }
}

static void restore_lane_ui(uint8_t dir, int16_t y) {
    const int16_t x = g_lane_x[dir - 1u];
    int16_t top = (int16_t)(y - 10);
    int16_t bottom = (int16_t)(y + 10);
    if (top < PLAY_TOP + 5) { top = PLAY_TOP + 5; }
    if (bottom > PLAY_BOT - 5) { bottom = PLAY_BOT - 5; }
    if (top <= bottom) {
        fill(x - 16, top, 1, bottom - top + 1, COLOR_DARK);
        fill(x + 16, top, 1, bottom - top + 1, COLOR_DARK);
    }
    if (y + 10 >= HIT_Y - 10 && y - 10 <= HIT_Y + 10) {
        draw_arrow(x, HIT_Y, dir, COLOR_DARK);
        fill(20, HIT_Y + 16, SCREEN_WIDTH - 40, 2, COLOR_CYAN);
    }
}

static void draw_static_scene(void) {
    fill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 2, 2, "RHYTHM", 1, COLOR_WHITE);
    fill(0, BAR_H - 1, SCREEN_WIDTH, 1, COLOR_DARK);
    fill(0, BAR_BOT, SCREEN_WIDTH, 1, COLOR_DARK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 56, BAR_BOT + 3, "HOLD TO BACK", 1, COLOR_GRAY);
    for (uint8_t i = 0; i < 4u; i++) {
        fill(g_lane_x[i] - 16, PLAY_TOP + 5, 1, PLAY_BOT - PLAY_TOP - 10, COLOR_DARK);
        fill(g_lane_x[i] + 16, PLAY_TOP + 5, 1, PLAY_BOT - PLAY_TOP - 10, COLOR_DARK);
        draw_arrow(g_lane_x[i], HIT_Y, (uint8_t)(i + 1u), COLOR_DARK);
    }
    fill(20, HIT_Y + 16, SCREEN_WIDTH - 40, 2, COLOR_CYAN);
}

static void draw_hud(void) {
    fill(88, 2, 152, 8, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 88, 2, "C", 1, COLOR_GRAY);
    Game_Graphics_Draw_U32(g_hardware.lcd, 100, 2, g_combo, 3, 1, COLOR_CYAN);
    Game_Graphics_Draw_Text(g_hardware.lcd, 132, 2, "L", 1, COLOR_GRAY);
    Game_Graphics_Draw_U32(g_hardware.lcd, 144, 2, g_lives, 1, 1, COLOR_RED);
    Game_Graphics_Draw_U32(g_hardware.lcd, 184, 2, g_score, 5, 1, COLOR_WHITE);
}

static void play_tone(uint8_t dir, uint8_t kind) {
    uint16_t base = g_dir_tone[dir];
    if (base == 0u) { base = 659u; }
    g_tone_notes[0] = (Buzzer_note){base, 34, 65, 55, 0};
    g_tone_notes[1] = (Buzzer_note){
        (uint16_t)(kind == 1u ? base + base / 2u : kind == 4u ? base * 2u : base + base / 4u), 46, 72, 62, 0};
    g_tone_music.length = 2;
    if (kind == 4u) {
        g_tone_notes[2] = (Buzzer_note){(uint16_t)(base * 2u + base / 2u), 60, 78, 70, 0};
        g_tone_music.length = 3;
    }
    Buzzer_Play(g_hardware.buzzer, &g_tone_music);
}

static void show_feedback(uint8_t kind) {
    g_feedback_kind = kind;
    g_feedback_frames = 18;
}

static void draw_feedback(void) {
    fill(36, FEEDBACK_Y, 168, 30, COLOR_BLACK);
    if (g_feedback_frames == 0u) { return; }
    if (g_feedback_kind == 1u) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 78, FEEDBACK_Y + 4, "PERFECT", 2, COLOR_CYAN);
    } else if (g_feedback_kind == 2u) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 96, FEEDBACK_Y + 4, "GOOD", 2, COLOR_GREEN);
    } else if (g_feedback_kind == 3u) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 96, FEEDBACK_Y + 4, "MISS", 2, COLOR_RED);
    } else {
        Game_Graphics_Draw_Text(g_hardware.lcd, 54, FEEDBACK_Y + 4, "COMBO", 2, COLOR_YELLOW);
        Game_Graphics_Draw_U32(g_hardware.lcd, 130, FEEDBACK_Y + 4, g_combo, 3, 2, COLOR_WHITE);
    }
    g_feedback_frames--;
}

static void start_game(void) {
    g_state = rhythm_state_playing;
    g_score = 0;
    g_combo = 0;
    g_lives = 5;
    g_speed = 140;
    g_spawn_interval = 780;
    g_move_acc = 0;
    g_spawn_ms = 0;
    g_need_center = 0;
    g_flash_frames = 0;
    g_feedback_frames = 0;
    g_feedback_kind = 0;
    g_last_tick_ms = Bsp_Get_Tick_Ms();
    for (uint8_t i = 0; i < NOTE_COUNT; i++) { g_notes[i].active = 0; }
    draw_static_scene();
    draw_hud();
}

static void restart_game(void) {
    g_state = rhythm_state_ready;
    g_rand_state = Bsp_Get_Tick_Ms();
    draw_static_scene();
    Game_Graphics_Draw_Text(g_hardware.lcd, 46, 140, "PUSH TO START", 1, COLOR_WHITE);
}

static void spawn_note(void) {
    for (uint8_t i = 0; i < NOTE_COUNT; i++) {
        if (g_notes[i].active) { continue; }
        g_notes[i].active = 1;
        g_notes[i].dir = (uint8_t)(1u + (fast_rand() & 3u));
        g_notes[i].y = NOTE_SPAWN_Y;
        return;
    }
}

static void miss_note(uint8_t i) {
    draw_arrow(g_lane_x[g_notes[i].dir - 1u], g_notes[i].y, g_notes[i].dir, COLOR_BLACK);
    restore_lane_ui(g_notes[i].dir, g_notes[i].y);
    g_notes[i].active = 0;
    g_combo = 0;
    if (g_lives > 0) { g_lives--; }
    g_flash_frames = 4;
    show_feedback(3u);
    Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_life_lost);
}

static void finish_game(void) {
    g_state = rhythm_state_over;
    Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_defeat);
    fill(45, 146, 150, 34, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 70, 150, "GAME OVER", 1, COLOR_RED);
    Game_Graphics_Draw_Text(g_hardware.lcd, 46, 168, "PUSH TO RESTART", 1, COLOR_WHITE);
}

void Rhythm_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

Game_result Rhythm_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state == rhythm_state_over) {
        if (input->direction_pressed) { restart_game(); }
        return game_result_running;
    }
    if (g_state == rhythm_state_ready) {
        if (input->direction_pressed || input->confirm_pressed) { start_game(); }
        return game_result_running;
    }

    const uint32_t now = Bsp_Get_Tick_Ms();
    uint32_t elapsed = now - g_last_tick_ms;
    if (elapsed > 60u) { elapsed = 60u; }
    g_last_tick_ms = now;
    g_spawn_ms += elapsed;

    if (g_spawn_ms >= g_spawn_interval) {
        g_spawn_ms = 0;
        spawn_note();
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_menu_move);
    }

    if (input->direction == game_direction_none) { g_need_center = 0; }
    if (input->direction_pressed && !g_need_center) {
        int16_t best_delta = 999;
        uint8_t best = NOTE_COUNT;
        for (uint8_t i = 0; i < NOTE_COUNT; i++) {
            if (!g_notes[i].active || g_notes[i].dir != input->direction) { continue; }
            int16_t delta = g_notes[i].y - HIT_Y;
            if (delta < 0) { delta = (int16_t)-delta; }
            if (delta < best_delta) {
                best_delta = delta;
                best = i;
            }
        }
        if (best < NOTE_COUNT && best_delta <= 30) {
            draw_arrow(g_lane_x[g_notes[best].dir - 1u], g_notes[best].y, g_notes[best].dir, COLOR_BLACK);
            restore_lane_ui(g_notes[best].dir, g_notes[best].y);
            g_notes[best].active = 0;
            g_combo++;
            g_score += best_delta <= 12 ? 100u + g_combo * 2u : 50u + g_combo;
            if (g_speed < 300u) { g_speed += 3u; }
            if (g_spawn_interval > 360u && (g_combo & 3u) == 0u) { g_spawn_interval -= 20u; }
            g_flash_frames = best_delta <= 12 ? 3u : 1u;
            const uint8_t feedback = (g_combo % 10u) == 0u ? 4u : (best_delta <= 12 ? 1u : 2u);
            show_feedback(feedback);
            play_tone(input->direction, feedback);
        } else {
            g_combo = 0;
            if (g_lives > 0) { g_lives--; }
            g_flash_frames = 4;
            show_feedback(3u);
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_life_lost);
        }
        g_need_center = 1;
    }

    g_move_acc = (uint16_t)(g_move_acc + elapsed * g_speed);
    const int16_t move_px = (int16_t)(g_move_acc / 1000u);
    g_move_acc = (uint16_t)(g_move_acc % 1000u);

    for (uint8_t i = 0; i < NOTE_COUNT; i++) {
        if (!g_notes[i].active) { continue; }
        const int16_t old_y = g_notes[i].y;
        g_notes[i].y = (int16_t)(g_notes[i].y + move_px);
        if (g_notes[i].y != old_y) {
            draw_arrow(g_lane_x[g_notes[i].dir - 1u], old_y, g_notes[i].dir, COLOR_BLACK);
            restore_lane_ui(g_notes[i].dir, old_y);
            draw_arrow(g_lane_x[g_notes[i].dir - 1u], g_notes[i].y, g_notes[i].dir, COLOR_YELLOW);
        }
        if (g_notes[i].y > HIT_Y + 42) { miss_note(i); }
    }

    if (g_flash_frames > 0u) {
        const uint16_t color = g_combo > 0 ? COLOR_GREEN : COLOR_RED;
        fill(20, HIT_Y + 16, SCREEN_WIDTH - 40, 2, color);
        g_flash_frames--;
        if (g_flash_frames == 0u) { fill(20, HIT_Y + 16, SCREEN_WIDTH - 40, 2, COLOR_CYAN); }
    }
    draw_feedback();
    draw_hud();

    if (g_lives == 0u) { finish_game(); }
    return game_result_running;
}

uint32_t Rhythm_Get_Score(void) { return g_score; }
uint8_t Rhythm_Is_Finished(void) { return g_state == rhythm_state_over; }
