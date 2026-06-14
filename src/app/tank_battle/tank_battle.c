#include "tank_battle.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define GRID_WIDTH  20
#define GRID_HEIGHT 20
#define CELL_SIZE   12
#define FIELD_X     0
#define FIELD_Y     48

#define ENEMY_COUNT       4
#define BULLET_COUNT      10
#define TOTAL_ENEMIES     12
#define PLAYER_MOVE_MS    250u
#define ENEMY_MOVE_MS     500u
#define BULLET_MOVE_MS    65u
#define ENEMY_SPAWN_MS    900u
#define ENEMY_FIRE_CHANCE 7u

#define COLOR_BLACK      0x0000u
#define COLOR_WHITE      0xffffu
#define COLOR_BRICK      0xb104u
#define COLOR_MORTAR     0x4208u
#define COLOR_STEEL      0x8410u
#define COLOR_PLAYER     0x07e0u
#define COLOR_ENEMY      0xf800u
#define COLOR_ENEMY_ALT  0xfd20u
#define COLOR_BULLET     0xffe0u
#define COLOR_BASE       0xffe0u
#define COLOR_HUD        0x07ffu
#define COLOR_GAME_OVER  0xf81fu
#define COLOR_WIN        0x07e0u

typedef enum {
    tile_empty,
    tile_brick,
    tile_steel,
    tile_base,
} Tile_type;

typedef enum {
    tank_state_playing,
    tank_state_over,
    tank_state_win,
} Tank_state;

typedef struct {
    int8_t x;
    int8_t y;
    Game_direction direction;
    uint8_t alive;
    uint8_t variant;
} Tank_actor;

typedef struct {
    int8_t x;
    int8_t y;
    Game_direction direction;
    uint8_t active;
    uint8_t from_player;
} Tank_bullet;

static const char* const g_level_template[GRID_HEIGHT] = {
    "....................",
    "..##....####....##..",
    "..##............##..",
    "......SS....SS......",
    ".####..........####.",
    "........####........",
    "..SS............SS..",
    "....##..####..##....",
    "....##........##....",
    ".##....SS..SS....##.",
    ".##..............##.",
    "....####....####....",
    "..SS............SS..",
    "......##....##......",
    ".####..........####.",
    "........####........",
    "..##............##..",
    "..##....####....##..",
    "........#..#........",
    "........#B.#........",
};

static Game_hardware g_hardware;
static Tile_type g_tiles[GRID_HEIGHT][GRID_WIDTH];
static Tank_actor g_player;
static Tank_actor g_enemies[ENEMY_COUNT];
static Tank_bullet g_bullets[BULLET_COUNT];
static Tank_state g_state = tank_state_playing;
static uint8_t g_lives = 3;
static uint8_t g_spawned = 0;
static uint8_t g_destroyed = 0;
static uint32_t g_score = 0;
static uint32_t g_last_player_move = 0;
static uint32_t g_last_enemy_move = 0;
static uint32_t g_last_bullet_move = 0;
static uint32_t g_last_spawn = 0;
static uint32_t g_random_state = 0x74a91c3du;
static uint16_t g_cell_buffer[CELL_SIZE * CELL_SIZE];

static void update_bullet(Tank_bullet* bullet);

static uint32_t random_next(void) {
    g_random_state = g_random_state * 1664525u + 1013904223u;
    return g_random_state;
}

static int8_t direction_x(Game_direction direction) {
    if (direction == game_direction_left) { return -1; }
    if (direction == game_direction_right) { return 1; }
    return 0;
}

static int8_t direction_y(Game_direction direction) {
    if (direction == game_direction_up) { return -1; }
    if (direction == game_direction_down) { return 1; }
    return 0;
}

static uint8_t position_in_bounds(int8_t x, int8_t y) {
    return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}

static Tank_actor* actor_at(int8_t x, int8_t y) {
    if (g_player.alive && g_player.x == x && g_player.y == y) { return &g_player; }
    for (uint8_t i = 0; i < ENEMY_COUNT; i++) {
        if (g_enemies[i].alive && g_enemies[i].x == x && g_enemies[i].y == y) {
            return &g_enemies[i];
        }
    }
    return NULL;
}

