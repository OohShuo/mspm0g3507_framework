#include "buzzer.h"

#define C4  262
#define D4  294
#define E4  330
#define F4  349
#define G4  392
#define A4  440
#define B4  494
#define C5  523
#define CS5 554
#define D5  587
#define DS5 622
#define E5  659
#define F5  698
#define FS5 740
#define G5  784
#define GS5 831
#define A5  880
#define AS5 932
#define B5  988
#define C6  1047
#define D6  1175
#define E6  1319
#define F6  1397
#define FS6 1480
#define G6  1568
#define A6  1760
#define B6  1976
#define C7  2093

#define LEN(a) ((uint16_t)(sizeof(a) / sizeof((a)[0])))
#define N(f, ms)       {(f), (ms), 78, 72, 0}
#define S(f, ms)       {(f), (ms), 48, 75, 0}
#define L(f, ms)       {(f), (ms), 94, 68, 0}
#define V(f, ms, vol)  {(f), (ms), 72, (vol), 0}
#define G(f, ms)       {(f), (ms), 96, 72, BUZZER_NOTE_GLISSANDO}
#define R(ms)          {0, (ms), 0, 0, 0}

/* Bright, relaxed game-selection loop. */
static const Buzzer_note menu_theme[] = {
    N(C5, 150), N(E5, 150), N(G5, 150), N(C6, 300), R(75),
    N(B5, 150), N(G5, 150), N(E5, 150), N(D5, 300), R(75),
    N(F5, 150), N(A5, 150), N(C6, 150), N(A5, 300), R(75),
    N(G5, 150), N(E5, 150), N(D5, 150), L(C5, 450), R(150),
    N(E5, 150), N(G5, 150), N(B5, 150), N(E6, 300), R(75),
    N(D6, 150), N(B5, 150), N(G5, 150), L(C6, 450), R(225),
};

/* Quick chromatic chase motif for Pac-Man. */
static const Buzzer_note pacman_theme[] = {
    S(C5, 90), S(E5, 90), S(G5, 90), S(B5, 90), S(C6, 180), R(45),
    S(B5, 90), S(G5, 90), S(E5, 90), S(D5, 90), S(F5, 180), R(45),
    S(D5, 90), S(F5, 90), S(A5, 90), S(C6, 90), S(D6, 180), R(45),
    S(C6, 90), S(A5, 90), S(F5, 90), S(E5, 90), L(C5, 270), R(90),
    S(C5, 90), S(C6, 90), S(B5, 90), S(G5, 90), S(E5, 180), R(45),
    S(F5, 90), S(FS5, 90), S(G5, 90), S(B5, 90), L(C6, 270), R(90),
};

/* Smooth, winding melody for Snake. */
static const Buzzer_note snake_theme[] = {
    L(E5, 220), L(G5, 220), L(A5, 330), N(G5, 110), L(E5, 220),
    L(D5, 220), L(E5, 330), R(110),
    L(G5, 220), L(A5, 220), L(C6, 330), N(B5, 110), L(A5, 220),
    L(G5, 220), L(E5, 330), R(110),
    L(D5, 220), L(F5, 220), L(A5, 330), N(G5, 110), L(F5, 220),
    L(E5, 220), G(D5, 330), L(E5, 440), R(220),
};

/* Fast pulse with a rising finish for Racing. */
static const Buzzer_note racing_theme[] = {
    S(E5, 85), S(B5, 85), S(E6, 85), S(B5, 85),
    S(FS5, 85), S(C6, 85), S(FS6, 85), S(C6, 85),
    S(G5, 85), S(D6, 85), S(G6, 85), S(D6, 85),
    S(A5, 85), S(E6, 85), S(A6, 85), S(E6, 85),
    S(G5, 85), S(D6, 85), S(B5, 85), S(D6, 85),
    S(FS5, 85), S(C6, 85), S(A5, 85), S(C6, 85),
    S(E5, 85), S(G5, 85), S(B5, 85), S(E6, 85),
    G(E6, 170), L(A6, 255), R(85),
};

/* Deliberate march for Tank Battle. */
static const Buzzer_note tank_theme[] = {
    S(E4, 180), S(E4, 90), N(G4, 180), N(A4, 270), R(90),
    S(E4, 180), S(G4, 90), N(B4, 180), N(A4, 270), R(90),
    S(D4, 180), S(D4, 90), N(F4, 180), N(G4, 270), R(90),
    S(E4, 180), N(G4, 180), N(B4, 180), L(E5, 360), R(180),
    S(C5, 180), S(B4, 90), N(A4, 180), N(G4, 270), R(90),
    S(F4, 180), N(G4, 180), N(A4, 180), L(E4, 360), R(180),
};

