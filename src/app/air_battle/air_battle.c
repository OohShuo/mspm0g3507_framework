#include "air_battle.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "air_assets.h"
#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define HUD_HEIGHT    30

#define MAX_ENEMIES       7
#define MAX_BULLETS       18
#define MAX_PICKUPS       3
#define MAX_EXPLOSIONS    5
#define MAX_DIRTY_RECTS   36
#define NORMAL_ENEMIES    18

#define PLAYER_MOVE_STEP  4
#define PLAYER_FIRE_MS    190u
#define WORLD_STEP_MS     50u
#define ENEMY_SPAWN_MS    720u
#define INVINCIBLE_MS     1300u

#define COLOR_BLACK       0x0000u
#define COLOR_WHITE       0xffffu
#define COLOR_CYAN        0x07ffu
#define COLOR_BLUE_DARK   0x0864u
#define COLOR_BLUE        0x041fu
#define COLOR_GREEN       0x07e0u
#define COLOR_YELLOW      0xffe0u
#define COLOR_ORANGE      0xfd20u
#define COLOR_RED         0xf800u
#define COLOR_PINK        0xf81fu

typedef enum {
    air_state_playing,
    air_state_over,
    air_state_win,
} Air_state;

typedef enum {
    enemy_mob,
    enemy_elite,
    enemy_elite_pro,
    enemy_boss,
} Enemy_kind;

typedef enum {
    pickup_blood,
    pickup_bomb,
    pickup_bullet,
} Pickup_kind;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vx;
    uint8_t active;
    uint8_t hp;
    Enemy_kind kind;
    uint32_t next_fire_at;
} Enemy;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t dx;
    int8_t dy;
    uint8_t active;
    uint8_t from_enemy;
} Bullet;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t active;
    Pickup_kind kind;
} Pickup;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t active;
    uint8_t radius;
    uint8_t life;
} Explosion;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
} Dirty_rect;

static Game_hardware g_hardware;
static Air_state g_state = air_state_playing;
static Enemy g_enemies[MAX_ENEMIES];
static Bullet g_bullets[MAX_BULLETS];
static Pickup g_pickups[MAX_PICKUPS];
static Explosion g_explosions[MAX_EXPLOSIONS];
static Dirty_rect g_dirty[MAX_DIRTY_RECTS];
static uint8_t g_dirty_count = 0;
static uint8_t g_force_full_redraw = 0;
static int16_t g_player_x = 102;
static int16_t g_player_y = 270;
static uint8_t g_lives = 3;
static uint8_t g_bombs = 2;
static uint8_t g_shot_level = 1;
static uint8_t g_spawned = 0;
static uint8_t g_destroyed = 0;
static uint8_t g_boss_spawned = 0;
static uint8_t g_boss_hp = 0;
static uint8_t g_boss_max_hp = 0;
static uint8_t g_fire_sound_divider = 0;
static uint32_t g_score = 0;
static uint32_t g_invincible_until = 0;
static uint32_t g_last_fire = 0;
static uint32_t g_last_world_step = 0;
static uint32_t g_last_spawn = 0;
static uint32_t g_random_state = 0x6d2b79f5u;
static uint16_t g_line_buffer[SCREEN_WIDTH];

static uint32_t random_next(void) {
    g_random_state = g_random_state * 1664525u + 1013904223u;
    return g_random_state;
}

static int16_t clamp_i16(int16_t value, int16_t minimum, int16_t maximum) {
    if (value < minimum) { return minimum; }
    if (value > maximum) { return maximum; }
    return value;
}

static const Air_sprite* enemy_sprite(Enemy_kind kind) {
    if (kind == enemy_mob) { return &air_sprite_mob; }
    if (kind == enemy_elite) { return &air_sprite_elite; }
    if (kind == enemy_elite_pro) { return &air_sprite_elite_pro; }
    return &air_sprite_boss;
}

static const Air_sprite* pickup_sprite(Pickup_kind kind) {
    if (kind == pickup_blood) { return &air_sprite_prop_blood; }
    if (kind == pickup_bomb) { return &air_sprite_prop_bomb; }
    return &air_sprite_prop_bullet;
}

