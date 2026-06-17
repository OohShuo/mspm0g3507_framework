#pragma once

#include <stdint.h>

#define BUZZER_NOTE_GLISSANDO 0x01u

typedef struct {
    uint16_t frequency_hz;
    uint16_t duration_ms;
    uint8_t gate_percent;
    uint8_t volume_percent;
    uint8_t flags;
} Buzzer_note;

typedef struct Music_t {
    const Buzzer_note* notes;
    uint16_t length;
} Music;

typedef enum {
    /* ── 通用 / 菜单 ── */
    buzzer_sfx_menu_move = 0,
    buzzer_sfx_menu_select,

    /* ── Pac‑Man ── */
    buzzer_sfx_pellet,
    buzzer_sfx_power,
    buzzer_sfx_ghost,
    buzzer_sfx_pacman_waka,

    /* ── Snake ── */
    buzzer_sfx_snake_eat,
    buzzer_sfx_snake_turn,
    buzzer_sfx_snake_grow,

    /* ── Racing ── */
    buzzer_sfx_lane_change,
    buzzer_sfx_overtake,
    buzzer_sfx_racing_crash,

    /* ── Tank Battle ── */
    buzzer_sfx_tank_fire,
    buzzer_sfx_tank_hit,

    /* ── Air Battle ── */
    buzzer_sfx_air_fire,
    buzzer_sfx_air_hit,
    buzzer_sfx_air_pickup,
    buzzer_sfx_boss_alert,

    /* ── Tetris ── */
    buzzer_sfx_tetris_move,
    buzzer_sfx_tetris_rotate,
    buzzer_sfx_tetris_lock,
    buzzer_sfx_tetris_line_clear,
    buzzer_sfx_tetris_tetris,

    /* ── Breakout ── */
    buzzer_sfx_breakout_bounce,
    buzzer_sfx_breakout_brick,

    /* ── Pong ── */
    buzzer_sfx_pong_paddle,
    buzzer_sfx_pong_wall,
    buzzer_sfx_pong_score,

    /* ── Gomoku ── */
    buzzer_sfx_gomoku_place,

    /* ── 2048 ── */
    buzzer_sfx_slide,
    buzzer_sfx_merge,

    /* ── Dino Runner ── */
    buzzer_sfx_dino_jump,
    buzzer_sfx_dino_duck,

    /* ── Flappy Bird ── */
    buzzer_sfx_flappy_flap,
    buzzer_sfx_flappy_score,

    /* ── Maze ── */
    buzzer_sfx_maze_move,
    buzzer_sfx_maze_goal,

    /* ── Needle ── */
    buzzer_sfx_needle_launch,
    buzzer_sfx_needle_stick,

    /* ── 通用结局 ── */
    buzzer_sfx_explosion,
    buzzer_sfx_life_lost,
    buzzer_sfx_victory,
    buzzer_sfx_defeat,

    buzzer_sfx_count
} Buzzer_sfx_idx;

typedef struct {
    uint8_t pwm_idx;
} Buzzer_config;

typedef struct Buzzer_t Buzzer;

extern const Music buzzer_sfx_library[buzzer_sfx_count];

void Buzzer_Init(void);
Buzzer* Buzzer_Create(const Buzzer_config* config);

void Buzzer_Play_Sfx(Buzzer* obj, Buzzer_sfx_idx sfx);
void Buzzer_Play(Buzzer* obj, const Music* music);
void Buzzer_Stop(Buzzer* obj);
void Buzzer_Update_All(void);
void Buzzer_Set_Volume(Buzzer* obj, uint8_t volume_percent);
uint8_t Buzzer_Get_Volume(Buzzer* obj);