static uint8_t bullet_at(int8_t x, int8_t y) {
    for (uint8_t i = 0; i < BULLET_COUNT; i++) {
        if (g_bullets[i].active && g_bullets[i].x == x && g_bullets[i].y == y) { return 1; }
    }
    return 0;
}

static uint8_t can_enter(int8_t x, int8_t y) {
    if (!position_in_bounds(x, y) || g_tiles[y][x] != tile_empty) { return 0; }
    return actor_at(x, y) == NULL;
}

static void cell_pixel(int32_t x, int32_t y, uint16_t color) {
    if (x < 0 || x >= CELL_SIZE || y < 0 || y >= CELL_SIZE) { return; }
    g_cell_buffer[y * CELL_SIZE + x] = color;
}

static void draw_tank_sprite(Game_direction direction, uint16_t color) {
    for (int32_t y = 2; y < CELL_SIZE - 2; y++) {
        cell_pixel(1, y, color);
        cell_pixel(2, y, color);
        cell_pixel(CELL_SIZE - 3, y, color);
        cell_pixel(CELL_SIZE - 2, y, color);
    }
    for (int32_t y = 3; y < CELL_SIZE - 3; y++) {
        for (int32_t x = 3; x < CELL_SIZE - 3; x++) { cell_pixel(x, y, color); }
    }
    for (int32_t y = 4; y < 8; y++) {
        for (int32_t x = 4; x < 8; x++) { cell_pixel(x, y, COLOR_BLACK); }
    }

    if (direction == game_direction_up) {
        cell_pixel(5, 0, color);
        cell_pixel(6, 0, color);
        cell_pixel(5, 1, color);
        cell_pixel(6, 1, color);
    } else if (direction == game_direction_down) {
        cell_pixel(5, 10, color);
        cell_pixel(6, 10, color);
        cell_pixel(5, 11, color);
        cell_pixel(6, 11, color);
    } else if (direction == game_direction_left) {
        cell_pixel(0, 5, color);
        cell_pixel(0, 6, color);
        cell_pixel(1, 5, color);
        cell_pixel(1, 6, color);
    } else {
        cell_pixel(10, 5, color);
        cell_pixel(10, 6, color);
        cell_pixel(11, 5, color);
        cell_pixel(11, 6, color);
    }
}

static void draw_tile(Tile_type tile) {
    if (tile == tile_brick) {
        for (int32_t y = 1; y < CELL_SIZE - 1; y++) {
            for (int32_t x = 1; x < CELL_SIZE - 1; x++) {
                cell_pixel(x, y, ((x + (y / 3) * 2) % 6 == 0) ? COLOR_MORTAR : COLOR_BRICK);
            }
        }
    } else if (tile == tile_steel) {
        for (int32_t y = 1; y < CELL_SIZE - 1; y++) {
            for (int32_t x = 1; x < CELL_SIZE - 1; x++) {
                cell_pixel(x, y, (x == 1 || y == 1 || x == 10 || y == 10) ? COLOR_WHITE : COLOR_STEEL);
            }
        }
    } else if (tile == tile_base) {
        for (int32_t y = 2; y < 10; y++) {
            for (int32_t x = 2; x < 10; x++) {
                if (y >= 7 || (x >= 4 && x <= 7)) { cell_pixel(x, y, COLOR_BASE); }
            }
        }
    }
}