/* Open, climbing arpeggios for Air Battle. */
static const Buzzer_note air_theme[] = {
    S(E5, 100), S(G5, 100), S(B5, 100), N(E6, 200), S(B5, 100), S(G5, 100),
    S(FS5, 100), S(A5, 100), S(C6, 100), N(FS6, 200), S(C6, 100), S(A5, 100),
    S(G5, 100), S(B5, 100), S(D6, 100), N(G6, 200), S(D6, 100), S(B5, 100),
    N(A5, 150), N(C6, 150), N(E6, 150), L(A6, 300), R(100),
    S(G5, 100), S(A5, 100), S(B5, 100), N(E6, 200), S(D6, 100), S(B5, 100),
    N(FS5, 150), N(A5, 150), N(C6, 150), L(E6, 300), R(100),
};

static const Buzzer_note victory_theme[] = {
    N(C5, 110), N(E5, 110), N(G5, 110), N(C6, 220),
    N(E6, 110), N(G6, 110), L(C7, 500), R(120),
};

static const Buzzer_note defeat_theme[] = {
    G(E6, 180), G(C6, 180), G(A5, 220), G(F5, 240),
    G(D5, 300), L(C5, 520), R(100),
};

static const Buzzer_note sfx_menu_move[] = {
    S(C6, 35), S(E6, 45),
};
static const Buzzer_note sfx_menu_select[] = {
    S(C6, 45), S(G6, 55), N(C7, 90),
};
static const Buzzer_note sfx_pellet[] = {
    S(B5, 28), S(E6, 32),
};
static const Buzzer_note sfx_power[] = {
    G(C5, 90), G(G5, 90), N(C6, 120),
};
static const Buzzer_note sfx_ghost[] = {
    S(C6, 45), S(E6, 45), S(G6, 45), N(C7, 80),
};
static const Buzzer_note sfx_snake_eat[] = {
    S(E6, 45), N(A6, 70),
};
static const Buzzer_note sfx_lane_change[] = {
    G(G5, 65), N(C6, 45),
};
static const Buzzer_note sfx_overtake[] = {
    S(A5, 35), S(C6, 35), N(E6, 55),
};
static const Buzzer_note sfx_tank_fire[] = {
    V(G5, 35, 90), G(D5, 55),
};
static const Buzzer_note sfx_tank_hit[] = {
    V(C5, 45, 90), V(G4, 70, 85),
};
static const Buzzer_note sfx_air_fire[] = {
    V(C7, 18, 65), G(E6, 24),
};
static const Buzzer_note sfx_air_pickup[] = {
    S(E6, 40), S(A6, 45), N(C7, 70),
};
static const Buzzer_note sfx_boss_alert[] = {
    V(C5, 120, 95), R(45), V(C5, 120, 95), R(45), V(G4, 220, 100),
};
static const Buzzer_note sfx_explosion[] = {
    V(C5, 45, 100), G(G4, 85), G(C4, 130),
};
static const Buzzer_note sfx_life_lost[] = {
    G(E6, 90), G(B5, 110), L(E5, 180),
};

const Music music_library[music_idx_count] = {
    [music_idx_menu_theme] = {menu_theme, LEN(menu_theme)},
    [music_idx_pacman_theme] = {pacman_theme, LEN(pacman_theme)},
    [music_idx_snake_theme] = {snake_theme, LEN(snake_theme)},
    [music_idx_racing_theme] = {racing_theme, LEN(racing_theme)},
    [music_idx_tank_theme] = {tank_theme, LEN(tank_theme)},
    [music_idx_air_theme] = {air_theme, LEN(air_theme)},
    [music_idx_victory] = {victory_theme, LEN(victory_theme)},
    [music_idx_defeat] = {defeat_theme, LEN(defeat_theme)},
};

const Music buzzer_sfx_library[buzzer_sfx_count] = {
    [buzzer_sfx_menu_move] = {sfx_menu_move, LEN(sfx_menu_move)},
    [buzzer_sfx_menu_select] = {sfx_menu_select, LEN(sfx_menu_select)},
    [buzzer_sfx_pellet] = {sfx_pellet, LEN(sfx_pellet)},
    [buzzer_sfx_power] = {sfx_power, LEN(sfx_power)},
    [buzzer_sfx_ghost] = {sfx_ghost, LEN(sfx_ghost)},
    [buzzer_sfx_snake_eat] = {sfx_snake_eat, LEN(sfx_snake_eat)},
    [buzzer_sfx_lane_change] = {sfx_lane_change, LEN(sfx_lane_change)},
    [buzzer_sfx_overtake] = {sfx_overtake, LEN(sfx_overtake)},
    [buzzer_sfx_tank_fire] = {sfx_tank_fire, LEN(sfx_tank_fire)},
    [buzzer_sfx_tank_hit] = {sfx_tank_hit, LEN(sfx_tank_hit)},
    [buzzer_sfx_air_fire] = {sfx_air_fire, LEN(sfx_air_fire)},
    [buzzer_sfx_air_pickup] = {sfx_air_pickup, LEN(sfx_air_pickup)},
    [buzzer_sfx_boss_alert] = {sfx_boss_alert, LEN(sfx_boss_alert)},
    [buzzer_sfx_explosion] = {sfx_explosion, LEN(sfx_explosion)},
    [buzzer_sfx_life_lost] = {sfx_life_lost, LEN(sfx_life_lost)},
};