static uint8_t rects_touch(const Dirty_rect* a, const Dirty_rect* b) {
    return a->x <= b->x + b->width + 2 && b->x <= a->x + a->width + 2 &&
           a->y <= b->y + b->height + 2 && b->y <= a->y + a->height + 2;
}

static void mark_dirty(int16_t x, int16_t y, int16_t width, int16_t height) {
    if (width <= 0 || height <= 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT ||
        x + width <= 0 || y + height <= HUD_HEIGHT) {
        return;
    }

    const int16_t x1 = clamp_i16(x, 0, SCREEN_WIDTH);
    const int16_t y1 = clamp_i16(y, HUD_HEIGHT, SCREEN_HEIGHT);
    const int16_t x2 = clamp_i16((int16_t)(x + width), 0, SCREEN_WIDTH);
    const int16_t y2 = clamp_i16((int16_t)(y + height), HUD_HEIGHT, SCREEN_HEIGHT);
    Dirty_rect incoming = {x1, y1, (int16_t)(x2 - x1), (int16_t)(y2 - y1)};

    for (uint8_t i = 0; i < g_dirty_count; i++) {
        Dirty_rect* existing = &g_dirty[i];
        if (!rects_touch(existing, &incoming)) { continue; }
        const int16_t right = existing->x + existing->width > incoming.x + incoming.width
                                  ? existing->x + existing->width
                                  : incoming.x + incoming.width;
        const int16_t bottom = existing->y + existing->height > incoming.y + incoming.height
                                   ? existing->y + existing->height
                                   : incoming.y + incoming.height;
        existing->x = existing->x < incoming.x ? existing->x : incoming.x;
        existing->y = existing->y < incoming.y ? existing->y : incoming.y;
        existing->width = right - existing->x;
        existing->height = bottom - existing->y;
        return;
    }

    if (g_dirty_count < MAX_DIRTY_RECTS) {
        g_dirty[g_dirty_count++] = incoming;
    } else {
        g_force_full_redraw = 1;
    }
}

static void mark_sprite(int16_t x, int16_t y, const Air_sprite* sprite) {
    mark_dirty(x, y, sprite->width, sprite->height);
}

static uint8_t overlaps(int16_t ax, int16_t ay, int16_t aw, int16_t ah, int16_t bx,
    int16_t by, int16_t bw, int16_t bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void draw_sprite_line(const Air_sprite* sprite, int16_t sprite_x, int16_t sprite_y,
    int16_t screen_y, int16_t region_x, int16_t region_width) {
    const int16_t source_y = screen_y - sprite_y;
    if (source_y < 0 || source_y >= sprite->height) { return; }

    int16_t start_x = sprite_x > region_x ? sprite_x : region_x;
    int16_t end_x = sprite_x + sprite->width < region_x + region_width
                        ? sprite_x + sprite->width
                        : region_x + region_width;
    for (int16_t screen_x = start_x; screen_x < end_x; screen_x++) {
        const uint16_t index =
            (uint16_t)(source_y * sprite->width + (screen_x - sprite_x));
        if ((sprite->mask[index >> 3] & (1u << (index & 7u))) != 0) {
            g_line_buffer[screen_x - region_x] = sprite->pixels[index];
        }
    }
}

static void draw_explosion_line(const Explosion* explosion, int16_t screen_y,
    int16_t region_x, int16_t region_width) {
    const int16_t dy = screen_y - explosion->y;
    if (dy < -(int16_t)explosion->radius || dy > explosion->radius) { return; }

    for (int16_t x = region_x; x < region_x + region_width; x++) {
        const int16_t dx = x - explosion->x;
        const int16_t distance = (int16_t)(dx * dx + dy * dy);
        const int16_t radius2 = explosion->radius * explosion->radius;
        if (distance <= radius2 && ((x + screen_y + explosion->life) & 1) == 0) {
            g_line_buffer[x - region_x] =
                distance < radius2 / 3 ? COLOR_YELLOW : COLOR_ORANGE;
        }
    }
}

static void compose_line(int16_t x, int16_t y, int16_t width) {
    for (int16_t col = 0; col < width; col++) {
        const int16_t screen_x = x + col;
        const uint16_t bg_x = (uint16_t)(screen_x / AIR_BACKGROUND_SCALE);
        const uint16_t bg_y = (uint16_t)(y / AIR_BACKGROUND_SCALE);
        g_line_buffer[col] = air_background[bg_y * AIR_BACKGROUND_WIDTH + bg_x];
    }

    for (uint8_t i = 0; i < MAX_PICKUPS; i++) {
        if (g_pickups[i].active) {
            draw_sprite_line(
                pickup_sprite(g_pickups[i].kind), g_pickups[i].x, g_pickups[i].y, y, x, width);
        }
    }
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        if (!g_bullets[i].active) { continue; }
        const Air_sprite* sprite =
            g_bullets[i].from_enemy ? &air_sprite_bullet_enemy : &air_sprite_bullet_hero;
        draw_sprite_line(sprite, g_bullets[i].x, g_bullets[i].y, y, x, width);
    }
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (g_enemies[i].active) {
            draw_sprite_line(enemy_sprite(g_enemies[i].kind), g_enemies[i].x,
                g_enemies[i].y, y, x, width);
        }
    }

    const uint32_t now = Bsp_Get_Tick_Ms();
    if (now >= g_invincible_until || ((now / 100u) & 1u) == 0) {
        draw_sprite_line(&air_sprite_hero, g_player_x, g_player_y, y, x, width);
    }
    for (uint8_t i = 0; i < MAX_EXPLOSIONS; i++) {
        if (g_explosions[i].active) {
            draw_explosion_line(&g_explosions[i], y, x, width);
        }
    }
}