static void render_cell(int8_t x, int8_t y) {
    if (!position_in_bounds(x, y)) { return; }
    for (uint32_t i = 0; i < CELL_SIZE * CELL_SIZE; i++) { g_cell_buffer[i] = COLOR_BLACK; }

    draw_tile(g_tiles[y][x]);

    Tank_actor* actor = actor_at(x, y);
    if (actor != NULL) {
        const uint16_t color =
            actor == &g_player ? COLOR_PLAYER : (actor->variant ? COLOR_ENEMY_ALT : COLOR_ENEMY);
        draw_tank_sprite(actor->direction, color);
    }

    if (bullet_at(x, y)) {
        for (int32_t py = 5; py <= 7; py++) {
            for (int32_t px = 5; px <= 7; px++) { cell_pixel(px, py, COLOR_BULLET); }
        }
    }

    const int32_t screen_x = FIELD_X + x * CELL_SIZE;
    const int32_t screen_y = FIELD_Y + y * CELL_SIZE;
    St7789_Flush(g_hardware.lcd, screen_x, screen_y, screen_x + CELL_SIZE - 1,
        screen_y + CELL_SIZE - 1, (uint8_t*)g_cell_buffer, sizeof(g_cell_buffer));
}

static void render_hud(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, 42, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 6, 7, "SCORE", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 48, 7, g_score, 5, 1, COLOR_HUD);
    Game_Graphics_Draw_Text(g_hardware.lcd, 112, 7, "LIFE", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 154, 7, g_lives, 1, 1, COLOR_PLAYER);
    Game_Graphics_Draw_Text(g_hardware.lcd, 176, 7, "EN", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(
        g_hardware.lcd, 198, 7, TOTAL_ENEMIES - g_destroyed, 2, 1, COLOR_ENEMY);

    if (g_state == tank_state_over) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 80, 29, "GAME OVER", 1, COLOR_GAME_OVER);
    } else if (g_state == tank_state_win) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 76, 29, "YOU WIN", 1, COLOR_WIN);
    } else {
        Game_Graphics_Draw_Text(g_hardware.lcd, 68, 29, "PRESS FIRE", 1, COLOR_WHITE);
    }
}

static void render_full(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    for (int8_t y = 0; y < GRID_HEIGHT; y++) {
        for (int8_t x = 0; x < GRID_WIDTH; x++) { render_cell(x, y); }
    }
    render_hud();
    Game_Graphics_Draw_Text(g_hardware.lcd, 78, 300, "HOLD MENU", 1, COLOR_WHITE);
}

static void load_level(void) {
    for (int8_t y = 0; y < GRID_HEIGHT; y++) {
        for (int8_t x = 0; x < GRID_WIDTH; x++) {
            const char cell = g_level_template[y][x];
            g_tiles[y][x] = cell == '#' ? tile_brick
                                         : (cell == 'S' ? tile_steel
                                                        : (cell == 'B' ? tile_base : tile_empty));
        }
    }
}

static uint8_t reset_player_position(void) {
    static const int8_t respawn_positions[][2] = {
        {10, 18},
        {9, 18},
        {10, 17},
        {9, 17},
    };

    g_player.alive = 0;
    for (uint8_t i = 0; i < sizeof(respawn_positions) / sizeof(respawn_positions[0]); i++) {
        const int8_t x = respawn_positions[i][0];
        const int8_t y = respawn_positions[i][1];
        if (!can_enter(x, y)) { continue; }

        g_player = (Tank_actor){
            .x = x,
            .y = y,
            .direction = game_direction_up,
            .alive = 1,
            .variant = 0,
        };
        return 1;
    }

    return 0;
}

static uint8_t active_enemy_count(void) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < ENEMY_COUNT; i++) {
        if (g_enemies[i].alive) { count++; }
    }
    return count;
}

static void end_game(Tank_state state) {
    if (g_state != tank_state_playing) { return; }
    g_state = state;
    if (g_hardware.buzzer != NULL) {
        const Music_idx music = state == tank_state_win ? music_idx_victory : music_idx_death;
        Buzzer_Play(g_hardware.buzzer, &music_library[music], 0);
    }
    render_hud();
}

static void restart_game(void) {
    load_level();
    memset(g_enemies, 0, sizeof(g_enemies));
    memset(g_bullets, 0, sizeof(g_bullets));
    (void)reset_player_position();
    g_lives = 3;
    g_spawned = 0;
    g_destroyed = 0;
    g_score = 0;
    g_state = tank_state_playing;

    const uint32_t now = Bsp_Get_Tick_Ms();
    g_last_player_move = now;
    g_last_enemy_move = now;
    g_last_bullet_move = now;
    g_last_spawn = now - ENEMY_SPAWN_MS;
    render_full();
}

