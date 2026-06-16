#include "buzzer.h"

#define C4            262
#define D4            294
#define E4            330
#define F4            349
#define G4            392
#define A4            440
#define B4            494
#define C5            523
#define CS5           554
#define D5            587
#define DS5           622
#define E5            659
#define F5            698
#define FS5           740
#define G5            784
#define GS5           831
#define A5            880
#define AS5           932
#define B5            988
#define C6            1047
#define D6            1175
#define E6            1319
#define F6            1397
#define FS6           1480
#define G6            1568
#define A6            1760
#define B6            1976
#define C7            2093
#define D7            2349
#define E7            2637
#define F7            2794
#define G7            3136
#define A7            3520

#define LEN(a)        ((uint16_t)(sizeof(a) / sizeof((a)[0])))
#define N(f, ms)      {(f), (ms), 78, 72, 0}
#define S(f, ms)      {(f), (ms), 48, 75, 0}
#define L(f, ms)      {(f), (ms), 94, 68, 0}
#define V(f, ms, vol) {(f), (ms), 72, (vol), 0}
#define G(f, ms)      {(f), (ms), 96, 72, BUZZER_NOTE_GLISSANDO}
#define R(ms)         {0, (ms), 0, 0, 0}

/*
 *  SFX 库 — 所有音效，music_library 已删除
 *  每个 SFX 1~8 个音符，在玩家操作时一次性播放
 */

/* ── 通用 / 菜单 ── */
static const Buzzer_note sfx_menu_move[] = {S(C6, 35), S(E6, 45)};
static const Buzzer_note sfx_menu_select[] = {S(C6, 45), S(G6, 55), N(C7, 90)};

/* ── Pac-Man ── */
static const Buzzer_note sfx_pellet[] = {S(B5, 28), S(E6, 32)};
static const Buzzer_note sfx_power[] = {G(C5, 90), G(G5, 90), N(C6, 120)};
static const Buzzer_note sfx_ghost[] = {S(C6, 45), S(E6, 45), S(G6, 45), N(C7, 80)};
static const Buzzer_note sfx_pacman_waka[] = {S(E5, 36), S(G5, 44)};

/* ── Snake ── */
static const Buzzer_note sfx_snake_eat[] = {S(E6, 45), N(A6, 70)};
static const Buzzer_note sfx_snake_turn[] = {S(C6, 30), G(C6, 25)};
static const Buzzer_note sfx_snake_grow[] = {S(C6, 35), S(E6, 40), S(G6, 50)};

/* ── Racing ── */
static const Buzzer_note sfx_lane_change[] = {G(G5, 65), N(C6, 45)};
static const Buzzer_note sfx_overtake[] = {S(A5, 35), S(C6, 35), N(E6, 55)};
static const Buzzer_note sfx_racing_crash[] = {V(C5, 55, 90), G(G4, 80), L(C4, 140)};

/* ── Tank Battle ── */
static const Buzzer_note sfx_tank_fire[] = {V(G5, 35, 90), G(D5, 55)};
static const Buzzer_note sfx_tank_hit[] = {V(C5, 45, 90), V(G4, 70, 85)};

/* ── Air Battle ── */
static const Buzzer_note sfx_air_fire[] = {V(C7, 18, 65), G(E6, 24)};
static const Buzzer_note sfx_air_hit[] = {V(A5, 35, 80), G(E5, 50), S(C5, 60)};
static const Buzzer_note sfx_air_pickup[] = {S(E6, 40), S(A6, 45), N(C7, 70)};
static const Buzzer_note sfx_boss_alert[] = {V(C5, 120, 95), R(45), V(C5, 120, 95), R(45), V(G4, 220, 100)};