static void render_region(const Dirty_rect* rect) {
    for (int16_t row = 0; row < rect->height; row++) {
        const int16_t screen_y = rect->y + row;
        compose_line(rect->x, screen_y, rect->width);
        St7789_Flush(g_hardware.lcd, rect->x, screen_y, rect->x + rect->width - 1,
            screen_y, (uint8_t*)g_line_buffer, (uint32_t)rect->width * sizeof(uint16_t));
    }
}

static void flush_dirty(void) {
    if (g_force_full_redraw) {
        const Dirty_rect full = {0, HUD_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - HUD_HEIGHT};
        render_region(&full);
    } else {
        for (uint8_t i = 0; i < g_dirty_count; i++) { render_region(&g_dirty[i]); }
    }
    g_dirty_count = 0;
    g_force_full_redraw = 0;
}

static void render_hud(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, HUD_HEIGHT, COLOR_BLUE_DARK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 5, 4, "SCORE", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 47, 4, g_score, 5, 1, COLOR_CYAN);
    Game_Graphics_Draw_Text(g_hardware.lcd, 112, 4, "LIFE", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 154, 4, g_lives, 1, 1, COLOR_GREEN);
    Game_Graphics_Draw_Text(g_hardware.lcd, 175, 4, "BOMB", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 217, 4, g_bombs, 1, 1, COLOR_YELLOW);

    if (g_boss_hp > 0 && g_boss_max_hp > 0) {
        Game_Graphics_Fill_Rect(g_hardware.lcd, 5, 19, 230, 6, COLOR_BLACK);
        Game_Graphics_Fill_Rect(g_hardware.lcd, 7, 21,
            (226 * g_boss_hp) / g_boss_max_hp, 2, COLOR_RED);
    } else {
        Game_Graphics_Draw_Text(g_hardware.lcd, 73, 19, "SKY FORCE", 1, COLOR_CYAN);
    }
}

static void render_full(void) {
    const Dirty_rect full = {0, HUD_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - HUD_HEIGHT};
    render_region(&full);
    render_hud();
}

static void add_explosion(int16_t x, int16_t y) {
    for (uint8_t i = 0; i < MAX_EXPLOSIONS; i++) {
        if (g_explosions[i].active) { continue; }
        g_explosions[i] = (Explosion){x, y, 1, 4, 7};
        mark_dirty(x - 5, y - 5, 11, 11);
        return;
    }
}

static Bullet* allocate_bullet(void) {
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        if (!g_bullets[i].active) { return &g_bullets[i]; }
    }
    return NULL;
}

static void spawn_bullet(
    int16_t x, int16_t y, int8_t dx, int8_t dy, uint8_t from_enemy) {
    Bullet* bullet = allocate_bullet();
    if (bullet == NULL) { return; }
    *bullet = (Bullet){x, y, dx, dy, 1, from_enemy};
    mark_sprite(x, y, from_enemy ? &air_sprite_bullet_enemy : &air_sprite_bullet_hero);
}