static uint8_t spawn_enemy(void) {
    static const int8_t spawn_x[] = {1, 9, 18};
    if (g_spawned >= TOTAL_ENEMIES || active_enemy_count() >= ENEMY_COUNT) { return 0; }

    for (uint8_t slot = 0; slot < ENEMY_COUNT; slot++) {
        if (g_enemies[slot].alive) { continue; }
        for (uint8_t attempt = 0; attempt < 3; attempt++) {
            const int8_t x = spawn_x[(g_spawned + attempt) % 3];
            if (!can_enter(x, 0)) { continue; }
            g_enemies[slot] = (Tank_actor){
                .x = x,
                .y = 0,
                .direction = game_direction_down,
                .alive = 1,
                .variant = g_spawned & 1u,
            };
            g_spawned++;
            render_cell(x, 0);
            render_hud();
            return 1;
        }
        break;
    }
    return 0;
}

static void move_actor(Tank_actor* actor, Game_direction direction) {
    if (actor == NULL || !actor->alive || direction == game_direction_none) { return; }
    const int8_t old_x = actor->x;
    const int8_t old_y = actor->y;
    actor->direction = direction;

    const int8_t next_x = actor->x + direction_x(direction);
    const int8_t next_y = actor->y + direction_y(direction);
    if (can_enter(next_x, next_y)) {
        actor->x = next_x;
        actor->y = next_y;
        render_cell(old_x, old_y);
    }
    render_cell(actor->x, actor->y);
}

static void fire_bullet(const Tank_actor* actor, uint8_t from_player) {
    if (actor == NULL || !actor->alive) { return; }
    for (uint8_t i = 0; i < BULLET_COUNT; i++) {
        if (g_bullets[i].active) { continue; }
        g_bullets[i] = (Tank_bullet){
            .x = actor->x,
            .y = actor->y,
            .direction = actor->direction,
            .active = 1,
            .from_player = from_player,
        };
        update_bullet(&g_bullets[i]);
        return;
    }
}

static void destroy_enemy(Tank_actor* enemy) {
    const int8_t x = enemy->x;
    const int8_t y = enemy->y;
    enemy->alive = 0;
    g_destroyed++;
    g_score += 100u;
    render_cell(x, y);
    render_hud();

    if (g_destroyed >= TOTAL_ENEMIES && active_enemy_count() == 0) { end_game(tank_state_win); }
}

static void hit_player(void) {
    const int8_t old_x = g_player.x;
    const int8_t old_y = g_player.y;
    if (g_lives > 0) { g_lives--; }
    g_player.alive = 0;
    render_cell(old_x, old_y);
    if (g_lives == 0) {
        end_game(tank_state_over);
        return;
    }
    if (reset_player_position()) {
        render_cell(g_player.x, g_player.y);
        render_hud();
    } else {
        g_lives = 0;
        end_game(tank_state_over);
    }
}

static void deactivate_bullet(Tank_bullet* bullet) {
    const int8_t x = bullet->x;
    const int8_t y = bullet->y;
    bullet->active = 0;
    render_cell(x, y);
}