/* ── Tetris ── */
static const Buzzer_note sfx_tetris_move[] = {V(C7, 12, 40), S(E7, 16)};
static const Buzzer_note sfx_tetris_rotate[] = {S(E7, 18), S(C7, 22)};
static const Buzzer_note sfx_tetris_lock[] = {V(G5, 40, 55), S(C5, 60)};
static const Buzzer_note sfx_tetris_line_clear[] = {S(B5, 40), S(E6, 45), S(G6, 55), N(B6, 75)};
static const Buzzer_note sfx_tetris_tetris[] = {
    S(E6, 38), S(G6, 42), S(B6, 48), S(D7, 55), S(E7, 60), N(G7, 80)};

/* ── Breakout ── */
static const Buzzer_note sfx_breakout_bounce[] = {S(A6, 20), S(C7, 28)};
static const Buzzer_note sfx_breakout_brick[] = {S(G6, 35), S(C7, 45), S(E7, 30)};

/* ── Pong ── */
static const Buzzer_note sfx_pong_paddle[] = {V(E7, 18, 35)};
static const Buzzer_note sfx_pong_wall[] = {V(A6, 22, 40)};
static const Buzzer_note sfx_pong_score[] = {G(E7, 50), G(C7, 65)};

/* ── Gomoku ── */
static const Buzzer_note sfx_gomoku_place[] = {S(C6, 40), S(G5, 55), N(E5, 70)};

/* ── 2048 ── */
static const Buzzer_note sfx_slide[] = {S(C6, 30), G(C6, 40)};
static const Buzzer_note sfx_merge[] = {S(C6, 35), S(E6, 40), N(G6, 60)};

/* ── Dino Runner ── */
static const Buzzer_note sfx_dino_jump[] = {S(C6, 30), N(E6, 50)};
static const Buzzer_note sfx_dino_duck[] = {G(E6, 35), S(C5, 48)};

/* ── Flappy Bird ── */
static const Buzzer_note sfx_flappy_flap[] = {S(C6, 28), S(E6, 36)};
static const Buzzer_note sfx_flappy_score[] = {S(C6, 38), S(E6, 42), N(G6, 70)};

/* ── Maze ── */
static const Buzzer_note sfx_maze_move[] = {V(C7, 12, 30)};
static const Buzzer_note sfx_maze_goal[] = {S(C6, 60), S(E6, 65), S(G6, 75), N(C7, 110)};

/* ── Needle ── */
static const Buzzer_note sfx_needle_launch[] = {G(C5, 50), G(E5, 55), N(G5, 40)};
static const Buzzer_note sfx_needle_stick[] = {V(C7, 20, 50), S(G6, 35)};

/* ── 通用结局 ── */
static const Buzzer_note sfx_explosion[] = {V(C5, 45, 100), G(G4, 85), G(C4, 130)};
static const Buzzer_note sfx_life_lost[] = {G(E6, 90), G(B5, 110), L(E5, 180)};
static const Buzzer_note sfx_victory[] = {
    N(C5, 110), N(E5, 110), N(G5, 110), N(C6, 220), N(E6, 110), N(G6, 110), L(C7, 500), R(120)};
static const Buzzer_note sfx_defeat[] = {
    G(E6, 180), G(C6, 180), G(A5, 220), G(F5, 240), G(D5, 300), L(C5, 520), R(100)};

/* ══════════════════════════════════════════════════════════════════════
 *  唯一音效库入口
 * ══════════════════════════════════════════════════════════════════════ */