static void player_fire(void) {
    const int16_t center = g_player_x + air_sprite_hero.width / 2 -
                           air_sprite_bullet_hero.width / 2;
    if (g_shot_level == 1) {
        spawn_bullet(center, g_player_y - 8, 0, -6, 0);
    } else if (g_shot_level == 2) {
        spawn_bullet(g_player_x + 7, g_player_y - 5, 0, -6, 0);
        spawn_bullet(g_player_x + air_sprite_hero.width - 11, g_player_y - 5, 0, -6, 0);
    } else {
        spawn_bullet(center, g_player_y - 9, 0, -7, 0);
        spawn_bullet(g_player_x + 5, g_player_y - 3, -1, -6, 0);
        spawn_bullet(g_player_x + air_sprite_hero.width - 9, g_player_y - 3, 1, -6, 0);
    }

    if (++g_fire_sound_divider >= 3) {
        g_fire_sound_divider = 0;
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_air_fire);
    }
}

static uint8_t active_enemy_count(void) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (g_enemies[i].active) { count++; }
    }
    return count;
}

static void spawn_enemy(uint32_t now) {
    if (g_spawned >= NORMAL_ENEMIES) { return; }
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (g_enemies[i].active) { continue; }

        Enemy_kind kind = enemy_mob;
        if ((g_spawned % 6u) == 5u) {
            kind = enemy_elite_pro;
        } else if ((g_spawned % 3u) == 2u) {
            kind = enemy_elite;
        }
        const Air_sprite* sprite = enemy_sprite(kind);
        const int16_t x = (int16_t)(8 + random_next() % (SCREEN_WIDTH - sprite->width - 16));
        const uint8_t hp = kind == enemy_mob ? 1 : (kind == enemy_elite ? 3 : 5);
        const int8_t vx = kind == enemy_mob ? 0 : ((random_next() & 1u) ? 2 : -2);
        g_enemies[i] = (Enemy){x, HUD_HEIGHT, vx, 1, hp, kind,
            now + 550u + random_next() % 700u};
        g_spawned++;
        mark_sprite(x, HUD_HEIGHT, sprite);
        return;
    }
}

static void spawn_boss(uint32_t now) {
    if (g_boss_spawned || g_spawned < NORMAL_ENEMIES || active_enemy_count() != 0) {
        return;
    }
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (g_enemies[i].active) { continue; }
        g_boss_max_hp = 36;
        g_boss_hp = g_boss_max_hp;
        g_enemies[i] = (Enemy){(SCREEN_WIDTH - air_sprite_boss.width) / 2, HUD_HEIGHT + 8,
            2, 1, g_boss_hp, enemy_boss, now + 700u};
        g_boss_spawned = 1;
        mark_sprite(g_enemies[i].x, g_enemies[i].y, &air_sprite_boss);
        render_hud();
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_boss_alert);
        return;
    }
}

static void spawn_pickup(int16_t x, int16_t y) {
    if ((random_next() % 100u) >= 38u) { return; }
    for (uint8_t i = 0; i < MAX_PICKUPS; i++) {
        if (g_pickups[i].active) { continue; }
        g_pickups[i] =
            (Pickup){x, y, 1, (Pickup_kind)(random_next() % 3u)};
        mark_sprite(x, y, pickup_sprite(g_pickups[i].kind));
        return;
    }
}

static void finish_game(Air_state state) {
    if (g_state != air_state_playing) { return; }
    g_state = state;
    Buzzer_Play_Music(g_hardware.buzzer,
        state == air_state_win ? music_idx_victory : music_idx_defeat, 0);
    Game_Graphics_Fill_Rect(g_hardware.lcd, 31, 133, 178, 70, COLOR_BLUE_DARK);
    Game_Graphics_Fill_Rect(g_hardware.lcd, 35, 137, 170, 62, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, state == air_state_win ? 42 : 37, 148,
        state == air_state_win ? "MISSION CLEAR" : "MISSION FAILED", 2,
        state == air_state_win ? COLOR_CYAN : COLOR_PINK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 73, 181, "PRESS RESTART", 1, COLOR_WHITE);
}