static void update_bullet(Tank_bullet* bullet) {
    const int8_t old_x = bullet->x;
    const int8_t old_y = bullet->y;
    const int8_t next_x = old_x + direction_x(bullet->direction);
    const int8_t next_y = old_y + direction_y(bullet->direction);

    if (!position_in_bounds(next_x, next_y)) {
        deactivate_bullet(bullet);
        return;
    }

    const Tile_type tile = g_tiles[next_y][next_x];
    if (tile == tile_brick) {
        bullet->active = 0;
        g_tiles[next_y][next_x] = tile_empty;
        render_cell(old_x, old_y);
        render_cell(next_x, next_y);
        return;
    }
    if (tile == tile_steel) {
        deactivate_bullet(bullet);
        return;
    }
    if (tile == tile_base) {
        bullet->active = 0;
        g_tiles[next_y][next_x] = tile_empty;
        render_cell(old_x, old_y);
        render_cell(next_x, next_y);
        end_game(tank_state_over);
        return;
    }

    Tank_actor* target = actor_at(next_x, next_y);
    if (target != NULL) {
        bullet->active = 0;
        render_cell(old_x, old_y);
        if (target == &g_player) {
            if (!bullet->from_player) { hit_player(); }
        } else if (bullet->from_player) {
            destroy_enemy(target);
        }
        render_cell(next_x, next_y);
        return;
    }

    for (uint8_t i = 0; i < BULLET_COUNT; i++) {
        Tank_bullet* other = &g_bullets[i];
        if (other == bullet || !other->active || other->x != next_x || other->y != next_y) {
            continue;
        }
        other->active = 0;
        bullet->active = 0;
        render_cell(old_x, old_y);
        render_cell(next_x, next_y);
        return;
    }

    bullet->x = next_x;
    bullet->y = next_y;
    render_cell(old_x, old_y);
    render_cell(next_x, next_y);
}

static Game_direction choose_enemy_direction(const Tank_actor* enemy) {
    if ((random_next() % 4u) == 0u) {
        return (Game_direction)(game_direction_up + random_next() % 4u);
    }

    const int8_t dx = g_player.x - enemy->x;
    const int8_t dy = g_player.y - enemy->y;
    if ((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)) {
        return dx < 0 ? game_direction_left : game_direction_right;
    }
    return dy < 0 ? game_direction_up : game_direction_down;
}

static void update_enemies(void) {
    for (uint8_t i = 0; i < ENEMY_COUNT; i++) {
        Tank_actor* enemy = &g_enemies[i];
        if (!enemy->alive) { continue; }

        Game_direction direction = enemy->direction;
        const int8_t next_x = enemy->x + direction_x(direction);
        const int8_t next_y = enemy->y + direction_y(direction);
        if (!can_enter(next_x, next_y) || (random_next() % 5u) == 0u) {
            direction = choose_enemy_direction(enemy);
        }
        move_actor(enemy, direction);

        if ((random_next() % ENEMY_FIRE_CHANCE) == 0u) { fire_bullet(enemy, 0); }
    }
}

void Tank_Battle_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

Game_result Tank_Battle_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state != tank_state_playing) {
        if (input->confirm_pressed) {
            Buzzer_Stop(g_hardware.buzzer);
            restart_game();
        }
        return game_result_running;
    }

    if (input->direction != game_direction_none) { g_player.direction = input->direction; }
    if (input->confirm_pressed) { fire_bullet(&g_player, 1); }

    const uint32_t now = Bsp_Get_Tick_Ms();
    if (input->direction != game_direction_none && now - g_last_player_move >= PLAYER_MOVE_MS) {
        g_last_player_move = now;
        move_actor(&g_player, input->direction);
    }
    if (now - g_last_enemy_move >= ENEMY_MOVE_MS) {
        g_last_enemy_move = now;
        update_enemies();
    }
    if (g_state != tank_state_playing) { return game_result_running; }

    if (now - g_last_bullet_move >= BULLET_MOVE_MS) {
        g_last_bullet_move = now;
        for (uint8_t i = 0; i < BULLET_COUNT; i++) {
            if (g_bullets[i].active) { update_bullet(&g_bullets[i]); }
            if (g_state != tank_state_playing) { break; }
        }
    }
    if (g_state != tank_state_playing) { return game_result_running; }

    if (now - g_last_spawn >= ENEMY_SPAWN_MS) {
        if (spawn_enemy()) { g_last_spawn = now; }
    }
    return game_result_running;
}

uint32_t Tank_Battle_Get_Score(void) { return g_score; }

uint8_t Tank_Battle_Is_Finished(void) { return g_state != tank_state_playing; }
