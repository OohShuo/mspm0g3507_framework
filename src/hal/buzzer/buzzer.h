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
    music_idx_menu_theme = 0,
    music_idx_pacman_theme,
    music_idx_snake_theme,
    music_idx_racing_theme,
    music_idx_tank_theme,
    music_idx_victory,
    music_idx_defeat,
    music_idx_count
} Music_idx;

typedef enum {
    buzzer_sfx_menu_move = 0,
    buzzer_sfx_menu_select,
    buzzer_sfx_pellet,
    buzzer_sfx_power,
    buzzer_sfx_ghost,
    buzzer_sfx_snake_eat,
    buzzer_sfx_lane_change,
    buzzer_sfx_overtake,
    buzzer_sfx_tank_fire,
    buzzer_sfx_tank_hit,
    buzzer_sfx_explosion,
    buzzer_sfx_life_lost,
    buzzer_sfx_count
} Buzzer_sfx_idx;

typedef struct {
    uint8_t pwm_idx;
} Buzzer_config;

typedef struct Buzzer_t Buzzer;

extern const Music music_library[music_idx_count];
extern const Music buzzer_sfx_library[buzzer_sfx_count];

void Buzzer_Init(void);
Buzzer* Buzzer_Create(const Buzzer_config* config);

void Buzzer_Play_Music(Buzzer* obj, Music_idx music, uint8_t is_loop);
void Buzzer_Play_Sfx(Buzzer* obj, Buzzer_sfx_idx sfx);
void Buzzer_Play(Buzzer* obj, const Music* music, uint8_t is_loop);
void Buzzer_Stop(Buzzer* obj);
void Buzzer_Update_All(void);