static void destroy_enemy(Enemy* enemy) {
    const Air_sprite* sprite = enemy_sprite(enemy->kind);
    mark_sprite(enemy->x, enemy->y, sprite);
    add_explosion(enemy->x + sprite->width / 2, enemy->y + sprite->height / 2);
    const int16_t pickup_x = enemy->x + sprite->width / 2 - 7;
    const int16_t pickup_y = enemy->y + sprite->height / 2 - 7;
    const Enemy_kind kind = enemy->kind;
    enemy->active = 0;

    if (kind == enemy_boss) {
        g_boss_hp = 0;
        g_score += 2000u;
        g_destroyed++;
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_explosion);
        render_hud();
        flush_dirty();
        finish_game(air_state_win);
        return;
    }

    g_destroyed++;
    g_score += kind == enemy_mob ? 100u : (kind == enemy_elite ? 250u : 400u);
    spawn_pickup(pickup_x, pickup_y);
    Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_explosion);
    render_hud();
}

static void damage_enemy(Enemy* enemy, uint8_t damage) {
    if (!enemy->active) { return; }
    if (damage >= enemy->hp) {
        enemy->hp = 0;
        destroy_enemy(enemy);
    } else {
        enemy->hp -= damage;
        if (enemy->kind == enemy_boss) {
            g_boss_hp = enemy->hp;
            render_hud();
        }
    }
}

static void hit_player(uint32_t now) {
    if (now < g_invincible_until || g_state != air_state_playing) { return; }
    add_explosion(g_player_x + air_sprite_hero.width / 2,
        g_player_y + air_sprite_hero.height / 2);
    if (g_lives > 0) { g_lives--; }
    g_invincible_until = now + INVINCIBLE_MS;
    Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_life_lost);
    render_hud();
    if (g_lives == 0) {
        flush_dirty();
        finish_game(air_state_over);
    }
}

static void use_bomb(void) {
    if (g_bombs == 0) { return; }
    g_bombs--;
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        if (g_bullets[i].active && g_bullets[i].from_enemy) {
            mark_sprite(g_bullets[i].x, g_bullets[i].y, &air_sprite_bullet_enemy);
            g_bullets[i].active = 0;
        }
    }
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].active) { continue; }
        const Air_sprite* sprite = enemy_sprite(g_enemies[i].kind);
        add_explosion(g_enemies[i].x + sprite->width / 2,
            g_enemies[i].y + sprite->height / 2);
        damage_enemy(&g_enemies[i], g_enemies[i].kind == enemy_boss ? 6 : 3);
        if (g_state != air_state_playing) { return; }
    }
    Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_explosion);
    render_hud();
}

static void move_player(Game_direction direction) {
    const int16_t old_x = g_player_x;
    const int16_t old_y = g_player_y;
    if (direction == game_direction_left) {
        g_player_x -= PLAYER_MOVE_STEP;
    } else if (direction == game_direction_right) {
        g_player_x += PLAYER_MOVE_STEP;
    } else if (direction == game_direction_up) {
        g_player_y -= PLAYER_MOVE_STEP;
    } else if (direction == game_direction_down) {
        g_player_y += PLAYER_MOVE_STEP;
    }
    g_player_x = clamp_i16(g_player_x, 0, SCREEN_WIDTH - air_sprite_hero.width);
    g_player_y =
        clamp_i16(g_player_y, HUD_HEIGHT + 4, SCREEN_HEIGHT - air_sprite_hero.height);
    if (old_x != g_player_x || old_y != g_player_y) {
        mark_sprite(old_x, old_y, &air_sprite_hero);
        mark_sprite(g_player_x, g_player_y, &air_sprite_hero);
    }
}

static void update_explosions(void) {
    for (uint8_t i = 0; i < MAX_EXPLOSIONS; i++) {
        Explosion* explosion = &g_explosions[i];
        if (!explosion->active) { continue; }
        mark_dirty(explosion->x - explosion->radius, explosion->y - explosion->radius,
            explosion->radius * 2 + 1, explosion->radius * 2 + 1);
        if (explosion->life > 0) { explosion->life--; }
        if (explosion->life == 0) {
            explosion->active = 0;
        } else {
            explosion->radius = (uint8_t)(explosion->radius + 3);
            mark_dirty(explosion->x - explosion->radius, explosion->y - explosion->radius,
                explosion->radius * 2 + 1, explosion->radius * 2 + 1);
        }
    }
}