const Music buzzer_sfx_library[buzzer_sfx_count] = {
    [buzzer_sfx_menu_move] = {sfx_menu_move, LEN(sfx_menu_move)},
    [buzzer_sfx_menu_select] = {sfx_menu_select, LEN(sfx_menu_select)},
    [buzzer_sfx_pellet] = {sfx_pellet, LEN(sfx_pellet)},
    [buzzer_sfx_power] = {sfx_power, LEN(sfx_power)},
    [buzzer_sfx_ghost] = {sfx_ghost, LEN(sfx_ghost)},
    [buzzer_sfx_pacman_waka] = {sfx_pacman_waka, LEN(sfx_pacman_waka)},
    [buzzer_sfx_snake_eat] = {sfx_snake_eat, LEN(sfx_snake_eat)},
    [buzzer_sfx_snake_turn] = {sfx_snake_turn, LEN(sfx_snake_turn)},
    [buzzer_sfx_snake_grow] = {sfx_snake_grow, LEN(sfx_snake_grow)},
    [buzzer_sfx_lane_change] = {sfx_lane_change, LEN(sfx_lane_change)},
    [buzzer_sfx_overtake] = {sfx_overtake, LEN(sfx_overtake)},
    [buzzer_sfx_racing_crash] = {sfx_racing_crash, LEN(sfx_racing_crash)},
    [buzzer_sfx_tank_fire] = {sfx_tank_fire, LEN(sfx_tank_fire)},
    [buzzer_sfx_tank_hit] = {sfx_tank_hit, LEN(sfx_tank_hit)},
    [buzzer_sfx_air_fire] = {sfx_air_fire, LEN(sfx_air_fire)},
    [buzzer_sfx_air_hit] = {sfx_air_hit, LEN(sfx_air_hit)},
    [buzzer_sfx_air_pickup] = {sfx_air_pickup, LEN(sfx_air_pickup)},
    [buzzer_sfx_boss_alert] = {sfx_boss_alert, LEN(sfx_boss_alert)},
    [buzzer_sfx_tetris_move] = {sfx_tetris_move, LEN(sfx_tetris_move)},
    [buzzer_sfx_tetris_rotate] = {sfx_tetris_rotate, LEN(sfx_tetris_rotate)},
    [buzzer_sfx_tetris_lock] = {sfx_tetris_lock, LEN(sfx_tetris_lock)},
    [buzzer_sfx_tetris_line_clear] = {sfx_tetris_line_clear, LEN(sfx_tetris_line_clear)},
    [buzzer_sfx_tetris_tetris] = {sfx_tetris_tetris, LEN(sfx_tetris_tetris)},
    [buzzer_sfx_breakout_bounce] = {sfx_breakout_bounce, LEN(sfx_breakout_bounce)},
    [buzzer_sfx_breakout_brick] = {sfx_breakout_brick, LEN(sfx_breakout_brick)},
    [buzzer_sfx_pong_paddle] = {sfx_pong_paddle, LEN(sfx_pong_paddle)},
    [buzzer_sfx_pong_wall] = {sfx_pong_wall, LEN(sfx_pong_wall)},
    [buzzer_sfx_pong_score] = {sfx_pong_score, LEN(sfx_pong_score)},
    [buzzer_sfx_gomoku_place] = {sfx_gomoku_place, LEN(sfx_gomoku_place)},
    [buzzer_sfx_slide] = {sfx_slide, LEN(sfx_slide)},
    [buzzer_sfx_merge] = {sfx_merge, LEN(sfx_merge)},
    [buzzer_sfx_dino_jump] = {sfx_dino_jump, LEN(sfx_dino_jump)},
    [buzzer_sfx_dino_duck] = {sfx_dino_duck, LEN(sfx_dino_duck)},
    [buzzer_sfx_flappy_flap] = {sfx_flappy_flap, LEN(sfx_flappy_flap)},
    [buzzer_sfx_flappy_score] = {sfx_flappy_score, LEN(sfx_flappy_score)},
    [buzzer_sfx_maze_move] = {sfx_maze_move, LEN(sfx_maze_move)},
    [buzzer_sfx_maze_goal] = {sfx_maze_goal, LEN(sfx_maze_goal)},
    [buzzer_sfx_needle_launch] = {sfx_needle_launch, LEN(sfx_needle_launch)},
    [buzzer_sfx_needle_stick] = {sfx_needle_stick, LEN(sfx_needle_stick)},
    [buzzer_sfx_explosion] = {sfx_explosion, LEN(sfx_explosion)},
    [buzzer_sfx_life_lost] = {sfx_life_lost, LEN(sfx_life_lost)},
    [buzzer_sfx_victory] = {sfx_victory, LEN(sfx_victory)},
    [buzzer_sfx_defeat] = {sfx_defeat, LEN(sfx_defeat)},
};