static void update_pickups(void) {
    for (uint8_t i = 0; i < MAX_PICKUPS; i++) {
        Pickup* pickup = &g_pickups[i];
        if (!pickup->active) { continue; }
        const Air_sprite* sprite = pickup_sprite(pickup->kind);
        mark_sprite(pickup->x, pickup->y, sprite);
        pickup->y += 2;
        if (pickup->y >= SCREEN_HEIGHT) {
            pickup->active = 0;
            continue;
        }

        if (overlaps(pickup->x, pickup->y, sprite->width, sprite->height, g_player_x,
                g_player_y, air_sprite_hero.width, air_sprite_hero.height)) {
            pickup->active = 0;
            if (pickup->kind == pickup_blood && g_lives < 5) {
                g_lives++;
            } else if (pickup->kind == pickup_bomb && g_bombs < 9) {
                g_bombs++;
            } else if (pickup->kind == pickup_bullet && g_shot_level < 3) {
                g_shot_level++;
            } else {
                g_score += 150u;
            }
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_air_pickup);
            render_hud();
        } else {
            mark_sprite(pickup->x, pickup->y, sprite);
        }
    }
}

static void update_enemy_fire(Enemy* enemy, uint32_t now) {
    if (now < enemy->next_fire_at || enemy->y < HUD_HEIGHT) { return; }
    const Air_sprite* sprite = enemy_sprite(enemy->kind);
    const int16_t center = enemy->x + sprite->width / 2 -
                           air_sprite_bullet_enemy.width / 2;
    if (enemy->kind == enemy_boss) {
        spawn_bullet(center, enemy->y + sprite->height - 2, 0, 4, 1);
        spawn_bullet(center - 18, enemy->y + sprite->height - 5, -1, 4, 1);
        spawn_bullet(center + 18, enemy->y + sprite->height - 5, 1, 4, 1);
        enemy->next_fire_at = now + 650u;
    } else {
        spawn_bullet(center, enemy->y + sprite->height - 2, 0,
            enemy->kind == enemy_mob ? 3 : 4, 1);
        enemy->next_fire_at = now + 900u + random_next() % 900u;
    }
}

static void update_enemies(uint32_t now) {
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        Enemy* enemy = &g_enemies[i];
        if (!enemy->active) { continue; }
        const Air_sprite* sprite = enemy_sprite(enemy->kind);
        mark_sprite(enemy->x, enemy->y, sprite);

        if (enemy->kind == enemy_boss) {
            enemy->x += enemy->vx;
            if (enemy->x <= 5 || enemy->x + sprite->width >= SCREEN_WIDTH - 5) {
                enemy->vx = (int8_t)-enemy->vx;
                enemy->x = clamp_i16(enemy->x, 5, SCREEN_WIDTH - sprite->width - 5);
            }
        } else {
            enemy->y += enemy->kind == enemy_mob ? 3 : 2;
            enemy->x += enemy->vx;
            if (enemy->x <= 2 || enemy->x + sprite->width >= SCREEN_WIDTH - 2) {
                enemy->vx = (int8_t)-enemy->vx;
            }
        }

        if (enemy->y >= SCREEN_HEIGHT) {
            enemy->active = 0;
            hit_player(now);
            if (g_state != air_state_playing) { return; }
            continue;
        }

        if (overlaps(enemy->x + 3, enemy->y + 3, sprite->width - 6, sprite->height - 6,
                g_player_x + 5, g_player_y + 5, air_sprite_hero.width - 10,
                air_sprite_hero.height - 8)) {
            if (enemy->kind != enemy_boss) { destroy_enemy(enemy); }
            hit_player(now);
            continue;
        }
        mark_sprite(enemy->x, enemy->y, sprite);
        update_enemy_fire(enemy, now);
    }
}

static void deactivate_bullet(Bullet* bullet) {
    const Air_sprite* sprite =
        bullet->from_enemy ? &air_sprite_bullet_enemy : &air_sprite_bullet_hero;
    mark_sprite(bullet->x, bullet->y, sprite);
    bullet->active = 0;
}

static void update_bullets(uint32_t now) {
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        Bullet* bullet = &g_bullets[i];
        if (!bullet->active) { continue; }
        const Air_sprite* sprite =
            bullet->from_enemy ? &air_sprite_bullet_enemy : &air_sprite_bullet_hero;
        mark_sprite(bullet->x, bullet->y, sprite);
        bullet->x += bullet->dx;
        bullet->y += bullet->dy;

        if (bullet->y + sprite->height < HUD_HEIGHT || bullet->y >= SCREEN_HEIGHT ||
            bullet->x + sprite->width < 0 || bullet->x >= SCREEN_WIDTH) {
            bullet->active = 0;
            continue;
        }

        if (bullet->from_enemy) {
            if (overlaps(bullet->x, bullet->y, sprite->width, sprite->height,
                    g_player_x + 6, g_player_y + 5, air_sprite_hero.width - 12,
                    air_sprite_hero.height - 8)) {
                deactivate_bullet(bullet);
                hit_player(now);
                if (g_state != air_state_playing) { return; }
                continue;
            }
        } else {
            for (uint8_t enemy_index = 0; enemy_index < MAX_ENEMIES; enemy_index++) {
                Enemy* enemy = &g_enemies[enemy_index];
                if (!enemy->active) { continue; }
                const Air_sprite* target = enemy_sprite(enemy->kind);
                if (!overlaps(bullet->x, bullet->y, sprite->width, sprite->height,
                        enemy->x + 2, enemy->y + 2, target->width - 4,
                        target->height - 4)) {
                    continue;
                }
                deactivate_bullet(bullet);
                damage_enemy(enemy, 1);
                if (g_state != air_state_playing) { return; }
                break;
            }
            if (!bullet->active) { continue; }
        }
        mark_sprite(bullet->x, bullet->y, sprite);
    }
}

static void restart_game(void) {
    memset(g_enemies, 0, sizeof(g_enemies));
    memset(g_bullets, 0, sizeof(g_bullets));
    memset(g_pickups, 0, sizeof(g_pickups));
    memset(g_explosions, 0, sizeof(g_explosions));
    g_player_x = (SCREEN_WIDTH - air_sprite_hero.width) / 2;
    g_player_y = SCREEN_HEIGHT - air_sprite_hero.height - 8;
    g_lives = 3;
    g_bombs = 2;
    g_shot_level = 1;
    g_spawned = 0;
    g_destroyed = 0;
    g_boss_spawned = 0;
    g_boss_hp = 0;
    g_boss_max_hp = 0;
    g_score = 0;
    g_invincible_until = 0;
    g_state = air_state_playing;
    g_dirty_count = 0;
    g_force_full_redraw = 0;

    const uint32_t now = Bsp_Get_Tick_Ms();
    g_last_fire = now;
    g_last_world_step = now;
    g_last_spawn = now - ENEMY_SPAWN_MS;
    render_full();
    Buzzer_Play_Music(g_hardware.buzzer, music_idx_air_theme, 1);
}

void Air_Battle_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

Game_result Air_Battle_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }
    if (g_state != air_state_playing) {
        if (input->confirm_pressed) {
            Buzzer_Stop(g_hardware.buzzer);
            restart_game();
        }
        return game_result_running;
    }

    if (input->direction != game_direction_none) { move_player(input->direction); }
    if (input->confirm_pressed) { use_bomb(); }
    if (g_state != air_state_playing) { return game_result_running; }

    const uint32_t now = Bsp_Get_Tick_Ms();
    if (now - g_last_fire >= PLAYER_FIRE_MS) {
        g_last_fire = now;
        player_fire();
    }
    if (now - g_last_spawn >= ENEMY_SPAWN_MS && g_spawned < NORMAL_ENEMIES) {
        g_last_spawn = now;
        spawn_enemy(now);
    }
    spawn_boss(now);

    if (now - g_last_world_step >= WORLD_STEP_MS) {
        g_last_world_step = now;
        update_bullets(now);
        if (g_state != air_state_playing) { return game_result_running; }
        update_enemies(now);
        if (g_state != air_state_playing) { return game_result_running; }
        update_pickups();
        update_explosions();
    }
    flush_dirty();
    return game_result_running;
}

uint32_t Air_Battle_Get_Score(void) { return g_score; }

uint8_t Air_Battle_Is_Finished(void) { return g_state != air_state_playing; }
