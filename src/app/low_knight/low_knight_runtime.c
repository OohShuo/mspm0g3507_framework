#include "low_knight_runtime.h"

#include <stddef.h>
#include <string.h>

#include "game_graphics.h"

#define SCREEN_WIDTH           240
#define SCREEN_HEIGHT          320
#define PICO_SCREEN_SIZE       128
#define PICO_TILE_SIZE         8
#define PICO_GFX_STRIDE_BYTES  64u
#define PICO_GFX_MAP_OFFSET    0x1000u
#define PICO_MAP_WIDTH         128u
#define PICO_MAP_HEIGHT        64u
#define PICO_MAP_RLE_WRAP_X    104u
#define LOW_KNIGHT_MAX_ROOM_W  48u
#define LOW_KNIGHT_MAX_ROOM_H  24u
#define LOW_KNIGHT_START_ROOM  9u
#define LOW_KNIGHT_BG_TILE_X   109u
#define LOW_KNIGHT_BG_TILE_Y   0u
#define LOW_KNIGHT_BG_TILE_W   19u
#define LOW_KNIGHT_BG_TILE_H   20u
#define TILE_ROW_CACHE_SIZE    64u
#define ENEMY_TILE_CACHE_COUNT 46u

#define PHYSICS_Q_SHIFT        8
#define PHYSICS_Q_ONE          (1 << PHYSICS_Q_SHIFT)
#define PLAYER_HIT_LEFT        (-3)
#define PLAYER_HIT_RIGHT       3
#define PLAYER_HIT_TOP         (-10)
#define PLAYER_HIT_BOTTOM      0
#define PLAYER_MOVE_SPEED_Q    448
#define PLAYER_GRAVITY_Q       72
#define PLAYER_MAX_FALL_Q      832
#define PLAYER_JUMP_Q          (-1200)
#define PLAYER_JUMP_CUT_Q      (-180)
#define PLAYER_WALL_SLIDE_Q    96
#define PLAYER_WALL_JUMP_X_Q   640
#define PLAYER_DASH_Q          1500
#define PLAYER_DASH_FRAMES     10u
#define PLAYER_COYOTE_FRAMES   3u
#define PLAYER_JUMP_BUFFER_FRAMES 4u
#define ROOM_ENTRY_INSET       24
#define ROOM_CORNER_MARGIN     24
#define LOW_KNIGHT_MAX_ENEMIES 12u
#define LOW_KNIGHT_MAX_PROJECTILES 8u
#define LOW_KNIGHT_MAX_BLOOD_PARTICLES 12u
#define LOW_KNIGHT_MAX_ITEMS   8u
#define LOW_KNIGHT_MAX_COLLECTED_ITEMS 32u
#define ENEMY_FLY_TILE         71u
#define ENEMY_AMBUSH_TILE      108u
#define ENEMY_MOSS_TILE        172u
#define ENEMY_BEE_TILE         192u
#define ENEMY_LIMPER_TILE      204u
#define ENEMY_BUSH_TILE        207u
#define ENEMY_FATGUY_TILE      210u
#define ENEMY_BALLGUY_TILE     248u
#define BUSH_MOVE_SPEED_Q      192
#define BUSH_GRAVITY_Q         96
#define BUSH_MAX_FALL_Q        768
#define ENEMY_GRAVITY_Q        77
#define ENEMY_MAX_FALL_Q       768
#define PROJECTILE_GRAVITY_Q   19
#define LOW_KNIGHT_MAX_DIRTY_RECTS 16u
#define ENEMY_REDRAW_DIVIDER   2u
#define BUSH_TRACK_STOP_DISTANCE 12
#define BUSH_TURN_LOCK_FRAMES  12u
#define STRIKE_DURATION_FRAMES 5u
#define STRIKE_COOLDOWN_FRAMES 14u
#define STRIKE_HIT_START       2u
#define STRIKE_HIT_END         5u
#define STRIKE_DAMAGE          5u
#define BLOOD_COLOR            9u
#define BLOOD_REDRAW_DIVIDER   2u
#define BLOOD_PHYSICS_SUBSTEPS 2u
#define PLAYER_BLOOD_COLOR     0u
#define PLAYER_MAX_HP_BASE     5u
#define PLAYER_MAX_HP_LIMIT    6u
#define NAV_MAP_WIDTH          50
#define NAV_MAP_HEIGHT         24
#define PLAYER_INVULNERABLE_FRAMES 60u
#define PLAYER_RESPAWN_INVULNERABLE_FRAMES 90u
#define PLAYER_HURT_LOCK_FRAMES 8u
#define PLAYER_DEATH_FRAMES    45u
#define PLAYER_HURT_KNOCKBACK_Q 768
#define PLAYER_HURT_LIFT_Q     (-640)
#define PLAYER_SOUL_MAX         12u
#define PLAYER_HEAL_SOUL_COST   4u
#define PLAYER_HEAL_FRAMES      40u
#define PLAYER_FAST_HEAL_FRAMES 25u
#define PLAYER_FOCUS_INPUT_GRACE_FRAMES 3u
#define PLAYER_DOWN_STRIKE_BOUNCE_Q (-720)
#define BENCH_TILE              33u
#define REMNANT_TILE            49u
#define NAIL_ITEM_TILE          111u

#define STRIKE_HORIZONTAL       0u
#define STRIKE_UP               1u
#define STRIKE_DOWN             2u

#define ROOM_EXIT_LEFT          (1u << 0)
#define ROOM_EXIT_RIGHT         (1u << 1)
#define ROOM_EXIT_UP            (1u << 2)
#define ROOM_EXIT_DOWN          (1u << 3)

#define CHEAT_INPUT_UP          1u
#define CHEAT_INPUT_DOWN        2u
#define CHEAT_INPUT_LEFT        3u
#define CHEAT_INPUT_RIGHT       4u
#define CHEAT_SEQUENCE_LENGTH   8u

#define ABILITY_WINDGUST        (1u << 0)
#define ABILITY_LONGNAIL        (1u << 1)
#define ABILITY_FAST_HEAL       (1u << 2)
#define ABILITY_SOULSEEK        (1u << 3)
#define ABILITY_GOLDNAIL        (1u << 4)
#define ABILITY_BLOODBLOOM      (1u << 5)
#define ABILITY_GHOSTSHELL      (1u << 6)
#define ABILITY_SOULWINGS       (1u << 7)
#define ABILITY_HEART_OF_ROCK   (1u << 8)
#define ABILITY_DISCARDED_NAIL  (1u << 9)

#define COLOR_BLACK            0x0000u
#define COLOR_RESOURCE_ERROR   0xf800u

typedef struct {
    uint8_t data_x;
    uint8_t data_y;
    uint8_t base_x;
    uint8_t base_y;
    uint8_t width;
    uint8_t height;
    uint8_t has_water;
    uint8_t no_background;
} Low_Knight_Room;

typedef struct {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
} Low_Knight_Rect;

typedef struct {
    uint16_t key;
    uint8_t valid;
    uint8_t data[4];
} Tile_Row_Cache_Entry;

typedef struct {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
} Low_Knight_Box;

typedef struct {
    int32_t qx;
    int32_t qy;
    int32_t weapon_qx;
    int32_t weapon_qy;
    int16_t x;
    int16_t y;
    int16_t vx;
    int16_t vy;
    int16_t weapon_vx;
    int16_t weapon_vy;
    uint16_t timer;
    int8_t way;
    uint8_t type;
    uint8_t active;
    uint8_t state;
    uint8_t phase;
    uint8_t anim_timer;
    uint8_t hp;
} Low_Knight_Enemy;

typedef struct {
    int32_t qx;
    int32_t qy;
    int16_t x;
    int16_t y;
    int16_t vx;
    int16_t vy;
    uint16_t life;
    uint8_t tile;
    uint8_t active;
} Low_Knight_Projectile;

typedef struct {
    int32_t qx;
    int32_t qy;
    int16_t x;
    int16_t y;
    int16_t vx;
    int16_t vy;
    uint8_t life;
    uint8_t radius;
    uint8_t color;
    uint8_t active;
} Low_Knight_Blood_Particle;

typedef struct {
    int16_t x;
    int16_t y;
    uint16_t key;
    uint8_t tile;
    uint8_t active;
} Low_Knight_Item;

static const Low_Knight_Room g_rooms[] = {
    {0, 0, 84, 64, 20, 16, 1, 0},
    {33, 1, 56, 64, 28, 16, 1, 0},
    {44, 3, 0, 58, 16, 22, 1, 0},
    {20, 6, 0, 42, 16, 16, 0, 0},
    {88, 7, 36, 56, 20, 24, 1, 0},
    {15, 11, 16, 42, 20, 22, 0, 0},
    {71, 13, 16, 64, 20, 16, 0, 0},
    {86, 15, 36, 36, 20, 20, 0, 0},
    {85, 17, 56, 46, 48, 18, 0, 0},
    {89, 22, 104, 64, 16, 16, 1, 0},
    {72, 23, 104, 47, 16, 16, 0, 0},
    {98, 23, 82, 30, 29, 16, 0, 0},
    {70, 25, 56, 28, 26, 18, 0, 1},
    {64, 27, 11, 80, 23, 16, 0, 0},
    {34, 30, 0, 26, 16, 16, 0, 0},
    {101, 30, 36, 15, 20, 21, 0, 1},
    {12, 33, 56, 12, 26, 16, 0, 1},
    {23, 35, 67, 6, 0, 0, 0, 0},
    {24, 35, 82, 12, 16, 16, 0, 1},
};

/*
 * The board initializes ST7789 with MADCTL_BGR. These values are BGR565 forms
 * of the PICO-8 palette, so the panel shows the intended RGB colors.
 */
static const uint16_t g_pico_palette[16] = {
    0x0000u, 0x5143u, 0x512fu, 0x5420u, 0x3295u, 0x4aabu, 0xc618u, 0xe79fu,
    0x481fu, 0x051fu, 0x277fu, 0x2720u, 0xfd65u, 0x9bb0u, 0xabbfu, 0xae7fu,
};

static const uint8_t g_text_font[][5] = {
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c},
    {0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01},
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0x00, 0x41, 0x7f, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3f, 0x01},
    {0x7f, 0x08, 0x14, 0x22, 0x41}, {0x7f, 0x40, 0x40, 0x40, 0x40},
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06},
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f},
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
};

static const uint8_t g_enemy_tile_cache_ids[ENEMY_TILE_CACHE_COUNT] = {
    45, 59, 60, 61, 68, 69, 70, 71, 91, 92,
    172, 173, 174, 175, 187, 188, 189, 190, 191, 192,
    193, 194, 195, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 224, 225, 226, 227, 230, 231, 240,
    241, 242, 243, 246, 247, 248,
};

static Low_Knight_Resources* g_resources = NULL;
static uint8_t g_room_index;
static uint8_t g_room_exit_flags;
static Low_Knight_Room g_room;
static uint8_t g_room_tiles[LOW_KNIGHT_MAX_ROOM_W * LOW_KNIGHT_MAX_ROOM_H];
static uint8_t g_background_tiles[LOW_KNIGHT_BG_TILE_W * LOW_KNIGHT_BG_TILE_H];
static uint8_t g_tile_flags[LOW_KNIGHT_GFF_SIZE];
static Tile_Row_Cache_Entry g_tile_row_cache[TILE_ROW_CACHE_SIZE];
static uint8_t g_enemy_tile_cache[ENEMY_TILE_CACHE_COUNT][PICO_TILE_SIZE * 4u];
static Low_Knight_Vec2i g_player;
static Low_Knight_Vec2i g_last_drawn_player;
static int32_t g_player_qx;
static int32_t g_player_qy;
static int16_t g_player_vx;
static int16_t g_player_vy;
static uint8_t g_player_facing_left;
static uint8_t g_player_on_ground;
static uint8_t g_coyote_frames;
static uint8_t g_jump_buffer_frames;
static uint8_t g_player_hp;
static uint8_t g_player_invulnerable;
static uint8_t g_player_hurt_lock;
static uint8_t g_player_death_timer;
static uint8_t g_player_soul;
static uint8_t g_player_focus_timer;
static uint8_t g_player_focus_input_grace;
static uint8_t g_player_air_jumps_used;
static uint8_t g_player_air_dash_available;
static uint8_t g_player_dash_timer;
static int8_t g_player_wall_dir;
static uint16_t g_player_abilities;
static uint8_t g_nail_damage_bonus;
static uint8_t g_item_showcase_active;
static uint8_t g_item_showcase_tile;
static uint8_t g_blood_active_count;
static uint8_t g_cheat_enabled;
static uint8_t g_cheat_sequence_index;
static uint8_t g_cheat_last_direction;
static uint8_t g_respawn_room_index;
static Low_Knight_Vec2i g_respawn_player;
static uint8_t g_has_bench_respawn;
static int16_t g_camera_x;
static int16_t g_camera_y;
static Low_Knight_Enemy g_enemies[LOW_KNIGHT_MAX_ENEMIES];
static Low_Knight_Projectile g_projectiles[LOW_KNIGHT_MAX_PROJECTILES];
static Low_Knight_Blood_Particle g_blood_particles[LOW_KNIGHT_MAX_BLOOD_PARTICLES];
static Low_Knight_Item g_items[LOW_KNIGHT_MAX_ITEMS];
static uint16_t g_collected_item_keys[LOW_KNIGHT_MAX_COLLECTED_ITEMS];
static Low_Knight_Rect g_last_drawn_enemy_rects[LOW_KNIGHT_MAX_ENEMIES];
static Low_Knight_Rect g_last_drawn_projectile_rects[LOW_KNIGHT_MAX_PROJECTILES];
static uint16_t g_last_drawn_enemy_visuals[LOW_KNIGHT_MAX_ENEMIES];
static uint8_t g_last_drawn_projectile_active[LOW_KNIGHT_MAX_PROJECTILES];
static uint8_t g_enemy_count;
static uint8_t g_item_count;
static uint8_t g_item_active_count;
static uint8_t g_collected_item_count;
static Low_Knight_Rect g_pending_dirty[LOW_KNIGHT_MAX_DIRTY_RECTS];
static uint8_t g_pending_dirty_count;
static uint8_t g_dynamic_redraw_due;
static uint8_t g_strike_timer;
static uint8_t g_strike_cooldown;
static uint16_t g_strike_hit_mask;
static int16_t g_strike_x;
static int16_t g_strike_y;
static uint8_t g_strike_facing_left;
static uint8_t g_strike_direction;
static uint16_t g_effect_rng;
static uint16_t g_runtime_frame;

static uint8_t player_has_ability(uint16_t ability) {
    return (g_player_abilities & ability) != 0u;
}

static uint8_t player_max_hp(void) {
    return player_has_ability(ABILITY_BLOODBLOOM) ? PLAYER_MAX_HP_LIMIT : PLAYER_MAX_HP_BASE;
}

static uint8_t player_heal_frames(void) {
    return player_has_ability(ABILITY_FAST_HEAL) ? PLAYER_FAST_HEAL_FRAMES : PLAYER_HEAL_FRAMES;
}

static uint8_t player_strike_damage(void) {
    uint8_t damage = (uint8_t)(STRIKE_DAMAGE + g_nail_damage_bonus);
    if (player_has_ability(ABILITY_GOLDNAIL)) { damage = (uint8_t)(damage + 2u); }
    return damage;
}

static int16_t strike_reach_extra(void) {
    return player_has_ability(ABILITY_LONGNAIL) ? 2 : 0;
}

static uint8_t cheat_direction_from_input(const Low_Knight_Input* input) {
    if (input->move_y > 0 && input->move_x == 0) { return CHEAT_INPUT_UP; }
    if (input->move_y < 0 && input->move_x == 0) { return CHEAT_INPUT_DOWN; }
    if (input->move_x < 0 && input->move_y == 0) { return CHEAT_INPUT_LEFT; }
    if (input->move_x > 0 && input->move_y == 0) { return CHEAT_INPUT_RIGHT; }
    return 0u;
}

static void update_cheat_sequence(const Low_Knight_Input* input) {
    static const uint8_t sequence[CHEAT_SEQUENCE_LENGTH] = {
        CHEAT_INPUT_UP, CHEAT_INPUT_UP, CHEAT_INPUT_DOWN, CHEAT_INPUT_DOWN,
        CHEAT_INPUT_LEFT, CHEAT_INPUT_LEFT, CHEAT_INPUT_RIGHT, CHEAT_INPUT_RIGHT,
    };
    const uint8_t direction = cheat_direction_from_input(input);
    if (direction == 0u) {
        g_cheat_last_direction = 0u;
        return;
    }
    if (g_cheat_last_direction != 0u) { return; }
    g_cheat_last_direction = direction;

    if (direction == sequence[g_cheat_sequence_index]) {
        g_cheat_sequence_index++;
        if (g_cheat_sequence_index >= CHEAT_SEQUENCE_LENGTH) {
            g_cheat_enabled = 1u;
            g_cheat_sequence_index = 0u;
        }
        return;
    }

    g_cheat_sequence_index = direction == sequence[0] ? 1u : 0u;
}

static uint8_t sample_packed_pixel(const uint8_t* bytes, uint8_t pixel_index) {
    const uint8_t packed = bytes[pixel_index / 2u];
    return (pixel_index & 1u) == 0 ? (uint8_t)(packed >> 4) : (uint8_t)(packed & 0x0fu);
}

static int16_t q_to_pixel(int32_t q) {
    return (int16_t)((q + (PHYSICS_Q_ONE / 2)) >> PHYSICS_Q_SHIFT);
}

static void sync_player_from_q(void) {
    g_player.x = q_to_pixel(g_player_qx);
    g_player.y = q_to_pixel(g_player_qy);
}

static int16_t clamp_i16(int16_t value, int16_t min_value, int16_t max_value) {
    if (value < min_value) { return min_value; }
    if (value > max_value) { return max_value; }
    return value;
}

static int16_t camera_page_for_coord(int16_t coord, int16_t max_camera) {
    if (coord < 0) { return 0; }
    return clamp_i16((int16_t)(coord / PICO_SCREEN_SIZE * PICO_SCREEN_SIZE), 0, max_camera);
}

static uint8_t is_transparent_color(uint8_t color) {
    /*
     * Low Knight rebuilds PICO-8 draw palettes from the spritesheet and only
     * marks color 14 transparent. Color 0 is a visible ink color here.
     */
    return color == 14u;
}

static Low_Knight_Box player_hitbox_at(int16_t x, int16_t y) {
    return (Low_Knight_Box){
        (int16_t)(x + PLAYER_HIT_LEFT),
        (int16_t)(y + PLAYER_HIT_TOP),
        (int16_t)(x + PLAYER_HIT_RIGHT),
        (int16_t)(y + PLAYER_HIT_BOTTOM),
    };
}

static uint8_t room_contains_world_tile(uint8_t room_index, int16_t tile_x, int16_t tile_y) {
    if (room_index >= (uint8_t)(sizeof(g_rooms) / sizeof(g_rooms[0]))) { return 0; }
    const Low_Knight_Room* room = &g_rooms[room_index];
    if (room->width == 0 || room->height == 0) { return 0; }

    return tile_x >= room->base_x && tile_y >= room->base_y &&
           tile_x < (int16_t)(room->base_x + room->width) &&
           tile_y < (int16_t)(room->base_y + room->height);
}

static uint8_t find_room_for_world_pixel(int16_t world_x, int16_t world_y, uint8_t* out_room_index) {
    if (out_room_index == NULL || world_x < 0 || world_y < 0) { return 0; }

    const int16_t tile_x = (int16_t)(world_x / PICO_TILE_SIZE);
    const int16_t tile_y = (int16_t)(world_y / PICO_TILE_SIZE);
    for (uint8_t i = 0; i < (uint8_t)(sizeof(g_rooms) / sizeof(g_rooms[0])); i++) {
        if (room_contains_world_tile(i, tile_x, tile_y)) {
            *out_room_index = i;
            return 1;
        }
    }
    return 0;
}

static uint8_t room_has_neighbor_at(int16_t tile_x, int16_t tile_y) {
    for (uint8_t i = 0; i < (uint8_t)(sizeof(g_rooms) / sizeof(g_rooms[0])); i++) {
        if (i != g_room_index && room_contains_world_tile(i, tile_x, tile_y)) { return 1; }
    }
    return 0;
}

static uint8_t room_has_exit_left(void) {
    const int16_t tile_x = (int16_t)(g_room.base_x - 1);
    for (uint8_t y = 0; y < g_room.height; y++) {
        if (room_has_neighbor_at(tile_x, (int16_t)(g_room.base_y + y))) { return 1; }
    }
    return 0;
}

static uint8_t room_has_exit_right(void) {
    const int16_t tile_x = (int16_t)(g_room.base_x + g_room.width);
    for (uint8_t y = 0; y < g_room.height; y++) {
        if (room_has_neighbor_at(tile_x, (int16_t)(g_room.base_y + y))) { return 1; }
    }
    return 0;
}

static uint8_t room_has_exit_up(void) {
    const int16_t tile_y = (int16_t)(g_room.base_y - 1);
    for (uint8_t x = 0; x < g_room.width; x++) {
        if (room_has_neighbor_at((int16_t)(g_room.base_x + x), tile_y)) { return 1; }
    }
    return 0;
}

static uint8_t room_has_exit_down(void) {
    const int16_t tile_y = (int16_t)(g_room.base_y + g_room.height);
    for (uint8_t x = 0; x < g_room.width; x++) {
        if (room_has_neighbor_at((int16_t)(g_room.base_x + x), tile_y)) { return 1; }
    }
    return 0;
}

static void update_room_exit_flags(void) {
    g_room_exit_flags = 0;
    if (room_has_exit_left()) { g_room_exit_flags |= ROOM_EXIT_LEFT; }
    if (room_has_exit_right()) { g_room_exit_flags |= ROOM_EXIT_RIGHT; }
    if (room_has_exit_up()) { g_room_exit_flags |= ROOM_EXIT_UP; }
    if (room_has_exit_down()) { g_room_exit_flags |= ROOM_EXIT_DOWN; }
}

static uint8_t update_camera(uint8_t snap) {
    const int16_t old_x = g_camera_x;
    const int16_t old_y = g_camera_y;
    const int16_t max_x =
        g_room.width * PICO_TILE_SIZE > PICO_SCREEN_SIZE ? (int16_t)(g_room.width * PICO_TILE_SIZE - PICO_SCREEN_SIZE)
                                                         : 0;
    const int16_t max_y =
        g_room.height * PICO_TILE_SIZE > PICO_SCREEN_SIZE ? (int16_t)(g_room.height * PICO_TILE_SIZE - PICO_SCREEN_SIZE)
                                                          : 0;

    if (snap) {
        g_camera_x = camera_page_for_coord(g_player.x, max_x);
        g_camera_y = camera_page_for_coord(g_player.y, max_y);
    } else {
        if (g_player.x < g_camera_x || g_player.x >= (int16_t)(g_camera_x + PICO_SCREEN_SIZE)) {
            g_camera_x = camera_page_for_coord(g_player.x, max_x);
        }
        if (g_player.y < g_camera_y || g_player.y >= (int16_t)(g_camera_y + PICO_SCREEN_SIZE)) {
            g_camera_y = camera_page_for_coord(g_player.y, max_y);
        }
    }

    return old_x != g_camera_x || old_y != g_camera_y;
}

static int16_t floor_div_tile(int16_t value) {
    if (value >= 0) { return (int16_t)(value / PICO_TILE_SIZE); }
    return (int16_t)(-(((-value) + PICO_TILE_SIZE - 1) / PICO_TILE_SIZE));
}

static uint8_t tile_is_solid_at(int16_t tile_x, int16_t tile_y) {
    if (tile_x < 0 || tile_y < 0 || tile_x >= g_room.width || tile_y >= g_room.height) {
        const int16_t world_tile_x = (int16_t)(g_room.base_x + tile_x);
        const int16_t world_tile_y = (int16_t)(g_room.base_y + tile_y);
        for (uint8_t i = 0; i < (uint8_t)(sizeof(g_rooms) / sizeof(g_rooms[0])); i++) {
            if (i != g_room_index && room_contains_world_tile(i, world_tile_x, world_tile_y)) { return 0; }
        }
        return 1;
    }
    const uint8_t tile = g_room_tiles[(uint16_t)tile_y * g_room.width + (uint8_t)tile_x];
    return (g_tile_flags[tile] & 0x01u) != 0u;
}

static uint8_t box_overlaps_solid(Low_Knight_Box box) {
    const int16_t left_tile = floor_div_tile(box.left);
    const int16_t top_tile = floor_div_tile(box.top);
    const int16_t right_tile = floor_div_tile((int16_t)(box.right - 1));
    const int16_t bottom_tile = floor_div_tile((int16_t)(box.bottom - 1));

    for (int16_t ty = top_tile; ty <= bottom_tile; ty++) {
        for (int16_t tx = left_tile; tx <= right_tile; tx++) {
            if (tile_is_solid_at(tx, ty)) { return 1; }
        }
    }
    return 0;
}

static uint8_t player_is_supported(void) {
    Low_Knight_Box box = player_hitbox_at(g_player.x, g_player.y);
    box.top++;
    box.bottom++;
    return box_overlaps_solid(box);
}

static uint8_t player_touching_wall(int8_t direction) {
    if (direction == 0) { return 0; }
    Low_Knight_Box box = player_hitbox_at(g_player.x, g_player.y);
    const int16_t offset = direction > 0 ? 2 : -2;
    box.left = (int16_t)(box.left + offset);
    box.right = (int16_t)(box.right + offset);
    box.bottom--;
    return box_overlaps_solid(box);
}

static uint8_t read_map_cell(uint8_t x, uint8_t y, uint8_t* out) {
    if (out == NULL || y >= PICO_MAP_HEIGHT) {
        if (out != NULL) { *out = 0; }
        return 0;
    }
    if (y < 32u) { return Low_Knight_Resources_Read_Map(g_resources, (uint32_t)y * PICO_MAP_WIDTH + x, out, 1); }

    return Low_Knight_Resources_Read_Gfx(
        g_resources, PICO_GFX_MAP_OFFSET + (uint32_t)(y - 32u) * PICO_MAP_WIDTH + x, out, 1);
}

static uint8_t read_rle_byte(uint8_t* x, uint8_t* y, uint8_t* out) {
    if (!read_map_cell(*x, *y, out)) { return 0; }
    (*x)++;
    if (*x == PICO_MAP_RLE_WRAP_X) {
        *x = 0;
        (*y)++;
    }
    return 1;
}

static uint8_t unpack_room(const Low_Knight_Room* room) {
    if (room == NULL || room->width > LOW_KNIGHT_MAX_ROOM_W || room->height > LOW_KNIGHT_MAX_ROOM_H) {
        return 0;
    }

    memset(g_room_tiles, 0, sizeof(g_room_tiles));
    uint8_t mx = room->data_x;
    uint8_t my = room->data_y;
    uint16_t out = 0;
    const uint16_t limit = (uint16_t)room->width * room->height;

    while (out < limit) {
        uint8_t header = 0;
        if (!read_rle_byte(&mx, &my, &header)) { return 0; }

        if (header > 128u) {
            uint8_t value = 0;
            if (!read_rle_byte(&mx, &my, &value)) { return 0; }
            uint8_t length = (uint8_t)(header - 128u);
            while (length-- > 0u && out < limit) { g_room_tiles[out++] = value; }
        } else {
            while (header-- > 0u && out < limit) {
                if (!read_rle_byte(&mx, &my, &g_room_tiles[out])) { return 0; }
                out++;
            }
        }
    }
    return 1;
}

static uint8_t is_enemy_spawn_tile(uint8_t tile) {
    return tile == ENEMY_FLY_TILE || tile == ENEMY_AMBUSH_TILE || tile == ENEMY_MOSS_TILE ||
           tile == ENEMY_BEE_TILE || tile == ENEMY_LIMPER_TILE || tile == ENEMY_BUSH_TILE ||
           tile == ENEMY_FATGUY_TILE || tile == ENEMY_BALLGUY_TILE;
}

static uint8_t enemy_start_hp(uint8_t type) {
    if (type == ENEMY_BUSH_TILE || type == ENEMY_AMBUSH_TILE) { return 13u; }
    if (type == ENEMY_BEE_TILE) { return 20u; }
    if (type == ENEMY_FATGUY_TILE || type == ENEMY_MOSS_TILE) { return 30u; }
    if (type == ENEMY_BALLGUY_TILE) { return 25u; }
    return 7u;
}

static uint8_t is_item_spawn_tile(uint8_t tile) {
    return tile == 3u || tile == 19u || tile == 35u || tile == 4u || tile == 5u ||
           (tile >= 6u && tile <= 11u) || tile == NAIL_ITEM_TILE || tile == REMNANT_TILE;
}

static uint16_t item_key_for(uint8_t tile_x, uint8_t tile_y) {
    const uint16_t world_x = (uint16_t)(g_room.base_x + tile_x);
    const uint16_t world_y = (uint16_t)(g_room.base_y + tile_y);
    return (uint16_t)((world_y << 7) | world_x);
}

static uint8_t item_key_collected(uint16_t key) {
    for (uint8_t i = 0; i < g_collected_item_count; i++) {
        if (g_collected_item_keys[i] == key) { return 1; }
    }
    return 0;
}

static void remember_collected_item(uint16_t key) {
    if (item_key_collected(key) || g_collected_item_count >= LOW_KNIGHT_MAX_COLLECTED_ITEMS) { return; }
    g_collected_item_keys[g_collected_item_count++] = key;
}

static void spawn_room_enemies(void) {
    memset(g_enemies, 0, sizeof(g_enemies));
    memset(g_projectiles, 0, sizeof(g_projectiles));
    memset(g_blood_particles, 0, sizeof(g_blood_particles));
    g_blood_active_count = 0;
    g_enemy_count = 0;

    for (uint8_t tile_y = 0; tile_y < g_room.height; tile_y++) {
        for (uint8_t tile_x = 0; tile_x < g_room.width; tile_x++) {
            uint8_t* tile = &g_room_tiles[(uint16_t)tile_y * g_room.width + tile_x];
            if (!is_enemy_spawn_tile(*tile) || g_enemy_count >= LOW_KNIGHT_MAX_ENEMIES) { continue; }

            Low_Knight_Enemy* enemy = &g_enemies[g_enemy_count++];
            enemy->x = (int16_t)(tile_x * PICO_TILE_SIZE);
            enemy->y = (int16_t)(tile_y * PICO_TILE_SIZE);
            enemy->qx = (int32_t)enemy->x * PHYSICS_Q_ONE;
            enemy->qy = (int32_t)enemy->y * PHYSICS_Q_ONE;
            enemy->weapon_qx = enemy->qx;
            enemy->weapon_qy = (int32_t)(enemy->y - 4) * PHYSICS_Q_ONE;
            enemy->way = tile_x >= g_room.width / 2u ? -1 : 1;
            enemy->type = *tile;
            enemy->active = 1;
            enemy->hp = enemy_start_hp(enemy->type);
            enemy->phase = (uint8_t)(tile_x * 13u + tile_y * 7u);
            enemy->timer = (uint16_t)(20u + enemy->phase);
            *tile = enemy->type == ENEMY_AMBUSH_TILE ? 124u : 0u;
        }
    }
}

static void spawn_room_items(void) {
    memset(g_items, 0, sizeof(g_items));
    g_item_count = 0;
    g_item_active_count = 0;

    for (uint8_t tile_y = 0; tile_y < g_room.height; tile_y++) {
        for (uint8_t tile_x = 0; tile_x < g_room.width; tile_x++) {
            uint8_t* tile = &g_room_tiles[(uint16_t)tile_y * g_room.width + tile_x];
            if (!is_item_spawn_tile(*tile)) { continue; }

            const uint16_t key = item_key_for(tile_x, tile_y);
            const uint8_t item_tile = *tile;
            if (item_key_collected(key)) {
                *tile = 0u;
                continue;
            }
            if (g_item_count >= LOW_KNIGHT_MAX_ITEMS) { continue; }
            *tile = 0u;

            Low_Knight_Item* item = &g_items[g_item_count++];
            item->tile = item_tile;
            item->key = key;
            item->active = 1;
            g_item_active_count++;
            item->x = (int16_t)(tile_x * PICO_TILE_SIZE + 4);
            item->y = (int16_t)(tile_y * PICO_TILE_SIZE + 12);
            if (item_tile == NAIL_ITEM_TILE) { item->y = (int16_t)(tile_y * PICO_TILE_SIZE + 20); }
            if (item_tile == REMNANT_TILE) {
                item->x = (int16_t)(tile_x * PICO_TILE_SIZE - 1);
                item->y = (int16_t)(tile_y * PICO_TILE_SIZE + 4);
            }
        }
    }
}

static int8_t enemy_tile_cache_slot(uint8_t tile) {
    int8_t low = 0;
    int8_t high = (int8_t)(ENEMY_TILE_CACHE_COUNT - 1u);
    while (low <= high) {
        const int8_t middle = (int8_t)(low + (high - low) / 2);
        const uint8_t candidate = g_enemy_tile_cache_ids[(uint8_t)middle];
        if (candidate == tile) { return middle; }
        if (candidate < tile) {
            low = (int8_t)(middle + 1);
        } else {
            high = (int8_t)(middle - 1);
        }
    }
    return -1;
}

static uint8_t cache_enemy_tiles(void) {
    for (uint8_t slot = 0; slot < ENEMY_TILE_CACHE_COUNT; slot++) {
        const uint8_t tile = g_enemy_tile_cache_ids[slot];
        const uint8_t tile_x = tile & 0x0fu;
        const uint8_t tile_y = tile >> 4;
        for (uint8_t row = 0; row < PICO_TILE_SIZE; row++) {
            const uint32_t offset =
                ((uint32_t)tile_y * PICO_TILE_SIZE + row) * PICO_GFX_STRIDE_BYTES +
                (uint32_t)tile_x * 4u;
            if (!Low_Knight_Resources_Read_Gfx(
                    g_resources, offset, &g_enemy_tile_cache[slot][row * 4u], 4u)) {
                return 0;
            }
        }
    }
    return 1;
}

static uint8_t read_gfx_tile_row(uint8_t tile, uint8_t row, uint8_t* out_row) {
    if (out_row == NULL) { return 0; }

    const int8_t enemy_slot = enemy_tile_cache_slot(tile);
    if (enemy_slot >= 0) {
        memcpy(out_row, &g_enemy_tile_cache[(uint8_t)enemy_slot][row * 4u], 4u);
        return 1;
    }

    const uint16_t key = (uint16_t)tile * PICO_TILE_SIZE + row;
    Tile_Row_Cache_Entry* entry = &g_tile_row_cache[key & (TILE_ROW_CACHE_SIZE - 1u)];
    if (entry->valid && entry->key == key) {
        memcpy(out_row, entry->data, sizeof(entry->data));
        return 1;
    }

    const uint8_t tile_x = tile & 0x0fu;
    const uint8_t tile_y = tile >> 4;
    const uint32_t offset =
        ((uint32_t)tile_y * PICO_TILE_SIZE + row) * PICO_GFX_STRIDE_BYTES + (uint32_t)tile_x * 4u;
    if (!Low_Knight_Resources_Read_Gfx(g_resources, offset, out_row, 4)) { return 0; }

    entry->key = key;
    entry->valid = 1;
    memcpy(entry->data, out_row, sizeof(entry->data));
    return 1;
}

static uint8_t cache_background_map(void) {
    for (uint8_t y = 0; y < LOW_KNIGHT_BG_TILE_H; y++) {
        for (uint8_t x = 0; x < LOW_KNIGHT_BG_TILE_W; x++) {
            if (!read_map_cell((uint8_t)(LOW_KNIGHT_BG_TILE_X + x), (uint8_t)(LOW_KNIGHT_BG_TILE_Y + y),
                    &g_background_tiles[(uint16_t)y * LOW_KNIGHT_BG_TILE_W + x])) {
                return 0;
            }
        }
    }
    return 1;
}

static uint8_t load_room(uint8_t room_index) {
    if (room_index >= (uint8_t)(sizeof(g_rooms) / sizeof(g_rooms[0]))) { return 0; }
    if (g_rooms[room_index].width == 0 || g_rooms[room_index].height == 0) { return 0; }

    g_room_index = room_index;
    g_room = g_rooms[g_room_index];
    update_room_exit_flags();
    if (!unpack_room(&g_room)) { return 0; }
    spawn_room_enemies();
    spawn_room_items();
    return g_room.no_background || cache_background_map();
}

static Low_Knight_Rect screen_rect(void) {
    const int16_t view_w = (int16_t)(PICO_SCREEN_SIZE * 2);
    const int16_t view_h = (int16_t)(PICO_SCREEN_SIZE * 2);
    return (Low_Knight_Rect){
        (int16_t)((SCREEN_WIDTH - view_w) / 2),
        (int16_t)((SCREEN_HEIGHT - view_h) / 2),
        (int16_t)((SCREEN_WIDTH - view_w) / 2 + view_w),
        (int16_t)((SCREEN_HEIGHT - view_h) / 2 + view_h),
    };
}

static Low_Knight_Rect rect_intersect(Low_Knight_Rect a, Low_Knight_Rect b) {
    Low_Knight_Rect r = {
        a.x1 > b.x1 ? a.x1 : b.x1,
        a.y1 > b.y1 ? a.y1 : b.y1,
        a.x2 < b.x2 ? a.x2 : b.x2,
        a.y2 < b.y2 ? a.y2 : b.y2,
    };
    if (r.x2 < r.x1) { r.x2 = r.x1; }
    if (r.y2 < r.y1) { r.y2 = r.y1; }
    return r;
}

static Low_Knight_Rect rect_union(Low_Knight_Rect a, Low_Knight_Rect b) {
    return (Low_Knight_Rect){
        a.x1 < b.x1 ? a.x1 : b.x1,
        a.y1 < b.y1 ? a.y1 : b.y1,
        a.x2 > b.x2 ? a.x2 : b.x2,
        a.y2 > b.y2 ? a.y2 : b.y2,
    };
}

static uint8_t rect_is_empty(Low_Knight_Rect r) { return r.x1 >= r.x2 || r.y1 >= r.y2; }

static uint8_t rects_touch(Low_Knight_Rect a, Low_Knight_Rect b) {
    return a.x1 <= b.x2 + 2 && b.x1 <= a.x2 + 2 && a.y1 <= b.y2 + 2 && b.y1 <= a.y2 + 2;
}

static uint32_t rect_area(Low_Knight_Rect rect) {
    if (rect_is_empty(rect)) { return 0; }
    return (uint32_t)(rect.x2 - rect.x1) * (uint32_t)(rect.y2 - rect.y1);
}

static void mark_dirty_rect(Low_Knight_Rect rect) {
    rect = rect_intersect(rect, screen_rect());
    rect = rect_intersect(rect, (Low_Knight_Rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT});
    if (rect_is_empty(rect)) { return; }

    for (uint8_t i = 0; i < g_pending_dirty_count; i++) {
        if (!rects_touch(g_pending_dirty[i], rect)) { continue; }

        g_pending_dirty[i] = rect_union(g_pending_dirty[i], rect);
        for (uint8_t j = (uint8_t)(i + 1); j < g_pending_dirty_count;) {
            if (!rects_touch(g_pending_dirty[i], g_pending_dirty[j])) {
                j++;
                continue;
            }
            g_pending_dirty[i] = rect_union(g_pending_dirty[i], g_pending_dirty[j]);
            g_pending_dirty[j] = g_pending_dirty[--g_pending_dirty_count];
        }
        return;
    }

    if (g_pending_dirty_count < LOW_KNIGHT_MAX_DIRTY_RECTS) {
        g_pending_dirty[g_pending_dirty_count++] = rect;
        return;
    }

    uint8_t best = 0;
    uint32_t best_growth = UINT32_MAX;
    for (uint8_t i = 0; i < g_pending_dirty_count; i++) {
        const Low_Knight_Rect combined = rect_union(g_pending_dirty[i], rect);
        const uint32_t growth = rect_area(combined) - rect_area(g_pending_dirty[i]);
        if (growth < best_growth) {
            best = i;
            best_growth = growth;
        }
    }
    g_pending_dirty[best] = rect_union(g_pending_dirty[best], rect);
}

static Low_Knight_Rect enemy_rect_for(const Low_Knight_Enemy* enemy) {
    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    int16_t left = -10;
    int16_t top = -10;
    int16_t right = 10;
    int16_t bottom = 5;
    if (enemy->type == ENEMY_AMBUSH_TILE) { return (Low_Knight_Rect){0, 0, 0, 0}; }
    if (enemy->type == ENEMY_BEE_TILE) {
        left = -12;
        top = -12;
        right = 12;
        bottom = 8;
    } else if (enemy->type == ENEMY_BUSH_TILE) {
        left = -14;
        top = -8;
        right = 14;
        bottom = 6;
    } else if (enemy->type == ENEMY_FATGUY_TILE || enemy->type == ENEMY_BALLGUY_TILE ||
               enemy->type == ENEMY_MOSS_TILE) {
        left = -10;
        top = -18;
        right = 10;
        bottom = 5;
    }

    if (enemy->type == ENEMY_BALLGUY_TILE || enemy->type == ENEMY_MOSS_TILE) {
        const int16_t weapon_x = q_to_pixel(enemy->weapon_qx);
        const int16_t weapon_y = q_to_pixel(enemy->weapon_qy);
        const int16_t weapon_left = (int16_t)(weapon_x - enemy->x - 5);
        const int16_t weapon_top = (int16_t)(weapon_y - enemy->y - 5);
        const int16_t weapon_right = (int16_t)(weapon_x - enemy->x + 5);
        const int16_t weapon_bottom = (int16_t)(weapon_y - enemy->y + 5);
        if (weapon_left < left) { left = weapon_left; }
        if (weapon_top < top) { top = weapon_top; }
        if (weapon_right > right) { right = weapon_right; }
        if (weapon_bottom > bottom) { bottom = weapon_bottom; }
    }

    const int16_t x = (int16_t)(view.x1 + (enemy->x - g_camera_x) * scale);
    const int16_t y = (int16_t)(view.y1 + (enemy->y - g_camera_y) * scale);
    return (Low_Knight_Rect){
        (int16_t)(x + left * scale - 2),
        (int16_t)(y + top * scale - 2),
        (int16_t)(x + right * scale + 2),
        (int16_t)(y + bottom * scale + 2),
    };
}

static Low_Knight_Rect projectile_rect_for(const Low_Knight_Projectile* projectile) {
    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    const int16_t x = (int16_t)(view.x1 + (projectile->x - g_camera_x) * scale);
    const int16_t y = (int16_t)(view.y1 + (projectile->y - g_camera_y) * scale);
    return (Low_Knight_Rect){
        (int16_t)(x - 5 * scale),
        (int16_t)(y - 5 * scale),
        (int16_t)(x + 5 * scale),
        (int16_t)(y + 5 * scale),
    };
}

static Low_Knight_Rect item_rect_for(const Low_Knight_Item* item) {
    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    const int16_t x = (int16_t)(view.x1 + (item->x - g_camera_x) * scale);
    const int16_t y = (int16_t)(view.y1 + (item->y - g_camera_y) * scale);
    if (item->tile == NAIL_ITEM_TILE) {
        return (Low_Knight_Rect){
            (int16_t)(x - 2),
            (int16_t)(y - 16 * scale - 2),
            (int16_t)(x + 8 * scale + 2),
            (int16_t)(y + 2),
        };
    }
    return (Low_Knight_Rect){
        (int16_t)(x - 8 * scale - 2),
        (int16_t)(y - 8 * scale - 2),
        (int16_t)(x + 8 * scale + 2),
        (int16_t)(y + 8 * scale + 2),
    };
}

static uint8_t rect_equals(Low_Knight_Rect a, Low_Knight_Rect b) {
    return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2;
}

static Low_Knight_Rect rect_union_visible(Low_Knight_Rect a, Low_Knight_Rect b) {
    if (rect_is_empty(a)) { return b; }
    if (rect_is_empty(b)) { return a; }
    return rect_union(a, b);
}

static uint16_t enemy_visual_key(const Low_Knight_Enemy* enemy) {
    uint16_t key = enemy->type;
    if (enemy->way < 0) { key |= 1u << 8; }
    key |= (uint16_t)(enemy->state & 0x03u) << 9;
    if (enemy->type == ENEMY_FLY_TILE) {
        key |= (uint16_t)(((g_runtime_frame + enemy->phase) / 3u) & 1u) << 11;
    } else if (enemy->type == ENEMY_BEE_TILE) {
        key |= (uint16_t)(((g_runtime_frame + enemy->phase) >> 1) & 1u) << 11;
    }
    return key;
}

static void capture_dynamic_draw_state(void) {
    for (uint8_t i = 0; i < LOW_KNIGHT_MAX_ENEMIES; i++) {
        if (i < g_enemy_count && g_enemies[i].active) {
            g_last_drawn_enemy_rects[i] = enemy_rect_for(&g_enemies[i]);
            g_last_drawn_enemy_visuals[i] = enemy_visual_key(&g_enemies[i]);
        } else {
            g_last_drawn_enemy_rects[i] = (Low_Knight_Rect){0, 0, 0, 0};
            g_last_drawn_enemy_visuals[i] = 0;
        }
    }

    for (uint8_t i = 0; i < LOW_KNIGHT_MAX_PROJECTILES; i++) {
        if (g_projectiles[i].active) {
            g_last_drawn_projectile_rects[i] = projectile_rect_for(&g_projectiles[i]);
            g_last_drawn_projectile_active[i] = 1;
        } else {
            g_last_drawn_projectile_rects[i] = (Low_Knight_Rect){0, 0, 0, 0};
            g_last_drawn_projectile_active[i] = 0;
        }
    }
}

static void queue_dynamic_dirty(void) {
    for (uint8_t i = 0; i < g_enemy_count; i++) {
        const Low_Knight_Enemy* enemy = &g_enemies[i];
        const Low_Knight_Rect current = enemy->active
                                            ? enemy_rect_for(enemy)
                                            : (Low_Knight_Rect){0, 0, 0, 0};
        const uint16_t visual = enemy->active ? enemy_visual_key(enemy) : 0;
        const uint8_t tether_moves =
            enemy->type == ENEMY_BALLGUY_TILE || enemy->type == ENEMY_MOSS_TILE;
        if (!rect_equals(current, g_last_drawn_enemy_rects[i]) ||
            visual != g_last_drawn_enemy_visuals[i] || tether_moves) {
            mark_dirty_rect(rect_union_visible(g_last_drawn_enemy_rects[i], current));
        }
    }

    for (uint8_t i = 0; i < LOW_KNIGHT_MAX_PROJECTILES; i++) {
        const Low_Knight_Projectile* projectile = &g_projectiles[i];
        const Low_Knight_Rect current = projectile->active
                                            ? projectile_rect_for(projectile)
                                            : (Low_Knight_Rect){0, 0, 0, 0};
        if (projectile->active != g_last_drawn_projectile_active[i] ||
            !rect_equals(current, g_last_drawn_projectile_rects[i])) {
            mark_dirty_rect(rect_union_visible(g_last_drawn_projectile_rects[i], current));
        }
    }
}

static void compose_sprite_rect_line_ex(uint16_t* line, int16_t region_x, int16_t region_width,
    int16_t screen_y, uint8_t tile, uint8_t width_tiles, uint8_t height_tiles, int16_t sprite_x,
    int16_t sprite_y, uint8_t scale, uint8_t hflip, uint8_t vflip) {
    const int16_t local_y = (int16_t)(screen_y - sprite_y);
    if (local_y < 0 || local_y >= (int16_t)(height_tiles * PICO_TILE_SIZE * scale)) { return; }

    uint8_t source_y = (uint8_t)(local_y / scale);
    if (vflip) { source_y = (uint8_t)(height_tiles * PICO_TILE_SIZE - 1u - source_y); }
    const uint8_t tile_y = (uint8_t)(source_y / PICO_TILE_SIZE);
    const uint8_t tile_row = (uint8_t)(source_y & 7u);
    const int16_t sprite_width = (int16_t)(width_tiles * PICO_TILE_SIZE * scale);
    const int16_t start = sprite_x > region_x ? sprite_x : region_x;
    const int16_t end = sprite_x + sprite_width < region_x + region_width
                            ? (int16_t)(sprite_x + sprite_width)
                            : (int16_t)(region_x + region_width);

    uint8_t cached_tile = 0xffu;
    uint8_t row_data[4];
    for (int16_t screen_x = start; screen_x < end; screen_x++) {
        const int16_t local_x = (int16_t)(screen_x - sprite_x);
        if (local_x < 0 || local_x >= sprite_width) { continue; }

        uint8_t source_x = (uint8_t)(local_x / scale);
        if (hflip) { source_x = (uint8_t)(width_tiles * PICO_TILE_SIZE - 1u - source_x); }

        const uint8_t tile_x = (uint8_t)(source_x / PICO_TILE_SIZE);
        const uint8_t draw_tile = (uint8_t)(tile + tile_y * 16u + tile_x);
        if (draw_tile != cached_tile) {
            if (!read_gfx_tile_row(draw_tile, tile_row, row_data)) { return; }
            cached_tile = draw_tile;
        }

        const uint8_t color = sample_packed_pixel(row_data, (uint8_t)(source_x & 7u));
        if (!is_transparent_color(color)) { line[screen_x - region_x] = g_pico_palette[color]; }
    }
}

static void compose_sprite_rect_line(uint16_t* line, int16_t region_x, int16_t region_width,
    int16_t screen_y, uint8_t tile, uint8_t width_tiles, uint8_t height_tiles, int16_t sprite_x,
    int16_t sprite_y, uint8_t scale, uint8_t hflip) {
    compose_sprite_rect_line_ex(line, region_x, region_width, screen_y, tile, width_tiles,
        height_tiles, sprite_x, sprite_y, scale, hflip, 0);
}

static void compose_enemies_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;

    for (uint8_t i = 0; i < g_enemy_count; i++) {
        const Low_Knight_Enemy* enemy = &g_enemies[i];
        if (!enemy->active || enemy->type == ENEMY_AMBUSH_TILE) { continue; }

        const int16_t x = (int16_t)(view.x1 + (enemy->x - g_camera_x) * scale);
        const int16_t y = (int16_t)(view.y1 + (enemy->y - g_camera_y) * scale);
        const uint8_t flip = enemy->way < 0;
        if (enemy->type == ENEMY_FLY_TILE) {
            const uint8_t tile = ((g_runtime_frame + enemy->phase) / 3u) & 1u ? 68u : 70u;
            compose_sprite_rect_line(line, region_x, region_width, screen_y, tile, 2, 1,
                (int16_t)(x - 8 * scale), (int16_t)(y - 6 * scale), scale, flip);
        } else if (enemy->type == ENEMY_BEE_TILE) {
            const int16_t wing_lift = ((g_runtime_frame + enemy->phase) & 2u) ? -1 * scale : 0;
            compose_sprite_rect_line(line, region_x, region_width, screen_y, 193, 1, 1,
                (int16_t)(x + 3 * scale), (int16_t)(y - 7 * scale + wing_lift), scale, 0);
            compose_sprite_rect_line(line, region_x, region_width, screen_y, 193, 1, 1,
                (int16_t)(x - 10 * scale), (int16_t)(y - 7 * scale + wing_lift), scale, 1);
            compose_sprite_rect_line(line, region_x, region_width, screen_y,
                enemy->state == 1u ? 208u : 192u, 1, 1, (int16_t)(x - 4 * scale),
                (int16_t)(y - 4 * scale), scale, flip);
        } else if (enemy->type == ENEMY_BUSH_TILE) {
            const int16_t trail = (int16_t)(enemy->way * -3 * scale);
            compose_sprite_rect_line(line, region_x, region_width, screen_y, 205, 1, 1,
                (int16_t)(x - 4 * scale + trail * 2), (int16_t)(y - 4 * scale), scale, flip);
            compose_sprite_rect_line(line, region_x, region_width, screen_y, 206, 1, 1,
                (int16_t)(x - 4 * scale + trail), (int16_t)(y - 4 * scale), scale, flip);
            compose_sprite_rect_line(line, region_x, region_width, screen_y, 207, 1, 1,
                (int16_t)(x - 4 * scale), (int16_t)(y - 4 * scale), scale, flip);
        } else if (enemy->type == ENEMY_LIMPER_TILE) {
            compose_sprite_rect_line(line, region_x, region_width, screen_y, 203, 2, 1,
                (int16_t)(x - 8 * scale), (int16_t)(y - 8 * scale), scale, flip);
        } else if (enemy->type == ENEMY_FATGUY_TILE) {
            compose_sprite_rect_line(line, region_x, region_width, screen_y,
                enemy->state == 1u ? 226u : 224u, 2, 2, (int16_t)(x - 8 * scale),
                (int16_t)(y - 16 * scale), scale, flip);
            compose_sprite_rect_line(line, region_x, region_width, screen_y,
                enemy->state == 2u ? 209u : 210u, 1, 1, (int16_t)(x - 4 * scale),
                (int16_t)(y - 16 * scale), scale, flip);
        } else if (enemy->type == ENEMY_BALLGUY_TILE || enemy->type == ENEMY_MOSS_TILE) {
            const uint8_t body_tile = enemy->type == ENEMY_MOSS_TILE
                                          ? (enemy->state == 1u ? 174u : 172u)
                                          : 230u;
            compose_sprite_rect_line(line, region_x, region_width, screen_y, body_tile, 2, 2,
                (int16_t)(x - 8 * scale), (int16_t)(y - 16 * scale), scale, flip);
            if (enemy->type == ENEMY_BALLGUY_TILE) {
                compose_sprite_rect_line(line, region_x, region_width, screen_y, 248, 1, 1,
                    (int16_t)(x - 4 * scale), (int16_t)(y - 14 * scale), scale, flip);
            }

            const int16_t weapon_world_x = q_to_pixel(enemy->weapon_qx);
            const int16_t weapon_world_y = q_to_pixel(enemy->weapon_qy);
            const int16_t weapon_x = (int16_t)(view.x1 + (weapon_world_x - g_camera_x) * scale);
            const int16_t weapon_y = (int16_t)(view.y1 + (weapon_world_y - g_camera_y) * scale);
            if (enemy->type == ENEMY_BALLGUY_TILE) {
                const int16_t anchor_x = (int16_t)(x - enemy->way * 4 * scale);
                const int16_t anchor_y = (int16_t)(y - 4 * scale);
                for (uint8_t link = 1; link <= 3u; link++) {
                    const int16_t link_x =
                        (int16_t)(weapon_x + (anchor_x - weapon_x) * link / 4 - 4 * scale);
                    const int16_t link_y =
                        (int16_t)(weapon_y + (anchor_y - weapon_y) * link / 4 - 4 * scale);
                    compose_sprite_rect_line(
                        line, region_x, region_width, screen_y, 212, 1, 1, link_x, link_y, scale, 0);
                }
            }
            compose_sprite_rect_line(line, region_x, region_width, screen_y,
                enemy->type == ENEMY_MOSS_TILE ? 187u : 211u, 1, 1,
                (int16_t)(weapon_x - 4 * scale), (int16_t)(weapon_y - 4 * scale), scale, flip);
        }
    }

    for (uint8_t i = 0; i < LOW_KNIGHT_MAX_PROJECTILES; i++) {
        const Low_Knight_Projectile* projectile = &g_projectiles[i];
        if (!projectile->active) { continue; }
        const int16_t x = (int16_t)(view.x1 + (projectile->x - g_camera_x) * scale);
        const int16_t y = (int16_t)(view.y1 + (projectile->y - g_camera_y) * scale);
        compose_sprite_rect_line(line, region_x, region_width, screen_y, projectile->tile, 1, 1,
            (int16_t)(x - 4 * scale), (int16_t)(y - 4 * scale), scale, projectile->vx < 0);
    }
}

static void compose_items_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    if (g_item_active_count == 0) { return; }

    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;

    for (uint8_t i = 0; i < g_item_count; i++) {
        const Low_Knight_Item* item = &g_items[i];
        if (!item->active) { continue; }

        const int16_t x = (int16_t)(view.x1 + (item->x - g_camera_x) * scale);
        const int16_t y = (int16_t)(view.y1 + (item->y - g_camera_y) * scale);
        if (item->tile == NAIL_ITEM_TILE) {
            compose_sprite_rect_line(line, region_x, region_width, screen_y, 95u, 1, 2,
                x, (int16_t)(y - 16 * scale), scale, 0);
        } else if (item->tile == REMNANT_TILE) {
            compose_sprite_rect_line(line, region_x, region_width, screen_y, REMNANT_TILE, 1, 1,
                (int16_t)(x - 4 * scale), (int16_t)(y - 4 * scale), scale, 0);
        } else {
            compose_sprite_rect_line(line, region_x, region_width, screen_y, item->tile, 1, 1,
                (int16_t)(x - 4 * scale), (int16_t)(y - 4 * scale), scale, 0);
            compose_sprite_rect_line(line, region_x, region_width, screen_y, 41u, 2, 2,
                (int16_t)(x - 8 * scale), (int16_t)(y - 8 * scale), scale, 0);
        }
    }
}

static void compose_map_region_line(uint16_t* line, int16_t region_x, int16_t region_width,
    int16_t screen_y, uint8_t map_x, uint8_t map_y, int16_t screen_x, int16_t map_screen_y,
    uint8_t width, uint8_t height, uint8_t scale) {
    const int16_t local_y = (int16_t)(screen_y - map_screen_y);
    if (local_y < 0 || local_y >= (int16_t)(height * PICO_TILE_SIZE * scale)) { return; }

    const uint8_t tile_y = (uint8_t)(local_y / (PICO_TILE_SIZE * scale));
    const uint8_t tile_row = (uint8_t)((local_y / scale) & 7);

    int16_t x = region_x;
    while (x < region_x + region_width) {
        const int16_t local_x = (int16_t)(x - screen_x);
        if (local_x < 0 || local_x >= (int16_t)(width * PICO_TILE_SIZE * scale)) {
            x++;
            continue;
        }

        const uint8_t tile_x = (uint8_t)(local_x / (PICO_TILE_SIZE * scale));
        uint8_t tile = 0;
        if (map_x == LOW_KNIGHT_BG_TILE_X && map_y == LOW_KNIGHT_BG_TILE_Y &&
            tile_x < LOW_KNIGHT_BG_TILE_W && tile_y < LOW_KNIGHT_BG_TILE_H) {
            tile = g_background_tiles[(uint16_t)tile_y * LOW_KNIGHT_BG_TILE_W + tile_x];
        } else if (!read_map_cell((uint8_t)(map_x + tile_x), (uint8_t)(map_y + tile_y), &tile)) {
            x++;
            continue;
        }
        if (tile == 0u) {
            x++;
            continue;
        }

        uint8_t row_data[4];
        if (!read_gfx_tile_row(tile, tile_row, row_data)) { return; }

        const int16_t tile_screen_x = (int16_t)(screen_x + tile_x * PICO_TILE_SIZE * scale);
        const int16_t tile_end = (int16_t)(tile_screen_x + PICO_TILE_SIZE * scale);
        const int16_t end = tile_end < region_x + region_width ? tile_end : (int16_t)(region_x + region_width);
        for (; x < end; x++) {
            const int16_t tile_local_x = (int16_t)(x - tile_screen_x);
            const uint8_t color = sample_packed_pixel(row_data, (uint8_t)(tile_local_x / scale));
            if (!is_transparent_color(color)) { line[x - region_x] = g_pico_palette[color]; }
        }
    }
}

static void compose_background_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    if (g_room.no_background) { return; }

    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    const uint8_t map_height = g_room.has_water ? 20u : 10u;

    compose_map_region_line(line, region_x, region_width, screen_y, LOW_KNIGHT_BG_TILE_X, LOW_KNIGHT_BG_TILE_Y,
        (int16_t)(view.x1 - 2 * scale - g_camera_x), (int16_t)(view.y1 - g_camera_y), 19, map_height, scale);
    compose_map_region_line(line, region_x, region_width, screen_y, LOW_KNIGHT_BG_TILE_X, LOW_KNIGHT_BG_TILE_Y,
        (int16_t)(view.x1 - 2 * scale - g_camera_x / 2),
        (int16_t)(view.y1 + 4 * scale - g_camera_y / 2), 19, map_height, scale);
}

static void compose_level_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    const int16_t pico_y = (int16_t)((screen_y - view.y1) / scale + g_camera_y);
    if (pico_y < 0 || pico_y >= (int16_t)(g_room.height * PICO_TILE_SIZE)) { return; }

    const uint8_t tile_y = (uint8_t)(pico_y / PICO_TILE_SIZE);
    if (tile_y >= g_room.height) { return; }
    const uint8_t tile_row = (uint8_t)(pico_y & 7);

    int16_t x = region_x;
    while (x < region_x + region_width) {
        const int16_t pico_x = (int16_t)((x - view.x1) / scale + g_camera_x);
        if (pico_x < 0 || pico_x >= (int16_t)(g_room.width * PICO_TILE_SIZE)) {
            x++;
            continue;
        }

        const uint8_t tile_x = (uint8_t)(pico_x / PICO_TILE_SIZE);
        if (tile_x >= g_room.width) {
            x++;
            continue;
        }

        const uint8_t tile = g_room_tiles[(uint16_t)tile_y * g_room.width + tile_x];
        if (tile == 0u) {
            x++;
            continue;
        }

        uint8_t row_data[4];
        if (!read_gfx_tile_row(tile, tile_row, row_data)) { return; }

        const int16_t tile_screen_x = (int16_t)(view.x1 + (tile_x * PICO_TILE_SIZE - g_camera_x) * scale);
        const int16_t tile_end = (int16_t)(tile_screen_x + PICO_TILE_SIZE * scale);
        const int16_t end = tile_end < region_x + region_width ? tile_end : (int16_t)(region_x + region_width);
        for (; x < end; x++) {
            const int16_t local_x = (int16_t)(x - tile_screen_x);
            const uint8_t color = sample_packed_pixel(row_data, (uint8_t)(local_x / scale));
            if (!is_transparent_color(color)) { line[x - region_x] = g_pico_palette[color]; }
        }
    }
}

static void compose_player_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    if (g_player_death_timer > 0 ||
        (g_player_invulnerable > 0 && (g_runtime_frame & 3u) < 2u)) {
        return;
    }

    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    const int16_t x = (int16_t)(view.x1 + (g_player.x - g_camera_x) * scale);
    const int16_t y = (int16_t)(view.y1 + (g_player.y - g_camera_y) * scale);
    uint8_t body_tile = g_player_vx != 0 ? 76u : 78u;
    if (g_player_dash_timer > 0) { body_tile = 72u; }
    if (g_player_wall_dir != 0) { body_tile = 74u; }

    compose_sprite_rect_line(line, region_x, region_width, screen_y, body_tile, 2, 1,
        (int16_t)(x - 8 * scale), (int16_t)(y - 4 * scale), scale, g_player_facing_left);
    compose_sprite_rect_line(line, region_x, region_width, screen_y, 47, 1, 1,
        (int16_t)(x - 4 * scale), (int16_t)(y - 12 * scale), scale, g_player_facing_left);
}

static void compose_strike_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    if (g_strike_timer == 0) { return; }

    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    const int16_t player_x = (int16_t)(view.x1 + (g_strike_x - g_camera_x) * scale);
    const int16_t player_y = (int16_t)(view.y1 + (g_strike_y - g_camera_y) * scale);
    if (g_strike_direction == STRIKE_HORIZONTAL) {
        const int16_t reach = (int16_t)(12 + strike_reach_extra());
        const int16_t x =
            (int16_t)(player_x + (g_strike_facing_left ? -reach : reach) * scale);
        const int16_t y = (int16_t)(player_y - 4 * scale);
        compose_sprite_rect_line(line, region_x, region_width, screen_y, 59, 2, 1,
            (int16_t)(x - 8 * scale), (int16_t)(y - 4 * scale), scale, g_strike_facing_left);
    } else {
        const uint8_t down = g_strike_direction == STRIKE_DOWN;
        const int16_t y = down ? (int16_t)(player_y + 2 * scale)
                               : (int16_t)(player_y - 26 * scale);
        compose_sprite_rect_line_ex(line, region_x, region_width, screen_y, 45, 1, 2,
            (int16_t)(player_x - 4 * scale), y, scale, g_strike_facing_left, down);
    }
}

static void compose_blood_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    if (g_blood_active_count == 0) { return; }

    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    for (uint8_t i = 0; i < LOW_KNIGHT_MAX_BLOOD_PARTICLES; i++) {
        const Low_Knight_Blood_Particle* particle = &g_blood_particles[i];
        if (!particle->active) { continue; }

        int16_t radius = (int16_t)(particle->radius * scale);
        if (particle->life < 6u) {
            radius = (int16_t)(radius * particle->life / 6u);
            if (radius < 1) { radius = 1; }
        }
        const int16_t center_x = (int16_t)(view.x1 + (particle->x - g_camera_x) * scale);
        const int16_t center_y = (int16_t)(view.y1 + (particle->y - g_camera_y) * scale);
        const int16_t dy = (int16_t)(screen_y - center_y);
        if (dy < -radius || dy > radius) { continue; }

        const int16_t radius_sq = (int16_t)(radius * radius);
        int16_t start = (int16_t)(center_x - radius);
        int16_t end = (int16_t)(center_x + radius + 1);
        if (start < region_x) { start = region_x; }
        if (end > region_x + region_width) { end = (int16_t)(region_x + region_width); }
        for (int16_t screen_x = start; screen_x < end; screen_x++) {
            const int16_t dx = (int16_t)(screen_x - center_x);
            if (dx * dx + dy * dy <= radius_sq) {
                line[screen_x - region_x] = g_pico_palette[particle->color];
            }
        }
    }
}

static const uint8_t* text_glyph_for(char character) {
    if (character >= 'a' && character <= 'z') { character = (char)(character - 'a' + 'A'); }
    if (character >= 'A' && character <= 'Z') { return g_text_font[character - 'A']; }
    return NULL;
}

static int16_t text_width(const char* text, uint8_t scale) {
    int16_t width = 0;
    while (*text != '\0') {
        width = (int16_t)(width + 6 * scale);
        text++;
    }
    return width > 0 ? (int16_t)(width - scale) : 0;
}

static void compose_text_line(uint16_t* line, int16_t region_x, int16_t region_width,
    int16_t screen_y, int16_t text_x, int16_t text_y, const char* text, uint8_t scale,
    uint16_t color) {
    const int16_t local_y = (int16_t)(screen_y - text_y);
    if (text == NULL || scale == 0 || local_y < 0 || local_y >= (int16_t)(7 * scale)) { return; }

    const uint8_t glyph_row = (uint8_t)(local_y / scale);
    while (*text != '\0') {
        const int16_t glyph_x = text_x;
        if (*text == '-') {
            if (glyph_row == 3u) {
                for (uint8_t col = 0; col < 5u; col++) {
                    for (uint8_t sx = 0; sx < scale; sx++) {
                        const int16_t screen_x = (int16_t)(glyph_x + col * scale + sx);
                        if (screen_x >= region_x && screen_x < region_x + region_width) {
                            line[screen_x - region_x] = color;
                        }
                    }
                }
            }
        } else {
            const uint8_t* glyph = text_glyph_for(*text);
            if (glyph != NULL) {
                for (uint8_t col = 0; col < 5u; col++) {
                    if ((glyph[col] & (1u << glyph_row)) == 0u) { continue; }
                    for (uint8_t sx = 0; sx < scale; sx++) {
                        const int16_t screen_x = (int16_t)(glyph_x + col * scale + sx);
                        if (screen_x >= region_x && screen_x < region_x + region_width) {
                            line[screen_x - region_x] = color;
                        }
                    }
                }
            }
        }
        text_x = (int16_t)(text_x + 6 * scale);
        text++;
    }
}

static void compose_centered_text_line(uint16_t* line, int16_t region_x, int16_t region_width,
    int16_t screen_y, int16_t center_x, int16_t text_y, const char* text, uint8_t scale,
    uint16_t color) {
    compose_text_line(line, region_x, region_width, screen_y,
        (int16_t)(center_x - text_width(text, scale) / 2), text_y, text, scale, color);
}

static const char* item_showcase_name(uint8_t tile) {
    switch (tile) {
    case 3u: return "WINDGUST";
    case 6u: return "LONGNAIL";
    case 7u: return "STOUTSCALE";
    case 8u: return "SOULSEEK";
    case 9u: return "GOLDNAIL";
    case 10u: return "BLOODBLOOM";
    case 11u: return "GHOSTSHELL";
    case 19u: return "SOULWINGS";
    case 35u: return "HEART OF ROCK";
    case NAIL_ITEM_TILE: return "DISCARDED NAIL";
    case REMNANT_TILE: return "NAIL STRENGTH";
    default: return "ITEM";
    }
}

static const char* item_showcase_desc1(uint8_t tile) {
    switch (tile) {
    case 3u: return "AIR LEFT OR RIGHT";
    case 6u: return "NAIL REACHES";
    case 7u: return "FOCUS HEALS";
    case 8u: return "GAIN MORE SOUL";
    case 9u: return "NAIL HITS";
    case 10u: return "MAX LIFE";
    case 11u: return "LONGER IMMUNE";
    case 19u: return "JUMP AGAIN";
    case 35u: return "CLING TO WALLS";
    case NAIL_ITEM_TILE: return "STRIKE UNLOCKED";
    case REMNANT_TILE: return "NAIL DAMAGE";
    default: return "POWER ACQUIRED";
    }
}

static const char* item_showcase_desc2(uint8_t tile) {
    switch (tile) {
    case 3u: return "PLUS STRIKE TO DASH";
    case 6u: return "A LITTLE FARTHER";
    case 7u: return "MUCH FASTER";
    case 8u: return "FROM HITS";
    case 9u: return "HARDER";
    case 10u: return "INCREASED";
    case 11u: return "AFTER DAMAGE";
    case 19u: return "WHILE AIRBORNE";
    case 35u: return "JUMP TO LEAP";
    case NAIL_ITEM_TILE: return "BTN BOARD TO ATTACK";
    case REMNANT_TILE: return "INCREASED";
    default: return "";
    }
}

static void compose_item_showcase_line(uint16_t* line, int16_t region_x, int16_t region_width,
    int16_t screen_y) {
    if (!g_item_showcase_active) { return; }

    const int16_t panel_x1 = 18;
    const int16_t panel_y1 = 74;
    const int16_t panel_x2 = 222;
    const int16_t panel_y2 = 246;
    if (screen_y < panel_y1 || screen_y >= panel_y2) { return; }

    const uint16_t border_color = g_pico_palette[7];
    const uint16_t fill_color = g_pico_palette[1];
    for (int16_t screen_x = panel_x1; screen_x < panel_x2; screen_x++) {
        if (screen_x < region_x || screen_x >= region_x + region_width) { continue; }
        const uint8_t border = screen_y < panel_y1 + 2 || screen_y >= panel_y2 - 2 ||
                               screen_x < panel_x1 + 2 || screen_x >= panel_x2 - 2;
        line[screen_x - region_x] = border ? border_color : fill_color;
    }

    const int16_t icon_x = 120;
    const int16_t icon_y = 114;
    const uint8_t tile = g_item_showcase_tile;
    if (tile == NAIL_ITEM_TILE) {
        compose_sprite_rect_line(line, region_x, region_width, screen_y, 95u, 1, 2,
            (int16_t)(icon_x - 16), (int16_t)(icon_y - 28), 4u, 0);
    } else if (tile == REMNANT_TILE) {
        compose_sprite_rect_line(line, region_x, region_width, screen_y, REMNANT_TILE, 1, 1,
            (int16_t)(icon_x - 16), (int16_t)(icon_y - 16), 4u, 0);
    } else {
        compose_sprite_rect_line(line, region_x, region_width, screen_y, tile, 1, 1,
            (int16_t)(icon_x - 16), (int16_t)(icon_y - 16), 4u, 0);
    }

    compose_centered_text_line(line, region_x, region_width, screen_y, 120, 150,
        item_showcase_name(tile), 2u, g_pico_palette[12]);
    compose_centered_text_line(line, region_x, region_width, screen_y, 120, 178,
        item_showcase_desc1(tile), 1u, g_pico_palette[7]);
    compose_centered_text_line(line, region_x, region_width, screen_y, 120, 194,
        item_showcase_desc2(tile), 1u, g_pico_palette[13]);
    compose_centered_text_line(line, region_x, region_width, screen_y, 120, 224,
        "JUMP OR STRIKE", 1u, g_pico_palette[6]);
}

static Low_Knight_Rect nav_hud_rect(void);

static uint8_t point_in_rect(int16_t x, int16_t y, Low_Knight_Rect rect) {
    return x >= rect.x1 && x < rect.x2 && y >= rect.y1 && y < rect.y2;
}

static void compose_nav_hud_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    const Low_Knight_Rect nav = nav_hud_rect();
    if (screen_y < nav.y1 || screen_y >= nav.y2) { return; }

    const Low_Knight_Rect room_rect = {
        (int16_t)(nav.x1 + 4),
        (int16_t)(nav.y1 + 3),
        (int16_t)(nav.x2 - 4),
        (int16_t)(nav.y2 - 3),
    };
    const int16_t room_w_px = (int16_t)(g_room.width * PICO_TILE_SIZE);
    const int16_t room_h_px = (int16_t)(g_room.height * PICO_TILE_SIZE);
    if (room_w_px <= 0 || room_h_px <= 0) { return; }

    const int16_t inner_w = (int16_t)(room_rect.x2 - room_rect.x1 - 1);
    const int16_t inner_h = (int16_t)(room_rect.y2 - room_rect.y1 - 1);
    const int16_t player_x =
        (int16_t)(room_rect.x1 + 1 + (int32_t)g_player.x * inner_w / room_w_px);
    const int16_t player_y =
        (int16_t)(room_rect.y1 + 1 + (int32_t)g_player.y * inner_h / room_h_px);
    const Low_Knight_Rect player_dot = {
        (int16_t)(player_x - 1),
        (int16_t)(player_y - 1),
        (int16_t)(player_x + 2),
        (int16_t)(player_y + 2),
    };

    const int16_t cam_x =
        (int16_t)(room_rect.x1 + 1 + (int32_t)g_camera_x * inner_w / room_w_px);
    const int16_t cam_y =
        (int16_t)(room_rect.y1 + 1 + (int32_t)g_camera_y * inner_h / room_h_px);
    int16_t cam_w = (int16_t)((int32_t)PICO_SCREEN_SIZE * inner_w / room_w_px);
    int16_t cam_h = (int16_t)((int32_t)PICO_SCREEN_SIZE * inner_h / room_h_px);
    if (cam_w < 2) { cam_w = 2; }
    if (cam_h < 2) { cam_h = 2; }
    const Low_Knight_Rect cam_rect = {cam_x, cam_y, (int16_t)(cam_x + cam_w), (int16_t)(cam_y + cam_h)};

    const uint8_t exit_left = (g_room_exit_flags & ROOM_EXIT_LEFT) != 0u;
    const uint8_t exit_right = (g_room_exit_flags & ROOM_EXIT_RIGHT) != 0u;
    const uint8_t exit_up = (g_room_exit_flags & ROOM_EXIT_UP) != 0u;
    const uint8_t exit_down = (g_room_exit_flags & ROOM_EXIT_DOWN) != 0u;

    int16_t start = nav.x1 > region_x ? nav.x1 : region_x;
    int16_t end = nav.x2 < region_x + region_width ? nav.x2 : (int16_t)(region_x + region_width);
    for (int16_t screen_x = start; screen_x < end; screen_x++) {
        uint16_t color = g_pico_palette[1];
        const uint8_t nav_border = screen_y == nav.y1 || screen_y == nav.y2 - 1 ||
                                   screen_x == nav.x1 || screen_x == nav.x2 - 1;
        const uint8_t room_border = point_in_rect(screen_x, screen_y, room_rect) &&
                                    (screen_y == room_rect.y1 || screen_y == room_rect.y2 - 1 ||
                                     screen_x == room_rect.x1 || screen_x == room_rect.x2 - 1);
        const uint8_t cam_border = point_in_rect(screen_x, screen_y, cam_rect) &&
                                   (screen_y == cam_rect.y1 || screen_y == cam_rect.y2 - 1 ||
                                    screen_x == cam_rect.x1 || screen_x == cam_rect.x2 - 1);
        if (nav_border || room_border) { color = g_pico_palette[6]; }
        if (cam_border) { color = g_pico_palette[13]; }
        if (point_in_rect(screen_x, screen_y, player_dot)) { color = g_pico_palette[12]; }

        const int16_t mid_x = (int16_t)((nav.x1 + nav.x2) / 2);
        const int16_t mid_y = (int16_t)((nav.y1 + nav.y2) / 2);
        if (exit_left && screen_x >= nav.x1 + 1 && screen_x <= nav.x1 + 3 &&
            screen_y >= mid_y - 2 && screen_y <= mid_y + 2) {
            color = g_pico_palette[7];
        }
        if (exit_right && screen_x >= nav.x2 - 4 && screen_x <= nav.x2 - 2 &&
            screen_y >= mid_y - 2 && screen_y <= mid_y + 2) {
            color = g_pico_palette[7];
        }
        if (exit_up && screen_y >= nav.y1 + 1 && screen_y <= nav.y1 + 3 &&
            screen_x >= mid_x - 3 && screen_x <= mid_x + 3) {
            color = g_pico_palette[7];
        }
        if (exit_down && screen_y >= nav.y2 - 4 && screen_y <= nav.y2 - 2 &&
            screen_x >= mid_x - 3 && screen_x <= mid_x + 3) {
            color = g_pico_palette[7];
        }
        line[screen_x - region_x] = color;
    }
}

static void compose_hud_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    const Low_Knight_Rect view = screen_rect();
    if (screen_y < view.y1 + 1 || screen_y >= view.y1 + 30) { return; }

    const uint8_t scale = 2;
    const uint8_t max_hp = player_max_hp();
    for (uint8_t i = 0; i < max_hp; i++) {
        compose_sprite_rect_line(line, region_x, region_width, screen_y,
            i < g_player_hp ? 92u : 91u, 1, 1,
            (int16_t)(view.x1 + 12 + i * 12 * scale), (int16_t)(view.y1 + 3), scale, 0);
    }

    const int16_t soul_x = (int16_t)(view.x1 + 12);
    const int16_t soul_y = (int16_t)(view.y1 + 22);
    if (screen_y >= soul_y && screen_y < soul_y + 6) {
        const int16_t fill_width = (int16_t)(g_player_soul * 6);
        const uint8_t heal_frames = player_heal_frames();
        const int16_t focus_width =
            g_player_focus_timer > 0 ? (int16_t)(g_player_focus_timer * 72 / heal_frames) : 0;
        for (int16_t screen_x = soul_x; screen_x < soul_x + 74; screen_x++) {
            if (screen_x < region_x || screen_x >= region_x + region_width) { continue; }
            const uint8_t border =
                screen_y == soul_y || screen_y == soul_y + 5 ||
                screen_x == soul_x || screen_x == soul_x + 73;
            uint16_t color = border ? g_pico_palette[7] : g_pico_palette[1];
            if (!border && screen_x < soul_x + 1 + fill_width) { color = g_pico_palette[12]; }
            if (!border && focus_width > 0 && screen_x < soul_x + 1 + focus_width) {
                color = g_pico_palette[7];
            }
            line[screen_x - region_x] = color;
        }
    }

    compose_nav_hud_line(line, region_x, region_width, screen_y);
}

static void compose_line(int16_t x, int16_t y, int16_t width) {
    uint16_t* line = Game_Graphics_Get_Line_Buffer();
    for (int16_t col = 0; col < width; col++) { line[col] = g_pico_palette[1]; }
    compose_background_line(line, x, width, y);
    compose_level_line(line, x, width, y);
    compose_enemies_line(line, x, width, y);
    compose_items_line(line, x, width, y);
    compose_player_line(line, x, width, y);
    compose_strike_line(line, x, width, y);
    compose_blood_line(line, x, width, y);
    compose_hud_line(line, x, width, y);
    compose_item_showcase_line(line, x, width, y);
}

static void render_region(St7789* lcd, Low_Knight_Rect rect) {
    const Low_Knight_Rect view = screen_rect();
    rect = rect_intersect(rect, view);
    rect = rect_intersect(rect, (Low_Knight_Rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT});
    if (rect_is_empty(rect)) { return; }

    St7789_Begin_Write(lcd, rect.x1, rect.y1, (int16_t)(rect.x2 - 1), (int16_t)(rect.y2 - 1));
    for (int16_t y = rect.y1; y < rect.y2; y++) {
        compose_line(rect.x1, y, (int16_t)(rect.x2 - rect.x1));
        St7789_Write_Pixels(
            lcd, (uint8_t*)Game_Graphics_Get_Line_Buffer(), (uint32_t)(rect.x2 - rect.x1) * sizeof(uint16_t));
    }
    St7789_End_Write(lcd);
}

static Low_Knight_Rect player_rect_for(Low_Knight_Vec2i player, uint8_t scale) {
    const Low_Knight_Rect view = screen_rect();
    const int16_t x = (int16_t)(view.x1 + (player.x - g_camera_x) * scale);
    const int16_t y = (int16_t)(view.y1 + (player.y - g_camera_y) * scale);
    return (Low_Knight_Rect){
        (int16_t)(x - 8 * scale - 2),
        (int16_t)(y - 12 * scale - 2),
        (int16_t)(x + 8 * scale + 2),
        (int16_t)(y + 4 * scale + 2),
    };
}

static Low_Knight_Rect strike_rect_at(
    int16_t world_x, int16_t world_y, uint8_t facing_left, uint8_t direction) {
    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    const int16_t extra = strike_reach_extra();
    if (direction == STRIKE_UP) {
        const int16_t x = (int16_t)(view.x1 + (world_x - g_camera_x) * scale);
        const int16_t y = (int16_t)(view.y1 + (world_y - g_camera_y) * scale);
        return (Low_Knight_Rect){
            (int16_t)(x - 5 * scale - 2),
            (int16_t)(y - (27 + extra) * scale - 2),
            (int16_t)(x + 5 * scale + 2),
            (int16_t)(y + 1 * scale + 2),
        };
    }
    if (direction == STRIKE_DOWN) {
        const int16_t x = (int16_t)(view.x1 + (world_x - g_camera_x) * scale);
        const int16_t y = (int16_t)(view.y1 + (world_y - g_camera_y) * scale);
        return (Low_Knight_Rect){
            (int16_t)(x - 5 * scale - 2),
            (int16_t)(y + 1 * scale - 2),
            (int16_t)(x + 5 * scale + 2),
            (int16_t)(y + (27 + extra) * scale + 2),
        };
    }

    const int16_t center_x =
        (int16_t)(view.x1 + (world_x - g_camera_x +
                             (facing_left ? -(12 + extra) : (12 + extra))) *
                              scale);
    const int16_t center_y = (int16_t)(view.y1 + (world_y - g_camera_y - 4) * scale);
    return (Low_Knight_Rect){
        (int16_t)(center_x - 8 * scale - 2),
        (int16_t)(center_y - 4 * scale - 2),
        (int16_t)(center_x + 8 * scale + 2),
        (int16_t)(center_y + 4 * scale + 2),
    };
}

static Low_Knight_Rect blood_particle_rect_for(const Low_Knight_Blood_Particle* particle) {
    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    const int16_t radius = (int16_t)(particle->radius * scale + 2);
    const int16_t x = (int16_t)(view.x1 + (particle->x - g_camera_x) * scale);
    const int16_t y = (int16_t)(view.y1 + (particle->y - g_camera_y) * scale);
    return (Low_Knight_Rect){
        (int16_t)(x - radius),
        (int16_t)(y - radius),
        (int16_t)(x + radius + 1),
        (int16_t)(y + radius + 1),
    };
}

static Low_Knight_Rect player_hud_rect(void) {
    const Low_Knight_Rect view = screen_rect();
    return (Low_Knight_Rect){
        (int16_t)(view.x1 + 10),
        (int16_t)(view.y1 + 1),
        (int16_t)(view.x1 + 12 + PLAYER_MAX_HP_LIMIT * 24),
        (int16_t)(view.y1 + 30),
    };
}

static Low_Knight_Rect nav_hud_rect(void) {
    const Low_Knight_Rect view = screen_rect();
    const int16_t x = (int16_t)(view.x2 - NAV_MAP_WIDTH - 10);
    const int16_t y = (int16_t)(view.y1 + 4);
    return (Low_Knight_Rect){x, y, (int16_t)(x + NAV_MAP_WIDTH), (int16_t)(y + NAV_MAP_HEIGHT)};
}

static Low_Knight_Rect item_showcase_rect(void) {
    return (Low_Knight_Rect){16, 72, 224, 248};
}

static void resolve_player_horizontal(void) {
    g_player_qx += g_player_vx;
    sync_player_from_q();
    Low_Knight_Box box = player_hitbox_at(g_player.x, g_player.y);
    if (!box_overlaps_solid(box)) { return; }

    const int16_t push = g_player_vx > 0 ? -1 : 1;
    for (uint8_t i = 0; i < PICO_TILE_SIZE * 2u && box_overlaps_solid(box); i++) {
        g_player.x = (int16_t)(g_player.x + push);
        box = player_hitbox_at(g_player.x, g_player.y);
    }
    g_player_qx = (int32_t)g_player.x * PHYSICS_Q_ONE;
    g_player_vx = 0;
}

static void resolve_player_vertical(void) {
    g_player_qy += g_player_vy;
    sync_player_from_q();
    Low_Knight_Box box = player_hitbox_at(g_player.x, g_player.y);
    g_player_on_ground = 0;
    if (!box_overlaps_solid(box)) { return; }

    const int16_t push = g_player_vy > 0 ? -1 : 1;
    for (uint8_t i = 0; i < PICO_TILE_SIZE * 2u && box_overlaps_solid(box); i++) {
        g_player.y = (int16_t)(g_player.y + push);
        box = player_hitbox_at(g_player.x, g_player.y);
    }
    g_player_qy = (int32_t)g_player.y * PHYSICS_Q_ONE;
    if (g_player_vy > 0) { g_player_on_ground = 1; }
    g_player_vy = 0;
}

static Low_Knight_Box bush_hitbox_at(int16_t x, int16_t y) {
    return (Low_Knight_Box){
        (int16_t)(x - 4),
        (int16_t)(y - 4),
        (int16_t)(x + 4),
        (int16_t)(y + 2),
    };
}

static int16_t abs_i16(int16_t value) { return value < 0 ? (int16_t)-value : value; }

static int16_t approach_i16(int16_t value, int16_t target, int16_t amount) {
    if (value < target) {
        value = (int16_t)(value + amount);
        return value > target ? target : value;
    }
    if (value > target) {
        value = (int16_t)(value - amount);
        return value < target ? target : value;
    }
    return value;
}

static int16_t direction_component(int16_t delta, int16_t dx, int16_t dy, int16_t speed_q) {
    const int16_t divisor = abs_i16(dx) > abs_i16(dy) ? abs_i16(dx) : abs_i16(dy);
    if (divisor == 0) { return 0; }
    return (int16_t)((int32_t)delta * speed_q / divisor);
}

static Low_Knight_Box enemy_hitbox_at(const Low_Knight_Enemy* enemy, int16_t x, int16_t y) {
    if (enemy->type == ENEMY_LIMPER_TILE) {
        return (Low_Knight_Box){(int16_t)(x - 4), (int16_t)(y - 8), (int16_t)(x + 4), (int16_t)(y - 1)};
    }
    if (enemy->type == ENEMY_FATGUY_TILE || enemy->type == ENEMY_BALLGUY_TILE ||
        enemy->type == ENEMY_MOSS_TILE) {
        return (Low_Knight_Box){(int16_t)(x - 6), (int16_t)(y - 16), (int16_t)(x + 6), y};
    }
    return bush_hitbox_at(x, y);
}

static uint8_t enemy_player_near(const Low_Knight_Enemy* enemy, int16_t range_x, int16_t range_y) {
    return abs_i16((int16_t)(g_player.x - enemy->x)) < range_x &&
           abs_i16((int16_t)(g_player.y - enemy->y)) < range_y;
}

static void move_ground_enemy(Low_Knight_Enemy* enemy, int16_t speed_q, uint8_t avoid_cliffs) {
    const int16_t ahead_x = (int16_t)(enemy->x + enemy->way * 10);
    const int16_t ahead_y = (int16_t)(enemy->y + (enemy->type == ENEMY_BUSH_TILE ? 4 : 2));
    if (avoid_cliffs && enemy->vy == 0 &&
        !tile_is_solid_at(floor_div_tile(ahead_x), floor_div_tile(ahead_y))) {
        enemy->way = -enemy->way;
        if (enemy->type == ENEMY_BUSH_TILE) { enemy->anim_timer = BUSH_TURN_LOCK_FRAMES; }
    }

    enemy->vx = (int16_t)(enemy->way * speed_q);
    enemy->qx += enemy->vx;
    enemy->x = q_to_pixel(enemy->qx);
    Low_Knight_Box box = enemy_hitbox_at(enemy, enemy->x, enemy->y);
    if (box_overlaps_solid(box)) {
        const int16_t push = enemy->vx > 0 ? -1 : 1;
        for (uint8_t i = 0; i < PICO_TILE_SIZE * 2u && box_overlaps_solid(box); i++) {
            enemy->x = (int16_t)(enemy->x + push);
            box = enemy_hitbox_at(enemy, enemy->x, enemy->y);
        }
        enemy->qx = (int32_t)enemy->x * PHYSICS_Q_ONE;
        enemy->way = -enemy->way;
        enemy->vx = 0;
        if (enemy->type == ENEMY_BUSH_TILE) { enemy->anim_timer = BUSH_TURN_LOCK_FRAMES; }
    }

    enemy->vy = (int16_t)(enemy->vy +
                          (enemy->type == ENEMY_BUSH_TILE ? BUSH_GRAVITY_Q : ENEMY_GRAVITY_Q));
    const int16_t max_fall =
        enemy->type == ENEMY_BUSH_TILE ? BUSH_MAX_FALL_Q : ENEMY_MAX_FALL_Q;
    if (enemy->vy > max_fall) { enemy->vy = max_fall; }
    enemy->qy += enemy->vy;
    enemy->y = q_to_pixel(enemy->qy);
    box = enemy_hitbox_at(enemy, enemy->x, enemy->y);
    if (box_overlaps_solid(box)) {
        const int16_t push = enemy->vy > 0 ? -1 : 1;
        for (uint8_t i = 0; i < PICO_TILE_SIZE * 2u && box_overlaps_solid(box); i++) {
            enemy->y = (int16_t)(enemy->y + push);
            box = enemy_hitbox_at(enemy, enemy->x, enemy->y);
        }
        enemy->qy = (int32_t)enemy->y * PHYSICS_Q_ONE;
        enemy->vy = 0;
    }
}

static void move_flying_enemy(Low_Knight_Enemy* enemy) {
    const int32_t old_qx = enemy->qx;
    const int32_t old_qy = enemy->qy;
    enemy->qx += enemy->vx;
    enemy->x = q_to_pixel(enemy->qx);
    Low_Knight_Box box =
        (Low_Knight_Box){(int16_t)(enemy->x - 4), (int16_t)(enemy->y - 5),
            (int16_t)(enemy->x + 4), (int16_t)(enemy->y + 4)};
    if (box_overlaps_solid(box)) {
        enemy->qx = old_qx;
        enemy->x = q_to_pixel(enemy->qx);
        enemy->vx = (int16_t)-enemy->vx;
        enemy->way = -enemy->way;
    }

    enemy->qy += enemy->vy;
    enemy->y = q_to_pixel(enemy->qy);
    box = (Low_Knight_Box){(int16_t)(enemy->x - 4), (int16_t)(enemy->y - 5),
        (int16_t)(enemy->x + 4), (int16_t)(enemy->y + 4)};
    if (box_overlaps_solid(box)) {
        enemy->qy = old_qy;
        enemy->y = q_to_pixel(enemy->qy);
        enemy->vy = (int16_t)-enemy->vy;
    }
}

static void spawn_projectile(
    int16_t x, int16_t y, int16_t vx, int16_t vy, uint8_t tile) {
    for (uint8_t i = 0; i < LOW_KNIGHT_MAX_PROJECTILES; i++) {
        Low_Knight_Projectile* projectile = &g_projectiles[i];
        if (projectile->active) { continue; }
        projectile->x = x;
        projectile->y = y;
        projectile->qx = (int32_t)x * PHYSICS_Q_ONE;
        projectile->qy = (int32_t)y * PHYSICS_Q_ONE;
        projectile->vx = vx;
        projectile->vy = vy;
        projectile->tile = tile;
        projectile->life = 180u;
        projectile->active = 1;
        return;
    }
}

static void update_fly(Low_Knight_Enemy* enemy) {
    if (enemy->timer > 0) { enemy->timer--; }
    if (enemy->timer == 0) {
        const int16_t dx = (int16_t)(g_player.x - enemy->x);
        const int16_t dy = (int16_t)(g_player.y - enemy->y);
        if (enemy_player_near(enemy, 112, 80) && ((enemy->phase + g_runtime_frame) & 1u)) {
            enemy->vx = direction_component(dx, dx, dy, 410);
            enemy->vy = direction_component(dy, dx, dy, 410);
        } else {
            enemy->way = ((enemy->phase + g_runtime_frame / 60u) & 1u) ? 1 : -1;
            enemy->vx = (int16_t)(enemy->way * 154);
            const uint8_t wave = (uint8_t)(g_runtime_frame + enemy->phase);
            enemy->vy = (wave & 32u) ? -96 : 96;
        }
        enemy->timer = 60u;
    }
    if (enemy->vx != 0) { enemy->way = enemy->vx < 0 ? -1 : 1; }
    move_flying_enemy(enemy);
}

static void update_bee(Low_Knight_Enemy* enemy) {
    if (enemy->anim_timer > 0) {
        enemy->anim_timer--;
        enemy->state = 1u;
    } else {
        enemy->state = 0u;
    }
    if (enemy->timer > 0) { enemy->timer--; }
    if (enemy->timer == 0) {
        enemy->way = -enemy->way;
        if (enemy_player_near(enemy, 128, 96)) {
            const int16_t dx = (int16_t)(g_player.x - enemy->x);
            const int16_t dy = (int16_t)(g_player.y - enemy->y);
            spawn_projectile(enemy->x, enemy->y,
                direction_component(dx, dx, dy, 256),
                direction_component(dy, dx, dy, 256), 194u);
            enemy->state = 1u;
            enemy->anim_timer = 15u;
            enemy->vy = (int16_t)(enemy->vy - 384);
        }
        enemy->timer = (uint16_t)(75u + (enemy->phase & 31u));
    }

    const int16_t target_y = (int16_t)(g_player.y - 42);
    enemy->vx = approach_i16(enemy->vx, (int16_t)(enemy->way * 256), 24);
    enemy->vy = approach_i16(enemy->vy,
        clamp_i16((int16_t)((target_y - enemy->y) * 12), -256, 256), 20);
    move_flying_enemy(enemy);
}

static void update_bush(Low_Knight_Enemy* enemy) {
    int16_t speed = BUSH_MOVE_SPEED_Q;
    if (enemy->anim_timer > 0) { enemy->anim_timer--; }
    if (enemy_player_near(enemy, 96, 48)) {
        const int16_t dx = (int16_t)(g_player.x - enemy->x);
        const int16_t distance_x = abs_i16(dx);
        const int16_t pulse = (int16_t)((g_runtime_frame + enemy->phase) & 31u);
        speed = (int16_t)(205 + (pulse < 16 ? pulse * 5 : (31 - pulse) * 5));
        if (distance_x <= BUSH_TRACK_STOP_DISTANCE) {
            speed = 0;
        } else if (enemy->anim_timer == 0) {
            enemy->way = dx < 0 ? -1 : 1;
        }
    }
    move_ground_enemy(enemy, speed, speed != 0);
}

static void update_limper(Low_Knight_Enemy* enemy) {
    if (enemy->timer > 0) { enemy->timer--; }
    if (enemy->timer == 0) {
        enemy->state ^= 1u;
        enemy->timer = enemy->state ? 18u : 12u;
    }
    move_ground_enemy(enemy, enemy->state ? 128 : 32, 1);
}

static void update_fatguy(Low_Knight_Enemy* enemy) {
    int16_t speed = 96;
    if (enemy->state == 0u) {
        if (enemy->timer > 0) { enemy->timer--; }
        if (enemy->timer == 0 && enemy_player_near(enemy, 104, 72)) {
            enemy->way = g_player.x < enemy->x ? -1 : 1;
            enemy->phase++;
            enemy->state = (enemy->phase & 1u) ? 1u : 2u;
            enemy->timer = enemy->state == 1u ? 30u : 80u;
        }
    } else if (enemy->state == 1u) {
        speed = (int16_t)(128 + enemy->timer * 13);
        if (enemy->timer > 0) { enemy->timer--; }
        if (enemy->timer == 0) {
            enemy->state = 0u;
            enemy->timer = 80u;
        }
    } else {
        speed = 0;
        if (enemy->timer == 40u) {
            for (int8_t spread = -1; spread <= 1; spread++) {
                spawn_projectile(enemy->x, (int16_t)(enemy->y - 12),
                    (int16_t)(enemy->way * (220 + (spread == 0 ? 24 : 0))),
                    (int16_t)(-250 + spread * 105), spread == 0 ? 194u : 195u);
            }
        }
        if (enemy->timer > 0) { enemy->timer--; }
        if (enemy->timer == 0) {
            enemy->state = 0u;
            enemy->timer = 100u;
        }
    }
    move_ground_enemy(enemy, speed, 1);
}

static void update_tether_weapon(Low_Knight_Enemy* enemy) {
    const int16_t weapon_x = q_to_pixel(enemy->weapon_qx);
    const int16_t weapon_y = q_to_pixel(enemy->weapon_qy);
    const int16_t target_x = (int16_t)(enemy->x - enemy->way *
        (enemy->type == ENEMY_MOSS_TILE ? 1 : 4));
    const int16_t target_y = (int16_t)(enemy->y - 4);
    const int16_t spring = enemy->type == ENEMY_MOSS_TILE ? 31 : 10;
    const int16_t damping = enemy->type == ENEMY_MOSS_TILE ? 179 : 233;
    enemy->weapon_vx =
        (int16_t)(enemy->weapon_vx + (target_x - weapon_x) * spring);
    enemy->weapon_vy =
        (int16_t)(enemy->weapon_vy + (target_y - weapon_y) * spring);
    enemy->weapon_vx = (int16_t)((int32_t)enemy->weapon_vx * damping / 256);
    enemy->weapon_vy = (int16_t)((int32_t)enemy->weapon_vy * damping / 256);
    enemy->weapon_qx += enemy->weapon_vx;
    enemy->weapon_qy += enemy->weapon_vy;
}

static void update_ballguy(Low_Knight_Enemy* enemy) {
    int16_t speed = 128;
    if (enemy->state == 0u) {
        if (enemy->timer > 0) { enemy->timer--; }
        if (enemy->timer == 0 && enemy_player_near(enemy, 112, 72)) {
            enemy->way = g_player.x < enemy->x ? -1 : 1;
            enemy->state = 1u;
            enemy->timer = 60u;
        }
    } else {
        speed = 0;
        if (enemy->timer == 50u) {
            enemy->weapon_vx = (int16_t)(-enemy->way * 220);
            enemy->weapon_vy = -80;
        } else if (enemy->timer == 38u) {
            const int16_t dx = (int16_t)(g_player.x - q_to_pixel(enemy->weapon_qx));
            const int16_t dy = (int16_t)(g_player.y - 8 - q_to_pixel(enemy->weapon_qy));
            enemy->weapon_vx = direction_component(dx, dx, dy, 768);
            enemy->weapon_vy = direction_component(dy, dx, dy, 768);
        }
        if (enemy->timer > 0) { enemy->timer--; }
        if (enemy->timer == 0) {
            enemy->state = 0u;
            enemy->timer = 90u;
        }
    }
    move_ground_enemy(enemy, speed, 1);
    update_tether_weapon(enemy);
}

static void update_moss(Low_Knight_Enemy* enemy) {
    int16_t speed = 96;
    if (enemy->state == 0u) {
        if (enemy->timer > 0) { enemy->timer--; }
        if (enemy->timer == 0 && enemy_player_near(enemy, 96, 56)) {
            enemy->way = g_player.x < enemy->x ? -1 : 1;
            enemy->state = 1u;
            enemy->timer = 60u;
        }
    } else {
        speed = enemy->timer <= 30u && enemy->timer > 18u ? 420 : 0;
        if (enemy->timer == 25u) {
            enemy->weapon_vx = (int16_t)(enemy->way * 768);
            enemy->weapon_vy = 0;
        }
        if (enemy->timer > 0) { enemy->timer--; }
        if (enemy->timer == 0) {
            enemy->state = 0u;
            enemy->timer = 80u;
        }
    }
    move_ground_enemy(enemy, speed, 1);
    update_tether_weapon(enemy);
}

static void update_ambush(Low_Knight_Enemy* enemy) {
    if (g_player.y <= enemy->y ||
        abs_i16((int16_t)(g_player.x - enemy->x - 4)) >= 12) {
        return;
    }
    enemy->type = ENEMY_BUSH_TILE;
    enemy->y = (int16_t)(enemy->y + 4);
    enemy->qy = (int32_t)enemy->y * PHYSICS_Q_ONE;
    enemy->way = g_player.x < enemy->x ? 1 : -1;
    enemy->timer = 0;
}

static void update_projectiles(void) {
    const int16_t room_width = (int16_t)(g_room.width * PICO_TILE_SIZE);
    const int16_t room_height = (int16_t)(g_room.height * PICO_TILE_SIZE);
    for (uint8_t i = 0; i < LOW_KNIGHT_MAX_PROJECTILES; i++) {
        Low_Knight_Projectile* projectile = &g_projectiles[i];
        if (!projectile->active) { continue; }

        projectile->vx = (int16_t)((int32_t)projectile->vx * 253 / 256);
        projectile->vy = (int16_t)(projectile->vy + PROJECTILE_GRAVITY_Q);
        projectile->qx += projectile->vx;
        projectile->qy += projectile->vy;
        projectile->x = q_to_pixel(projectile->qx);
        projectile->y = q_to_pixel(projectile->qy);
        if (projectile->life > 0) { projectile->life--; }

        const Low_Knight_Box box = {(int16_t)(projectile->x - 2), (int16_t)(projectile->y - 2),
            (int16_t)(projectile->x + 2), (int16_t)(projectile->y + 2)};
        if (projectile->life == 0 || projectile->x < -8 || projectile->y < -8 ||
            projectile->x > room_width + 8 || projectile->y > room_height + 8 ||
            box_overlaps_solid(box)) {
            projectile->active = 0;
            continue;
        }
    }
}

static void update_enemies(void) {
    for (uint8_t i = 0; i < g_enemy_count; i++) {
        Low_Knight_Enemy* enemy = &g_enemies[i];
        if (!enemy->active) { continue; }
        if (enemy->type == ENEMY_FLY_TILE) {
            update_fly(enemy);
        } else if (enemy->type == ENEMY_BEE_TILE) {
            update_bee(enemy);
        } else if (enemy->type == ENEMY_BUSH_TILE) {
            update_bush(enemy);
        } else if (enemy->type == ENEMY_LIMPER_TILE) {
            update_limper(enemy);
        } else if (enemy->type == ENEMY_FATGUY_TILE) {
            update_fatguy(enemy);
        } else if (enemy->type == ENEMY_BALLGUY_TILE) {
            update_ballguy(enemy);
        } else if (enemy->type == ENEMY_MOSS_TILE) {
            update_moss(enemy);
        } else if (enemy->type == ENEMY_AMBUSH_TILE) {
            update_ambush(enemy);
        }
    }
    update_projectiles();
}

static uint8_t boxes_overlap(Low_Knight_Box a, Low_Knight_Box b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

static uint16_t item_ability_for_tile(uint8_t tile) {
    switch (tile) {
    case 3u: return ABILITY_WINDGUST;
    case 6u: return ABILITY_LONGNAIL;
    case 7u: return ABILITY_FAST_HEAL;
    case 8u: return ABILITY_SOULSEEK;
    case 9u: return ABILITY_GOLDNAIL;
    case 10u: return ABILITY_BLOODBLOOM;
    case 11u: return ABILITY_GHOSTSHELL;
    case 19u: return ABILITY_SOULWINGS;
    case 35u: return ABILITY_HEART_OF_ROCK;
    case NAIL_ITEM_TILE: return ABILITY_DISCARDED_NAIL;
    default: return 0u;
    }
}

static Low_Knight_Box strike_hitbox(void) {
    const int16_t extra = strike_reach_extra();
    if (g_strike_direction == STRIKE_UP) {
        return (Low_Knight_Box){
            (int16_t)(g_strike_x - 7),
            (int16_t)(g_strike_y - 28 - extra),
            (int16_t)(g_strike_x + 7),
            (int16_t)(g_strike_y - 3),
        };
    }
    if (g_strike_direction == STRIKE_DOWN) {
        return (Low_Knight_Box){
            (int16_t)(g_strike_x - 7),
            (int16_t)(g_strike_y + 2),
            (int16_t)(g_strike_x + 7),
            (int16_t)(g_strike_y + 28 + extra),
        };
    }
    if (g_strike_facing_left) {
        return (Low_Knight_Box){
            (int16_t)(g_strike_x - 24 - extra),
            (int16_t)(g_strike_y - 11),
            (int16_t)(g_strike_x - 3),
            (int16_t)(g_strike_y + 2),
        };
    }
    return (Low_Knight_Box){
        (int16_t)(g_strike_x + 3),
        (int16_t)(g_strike_y - 11),
        (int16_t)(g_strike_x + 24 + extra),
        (int16_t)(g_strike_y + 2),
    };
}

static uint16_t effect_random(void) {
    g_effect_rng = (uint16_t)(g_effect_rng * 25173u + 13849u);
    return g_effect_rng;
}

static void spawn_blood_particle(
    int16_t x, int16_t y, int16_t vx, int16_t vy, uint8_t radius, uint8_t life, uint8_t color) {
    Low_Knight_Blood_Particle* target = NULL;
    for (uint8_t i = 0; i < LOW_KNIGHT_MAX_BLOOD_PARTICLES; i++) {
        Low_Knight_Blood_Particle* particle = &g_blood_particles[i];
        if (!particle->active) {
            target = particle;
            break;
        }
        if (target == NULL || particle->life < target->life) { target = particle; }
    }
    if (target == NULL) { return; }

    const uint8_t was_active = target->active;
    if (was_active) { mark_dirty_rect(blood_particle_rect_for(target)); }
    target->x = x;
    target->y = y;
    target->qx = (int32_t)x * PHYSICS_Q_ONE;
    target->qy = (int32_t)y * PHYSICS_Q_ONE;
    target->vx = vx;
    target->vy = vy;
    target->radius = radius;
    target->life = life;
    target->color = color;
    target->active = 1;
    if (!was_active && g_blood_active_count < LOW_KNIGHT_MAX_BLOOD_PARTICLES) { g_blood_active_count++; }
    mark_dirty_rect(blood_particle_rect_for(target));
}

static void spawn_enemy_blood(const Low_Knight_Enemy* enemy, uint8_t dying) {
    static const int16_t direction_x[8] = {256, 181, 0, -181, -256, -181, 0, 181};
    static const int16_t direction_y[8] = {0, -181, -256, -181, 0, 181, 256, 181};
    const Low_Knight_Box box = enemy_hitbox_at(enemy, enemy->x, enemy->y);
    const int16_t center_x = (int16_t)((box.left + box.right) / 2);
    const int16_t center_y = (int16_t)((box.top + box.bottom) / 2);
    const uint8_t count = dying ? 9u : 4u;

    for (uint8_t i = 0; i < count; i++) {
        const uint16_t random = effect_random();
        const uint8_t direction = (uint8_t)(random & 7u);
        const int16_t speed = (int16_t)((dying ? 170 : 110) + ((random >> 3) & 0x7fu));
        const int16_t vx = (int16_t)((int32_t)direction_x[direction] * speed / 256);
        const int16_t vy = (int16_t)((int32_t)direction_y[direction] * speed / 256 - 35);
        const uint8_t radius = (uint8_t)(1u + ((random >> 10) & (dying ? 0x03u : 0x01u)));
        const uint8_t life = (uint8_t)((dying ? 18u : 10u) + ((random >> 12) & 0x0fu));
        spawn_blood_particle(
            center_x, center_y, vx, vy, radius > 3u ? 3u : radius, life, BLOOD_COLOR);
    }
}

static void update_blood_particles(void) {
    if (g_blood_active_count == 0) { return; }
    if ((g_runtime_frame % BLOOD_REDRAW_DIVIDER) != 0u) { return; }

    for (uint8_t i = 0; i < LOW_KNIGHT_MAX_BLOOD_PARTICLES; i++) {
        Low_Knight_Blood_Particle* particle = &g_blood_particles[i];
        if (!particle->active) { continue; }
        const Low_Knight_Rect before = blood_particle_rect_for(particle);

        for (uint8_t step = 0; step < BLOOD_PHYSICS_SUBSTEPS && particle->life > 0; step++) {
            particle->vx = (int16_t)((int32_t)particle->vx * 244 / 256);
            particle->vy = (int16_t)((int32_t)particle->vy * 244 / 256 + 10);
            particle->qx += particle->vx;
            particle->qy += particle->vy;
            particle->life--;
        }
        particle->x = q_to_pixel(particle->qx);
        particle->y = q_to_pixel(particle->qy);
        if (particle->life == 0) {
            particle->active = 0;
            if (g_blood_active_count > 0) { g_blood_active_count--; }
            mark_dirty_rect(before);
            continue;
        }
        mark_dirty_rect(rect_union_visible(before, blood_particle_rect_for(particle)));
    }
}

static void spawn_player_blood(uint8_t dying) {
    static const int16_t direction_x[8] = {256, 181, 0, -181, -256, -181, 0, 181};
    static const int16_t direction_y[8] = {0, -181, -256, -181, 0, 181, 256, 181};
    const Low_Knight_Box box = player_hitbox_at(g_player.x, g_player.y);
    const int16_t center_x = (int16_t)((box.left + box.right) / 2);
    const int16_t center_y = (int16_t)((box.top + box.bottom) / 2);
    const uint8_t count = dying ? 10u : 6u;

    for (uint8_t i = 0; i < count; i++) {
        const uint16_t random = effect_random();
        const uint8_t direction = (uint8_t)(random & 7u);
        const int16_t speed = (int16_t)((dying ? 190 : 130) + ((random >> 3) & 0x7fu));
        const int16_t vx = (int16_t)((int32_t)direction_x[direction] * speed / 256);
        const int16_t vy = (int16_t)((int32_t)direction_y[direction] * speed / 256 - 45);
        const uint8_t radius = (uint8_t)(1u + ((random >> 10) & 0x03u));
        const uint8_t life = (uint8_t)((dying ? 20u : 12u) + ((random >> 12) & 0x0fu));
        spawn_blood_particle(center_x, center_y, vx, vy,
            radius > 3u ? 3u : radius, life, PLAYER_BLOOD_COLOR);
    }
}

static void spawn_heal_effect(void) {
    for (uint8_t i = 0; i < 10u; i++) {
        const uint16_t random = effect_random();
        const int16_t x = (int16_t)(g_player.x + (int16_t)((random & 0x0fu) - 8));
        const int16_t vx = (int16_t)(((int16_t)((random >> 4) & 0x1fu) - 16) * 5);
        const int16_t vy = (int16_t)(-70 - ((random >> 9) & 0x7fu));
        spawn_blood_particle(x, (int16_t)(g_player.y - 3), vx, vy,
            (uint8_t)(1u + ((random >> 14) & 1u)), (uint8_t)(12u + (random & 7u)), 7u);
    }
}

static void apply_item_effect(uint8_t tile) {
    if (tile == REMNANT_TILE) {
        if (g_nail_damage_bonus < 3u) { g_nail_damage_bonus++; }
    } else {
        g_player_abilities |= item_ability_for_tile(tile);
    }

    if (tile == 10u && g_player_hp < player_max_hp()) { g_player_hp = player_max_hp(); }
    g_item_showcase_tile = tile;
    g_item_showcase_active = 1;
    spawn_heal_effect();
    mark_dirty_rect(item_showcase_rect());
    mark_dirty_rect(player_hud_rect());
}

static uint8_t check_item_pickups(void) {
    const Low_Knight_Box player_box = player_hitbox_at(g_player.x, g_player.y);
    for (uint8_t i = 0; i < g_item_count; i++) {
        Low_Knight_Item* item = &g_items[i];
        if (!item->active) { continue; }

        const Low_Knight_Box item_box = {
            (int16_t)(item->x - 5),
            (int16_t)(item->y - 9),
            (int16_t)(item->x + 5),
            (int16_t)(item->y + 5),
        };
        if (!boxes_overlap(player_box, item_box)) { continue; }

        mark_dirty_rect(item_rect_for(item));
        item->active = 0;
        if (g_item_active_count > 0) { g_item_active_count--; }
        remember_collected_item(item->key);
        apply_item_effect(item->tile);
        return 1;
    }
    return 0;
}

static uint8_t update_focus(const Low_Knight_Input* input, uint8_t controls_locked) {
    if (input->move_y < 0) {
        g_player_focus_input_grace = PLAYER_FOCUS_INPUT_GRACE_FRAMES;
    } else if (g_player_focus_input_grace > 0) {
        g_player_focus_input_grace--;
    }

    const uint8_t can_focus = !controls_locked && g_player_focus_input_grace > 0 &&
                              (g_player_on_ground || player_is_supported()) &&
                              g_player_hp < player_max_hp() &&
                              g_player_soul >= PLAYER_HEAL_SOUL_COST;
    if (!can_focus) {
        if (g_player_focus_timer > 0) { mark_dirty_rect(player_hud_rect()); }
        g_player_focus_timer = 0;
        return 0;
    }

    g_player_focus_timer++;
    mark_dirty_rect(player_hud_rect());
    if ((g_player_focus_timer & 3u) == 0u) {
        const uint16_t random = effect_random();
        spawn_blood_particle((int16_t)(g_player.x + (int16_t)((random & 7u) - 4)),
            g_player.y, 0, -96, 1u, 12u, 7u);
    }
    if (g_player_focus_timer >= player_heal_frames()) {
        g_player_focus_timer = 0;
        g_player_soul = (uint8_t)(g_player_soul - PLAYER_HEAL_SOUL_COST);
        g_player_hp++;
        spawn_heal_effect();
        mark_dirty_rect(player_hud_rect());
    }
    return 1;
}

static uint8_t interact_with_bench(const Low_Knight_Input* input) {
    if (!input->up_pressed) { return 0; }

    const Low_Knight_Box player_box = player_hitbox_at(g_player.x, g_player.y);
    for (uint8_t tile_y = 0; tile_y < g_room.height; tile_y++) {
        for (uint8_t tile_x = 0; tile_x < g_room.width; tile_x++) {
            if (g_room_tiles[(uint16_t)tile_y * g_room.width + tile_x] != BENCH_TILE) { continue; }
            const int16_t x = (int16_t)(tile_x * PICO_TILE_SIZE);
            const int16_t y = (int16_t)(tile_y * PICO_TILE_SIZE);
            const Low_Knight_Box bench_box = {
                (int16_t)(x - 4),
                y,
                (int16_t)(x + 12),
                (int16_t)(y + 8),
            };
            if (!boxes_overlap(player_box, bench_box)) { continue; }

            g_player_hp = player_max_hp();
            g_player_focus_timer = 0;
            g_player_vx = 0;
            g_player_vy = -96;
            g_respawn_room_index = g_room_index;
            g_respawn_player =
                (Low_Knight_Vec2i){g_player.x, (int16_t)(g_player.y - 3)};
            g_has_bench_respawn = 1;
            spawn_heal_effect();
            mark_dirty_rect(player_hud_rect());
            mark_dirty_rect(player_rect_for(g_player, 2));
            return 1;
        }
    }
    return 0;
}

static void hurt_player(int16_t source_x) {
    if (g_cheat_enabled || g_player_invulnerable > 0 || g_player_death_timer > 0 || g_player_hp == 0) { return; }

    mark_dirty_rect(player_rect_for(g_player, 2));
    mark_dirty_rect(player_hud_rect());
    if (g_strike_timer > 0) {
        mark_dirty_rect(
            strike_rect_at(g_strike_x, g_strike_y, g_strike_facing_left, g_strike_direction));
    }

    g_player_hp--;
    g_player_focus_timer = 0;
    g_player_focus_input_grace = 0;
    g_player_dash_timer = 0;
    g_player_wall_dir = 0;
    g_strike_timer = 0;
    g_strike_hit_mask = 0;
    if (g_player_hp == 0) {
        spawn_player_blood(1);
        g_player_death_timer = PLAYER_DEATH_FRAMES;
        g_player_vx = 0;
        g_player_vy = 0;
        g_player_hurt_lock = 0;
        return;
    }

    spawn_player_blood(0);
    g_player_invulnerable = player_has_ability(ABILITY_GHOSTSHELL)
                                 ? (uint8_t)(PLAYER_INVULNERABLE_FRAMES + 30u)
                                 : PLAYER_INVULNERABLE_FRAMES;
    g_player_hurt_lock = PLAYER_HURT_LOCK_FRAMES;
    g_player_vx = g_player.x >= source_x ? PLAYER_HURT_KNOCKBACK_Q : -PLAYER_HURT_KNOCKBACK_Q;
    g_player_vy = PLAYER_HURT_LIFT_Q;
    g_player_on_ground = 0;
}

static void check_player_damage(void) {
    if (g_cheat_enabled || g_player_invulnerable > 0 || g_player_death_timer > 0) { return; }

    const Low_Knight_Box player_box = player_hitbox_at(g_player.x, g_player.y);
    for (uint8_t i = 0; i < LOW_KNIGHT_MAX_PROJECTILES; i++) {
        Low_Knight_Projectile* projectile = &g_projectiles[i];
        if (!projectile->active) { continue; }
        const Low_Knight_Box projectile_box = {
            (int16_t)(projectile->x - 2),
            (int16_t)(projectile->y - 2),
            (int16_t)(projectile->x + 2),
            (int16_t)(projectile->y + 2),
        };
        if (!boxes_overlap(player_box, projectile_box)) { continue; }
        mark_dirty_rect(projectile_rect_for(projectile));
        projectile->active = 0;
        g_dynamic_redraw_due = 1;
        hurt_player(projectile->x);
        return;
    }

    for (uint8_t i = 0; i < g_enemy_count; i++) {
        const Low_Knight_Enemy* enemy = &g_enemies[i];
        if (!enemy->active || enemy->type == ENEMY_AMBUSH_TILE) { continue; }
        if (boxes_overlap(player_box, enemy_hitbox_at(enemy, enemy->x, enemy->y))) {
            hurt_player(enemy->x);
            return;
        }
        if (enemy->type == ENEMY_BALLGUY_TILE || enemy->type == ENEMY_MOSS_TILE) {
            const int16_t weapon_x = q_to_pixel(enemy->weapon_qx);
            const int16_t weapon_y = q_to_pixel(enemy->weapon_qy);
            const Low_Knight_Box weapon_box = {
                (int16_t)(weapon_x - 3),
                (int16_t)(weapon_y - 3),
                (int16_t)(weapon_x + 3),
                (int16_t)(weapon_y + 3),
            };
            if (boxes_overlap(player_box, weapon_box)) {
                hurt_player(weapon_x);
                return;
            }
        }
    }
}

static void strike_enemy(Low_Knight_Enemy* enemy) {
    const Low_Knight_Rect before = enemy_rect_for(enemy);
    const uint8_t damage = player_strike_damage();
    const uint8_t dying = enemy->hp <= damage;
    spawn_enemy_blood(enemy, dying);
    if (dying) {
        enemy->hp = 0;
        enemy->active = 0;
    } else {
        const int16_t knockback = g_strike_direction == STRIKE_HORIZONTAL
                                      ? (g_strike_facing_left ? -3 : 3)
                                      : 0;
        const int16_t old_x = enemy->x;
        const int32_t old_qx = enemy->qx;
        enemy->hp = (uint8_t)(enemy->hp - damage);
        enemy->x = (int16_t)(enemy->x + knockback);
        enemy->qx = (int32_t)enemy->x * PHYSICS_Q_ONE;
        if (box_overlaps_solid(enemy_hitbox_at(enemy, enemy->x, enemy->y))) {
            enemy->x = old_x;
            enemy->qx = old_qx;
        }
        if (g_strike_direction == STRIKE_HORIZONTAL) {
            enemy->way = knockback < 0 ? 1 : -1;
        }
    }
    const uint8_t soul_gain = player_has_ability(ABILITY_SOULSEEK) ? 2u : 1u;
    g_player_soul =
        (uint8_t)(g_player_soul + soul_gain > PLAYER_SOUL_MAX ? PLAYER_SOUL_MAX : g_player_soul + soul_gain);
    mark_dirty_rect(player_hud_rect());
    if (g_strike_direction == STRIKE_DOWN) {
        g_player_vy = PLAYER_DOWN_STRIKE_BOUNCE_Q;
        g_player_on_ground = 0;
    }
    mark_dirty_rect(rect_union_visible(before,
        enemy->active ? enemy_rect_for(enemy) : (Low_Knight_Rect){0, 0, 0, 0}));
    g_dynamic_redraw_due = 1;
}

static void update_strike(const Low_Knight_Input* input) {
    if (g_strike_cooldown > 0) { g_strike_cooldown--; }
    if (g_strike_timer > 0) { g_strike_timer--; }

    if (input->strike_pressed && g_strike_cooldown == 0 && g_player_hurt_lock == 0 &&
        g_player_focus_timer == 0) {
        if (!g_player_on_ground && player_has_ability(ABILITY_WINDGUST) &&
            g_player_air_dash_available && input->move_x != 0 && input->move_y == 0) {
            g_player_vx = (int16_t)(input->move_x * PLAYER_DASH_Q);
            g_player_vy = -64;
            g_player_facing_left = input->move_x < 0;
            g_player_dash_timer = PLAYER_DASH_FRAMES;
            g_player_air_dash_available = 0;
            g_strike_cooldown = STRIKE_COOLDOWN_FRAMES;
            g_strike_timer = 0;
            g_strike_hit_mask = 0;
            mark_dirty_rect(player_rect_for(g_player, 2));
            return;
        }

        g_strike_timer = STRIKE_DURATION_FRAMES;
        g_strike_cooldown = STRIKE_COOLDOWN_FRAMES;
        g_strike_hit_mask = 0;
        g_strike_x = g_player.x;
        g_strike_y = g_player.y;
        g_strike_facing_left = g_player_facing_left;
        g_strike_direction = input->move_y > 0
                                 ? STRIKE_UP
                                 : (input->move_y < 0 && !g_player_on_ground ? STRIKE_DOWN
                                                                            : STRIKE_HORIZONTAL);
        if (g_strike_direction == STRIKE_UP) {
            g_player_vy = (int16_t)(g_player_vy + (g_player_on_ground ? -512 : -32));
            g_player_on_ground = 0;
        } else if (g_strike_direction == STRIKE_DOWN) {
            g_player_vy = (int16_t)(g_player_vy + 256);
        }
    }
    if (g_strike_timer < STRIKE_HIT_START || g_strike_timer > STRIKE_HIT_END) { return; }

    const Low_Knight_Box hitbox = strike_hitbox();
    for (uint8_t i = 0; i < g_enemy_count; i++) {
        Low_Knight_Enemy* enemy = &g_enemies[i];
        const uint16_t enemy_bit = (uint16_t)(1u << i);
        if (!enemy->active || enemy->type == ENEMY_AMBUSH_TILE ||
            (g_strike_hit_mask & enemy_bit) != 0u) {
            continue;
        }
        if (!boxes_overlap(hitbox, enemy_hitbox_at(enemy, enemy->x, enemy->y))) { continue; }
        g_strike_hit_mask |= enemy_bit;
        strike_enemy(enemy);
    }
}

static uint8_t respawn_player(void) {
    if (!load_room(g_respawn_room_index)) { return 0; }

    const int16_t max_x = (int16_t)(g_room.width * PICO_TILE_SIZE - 1);
    const int16_t max_y = (int16_t)(g_room.height * PICO_TILE_SIZE - 1);
    g_player.x = clamp_i16(g_respawn_player.x, 0, max_x);
    g_player.y = clamp_i16(g_respawn_player.y, 0, max_y);
    g_player_qx = (int32_t)g_player.x * PHYSICS_Q_ONE;
    g_player_qy = (int32_t)g_player.y * PHYSICS_Q_ONE;
    g_player_vx = 0;
    g_player_vy = 0;
    g_player_hp = player_max_hp();
    g_player_soul = 0;
    g_player_focus_timer = 0;
    g_player_focus_input_grace = 0;
    g_player_air_jumps_used = 0;
    g_player_air_dash_available = 1;
    g_player_dash_timer = 0;
    g_player_wall_dir = 0;
    g_item_showcase_active = 0;
    g_player_invulnerable = PLAYER_RESPAWN_INVULNERABLE_FRAMES;
    g_player_hurt_lock = 0;
    g_player_on_ground = 0;
    g_coyote_frames = 0;
    g_jump_buffer_frames = 0;
    g_strike_timer = 0;
    g_strike_cooldown = 0;
    g_strike_hit_mask = 0;
    g_strike_direction = STRIKE_HORIZONTAL;
    update_camera(1);
    return 1;
}

static void constrain_corner_transition(void) {
    const int16_t room_width_px = (int16_t)(g_room.width * PICO_TILE_SIZE);
    if (g_player.x >= 0 && g_player.x < room_width_px) { return; }

    int16_t probe_world_y = 0;
    uint8_t toward_vertical_exit = 0;
    if (g_player_vy < 0 && g_player.y < ROOM_CORNER_MARGIN) {
        probe_world_y = (int16_t)(g_room.base_y * PICO_TILE_SIZE - 1);
        toward_vertical_exit = 1;
    } else if (g_player_vy > 0 &&
               g_player.y > (int16_t)(g_room.height * PICO_TILE_SIZE - ROOM_CORNER_MARGIN)) {
        probe_world_y = (int16_t)((g_room.base_y + g_room.height) * PICO_TILE_SIZE);
        toward_vertical_exit = 1;
    }
    if (!toward_vertical_exit) { return; }

    const int16_t clamped_local_x = clamp_i16(g_player.x, 0, (int16_t)(room_width_px - 1));
    const int16_t probe_world_x = (int16_t)(g_room.base_x * PICO_TILE_SIZE + clamped_local_x);
    uint8_t vertical_room = g_room_index;
    if (!find_room_for_world_pixel(probe_world_x, probe_world_y, &vertical_room) ||
        vertical_room == g_room_index) {
        return;
    }

    g_player.x = clamped_local_x;
    g_player_qx = (int32_t)g_player.x * PHYSICS_Q_ONE;
    g_player_vx = 0;
}

static uint8_t try_room_transition(void) {
    constrain_corner_transition();
    const Low_Knight_Room old_room = g_room;
    const int16_t world_x = (int16_t)(g_room.base_x * PICO_TILE_SIZE + g_player.x);
    const int16_t world_y = (int16_t)(g_room.base_y * PICO_TILE_SIZE + g_player.y);
    uint8_t next_room = g_room_index;
    if (!find_room_for_world_pixel(world_x, world_y, &next_room) || next_room == g_room_index) { return 0; }

    if (!load_room(next_room)) { return 0; }
    const int16_t max_x = (int16_t)(g_room.width * PICO_TILE_SIZE - 1);
    const int16_t max_y = (int16_t)(g_room.height * PICO_TILE_SIZE - 1);
    int16_t local_x = (int16_t)(world_x - g_room.base_x * PICO_TILE_SIZE);
    int16_t local_y = (int16_t)(world_y - g_room.base_y * PICO_TILE_SIZE);

    if ((int16_t)(g_room.base_x + g_room.width) <= old_room.base_x) {
        local_x = (int16_t)(g_room.width * PICO_TILE_SIZE - ROOM_ENTRY_INSET);
    } else if (g_room.base_x >= (int16_t)(old_room.base_x + old_room.width)) {
        local_x = ROOM_ENTRY_INSET;
    }
    if ((int16_t)(g_room.base_y + g_room.height) <= old_room.base_y) {
        local_y = (int16_t)(g_room.height * PICO_TILE_SIZE - ROOM_ENTRY_INSET);
    } else if (g_room.base_y >= (int16_t)(old_room.base_y + old_room.height)) {
        local_y = ROOM_ENTRY_INSET;
    }

    g_player.x = clamp_i16(local_x, 0, max_x);
    g_player.y = clamp_i16(local_y, 0, max_y);
    g_player_qx = (int32_t)g_player.x * PHYSICS_Q_ONE;
    g_player_qy = (int32_t)g_player.y * PHYSICS_Q_ONE;
    g_player_on_ground = 0;
    g_coyote_frames = 0;
    g_jump_buffer_frames = 0;
    g_strike_timer = 0;
    g_strike_hit_mask = 0;
    g_player_focus_timer = 0;
    g_player_focus_input_grace = 0;
    g_player_air_jumps_used = 0;
    g_player_air_dash_available = 1;
    g_player_dash_timer = 0;
    g_player_wall_dir = 0;
    g_item_showcase_active = 0;
    if (!g_has_bench_respawn) {
        g_respawn_room_index = g_room_index;
        g_respawn_player = g_player;
    }
    update_camera(1);
    return 1;
}

uint8_t Low_Knight_Runtime_Init(Low_Knight_Resources* resources) {
    if (resources == NULL || !resources->is_open) { return 0; }
    g_resources = resources;
    g_room = g_rooms[LOW_KNIGHT_START_ROOM];
    memset(g_tile_row_cache, 0, sizeof(g_tile_row_cache));
    if (!Low_Knight_Resources_Read_Gff(g_resources, 0, g_tile_flags, sizeof(g_tile_flags))) { return 0; }
    if (!cache_enemy_tiles()) { return 0; }
    g_player.x = (int16_t)(892 - (int16_t)g_room.base_x * PICO_TILE_SIZE);
    g_player.y = (int16_t)(583 - (int16_t)g_room.base_y * PICO_TILE_SIZE);
    g_player_qx = (int32_t)g_player.x * PHYSICS_Q_ONE;
    g_player_qy = (int32_t)g_player.y * PHYSICS_Q_ONE;
    g_player_vx = 0;
    g_player_vy = 0;
    g_player_facing_left = 1;
    g_player_on_ground = 0;
    g_coyote_frames = 0;
    g_jump_buffer_frames = 0;
    g_player_abilities = 0;
    g_nail_damage_bonus = 0;
    g_item_showcase_active = 0;
    g_item_showcase_tile = 0;
    g_cheat_enabled = 0;
    g_cheat_sequence_index = 0;
    g_cheat_last_direction = 0;
    g_collected_item_count = 0;
    memset(g_collected_item_keys, 0, sizeof(g_collected_item_keys));
    g_player_air_jumps_used = 0;
    g_player_air_dash_available = 1;
    g_player_dash_timer = 0;
    g_player_wall_dir = 0;
    g_player_hp = player_max_hp();
    g_player_soul = PLAYER_SOUL_MAX;
    g_player_focus_timer = 0;
    g_player_focus_input_grace = 0;
    g_player_invulnerable = 0;
    g_player_hurt_lock = 0;
    g_player_death_timer = 0;
    g_has_bench_respawn = 0;
    g_strike_timer = 0;
    g_strike_cooldown = 0;
    g_strike_hit_mask = 0;
    g_strike_x = g_player.x;
    g_strike_y = g_player.y;
    g_strike_facing_left = g_player_facing_left;
    g_strike_direction = STRIKE_HORIZONTAL;
    g_effect_rng = 0x4d2bu;
    g_runtime_frame = 0;
    g_last_drawn_player = g_player;
    if (!load_room(LOW_KNIGHT_START_ROOM)) { return 0; }
    g_respawn_room_index = g_room_index;
    g_respawn_player = g_player;
    update_camera(1);
    return 1;
}

void Low_Knight_Runtime_Draw(St7789* lcd) {
    if (lcd == NULL || g_resources == NULL) { return; }

    Game_Graphics_Fill_Rect(lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    render_region(lcd, screen_rect());
    capture_dynamic_draw_state();
    g_dynamic_redraw_due = 0;
    g_last_drawn_player = g_player;
    g_pending_dirty_count = 0;
}

void Low_Knight_Runtime_Draw_Dirty(St7789* lcd) {
    if (lcd == NULL || g_resources == NULL) { return; }

    if (g_pending_dirty_count == 0) {
        mark_dirty_rect(rect_union(player_rect_for(g_last_drawn_player, 2), player_rect_for(g_player, 2)));
    }

    for (uint8_t i = 0; i < g_pending_dirty_count; i++) {
        Low_Knight_Rect dirty = rect_intersect(g_pending_dirty[i], screen_rect());
        dirty = rect_intersect(dirty, (Low_Knight_Rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT});
        if (!rect_is_empty(dirty)) { render_region(lcd, dirty); }
    }
    if (g_dynamic_redraw_due) {
        capture_dynamic_draw_state();
        g_dynamic_redraw_due = 0;
    }
    g_last_drawn_player = g_player;
    g_pending_dirty_count = 0;
}

Low_Knight_Step_Result Low_Knight_Runtime_Step(const Low_Knight_Input* input) {
    if (input == NULL || g_resources == NULL) { return low_knight_step_none; }

    g_runtime_frame++;
    g_dynamic_redraw_due = 0;
    g_pending_dirty_count = 0;
    update_cheat_sequence(input);
    const Low_Knight_Vec2i before = g_player;
    const uint8_t before_facing_left = g_player_facing_left;
    const uint8_t before_moving = g_player_vx != 0;
    const uint8_t before_dash_timer = g_player_dash_timer;
    const int8_t before_wall_dir = g_player_wall_dir;
    const uint8_t before_invulnerable = g_player_invulnerable;
    const uint8_t before_strike_timer = g_strike_timer;
    const Low_Knight_Rect before_strike_rect =
        before_strike_timer > 0
            ? strike_rect_at(
                  g_strike_x, g_strike_y, g_strike_facing_left, g_strike_direction)
            : (Low_Knight_Rect){0, 0, 0, 0};
    const int16_t before_camera_x = g_camera_x;
    const int16_t before_camera_y = g_camera_y;

    if (g_item_showcase_active) {
        if (input->jump_pressed || input->strike_pressed) {
            g_item_showcase_active = 0;
            mark_dirty_rect(item_showcase_rect());
            return low_knight_step_dirty;
        }
        return low_knight_step_none;
    }

    if (g_player_death_timer > 0) {
        update_blood_particles();
        update_enemies();
        g_player_death_timer--;
        if (g_player_death_timer == 0) {
            return respawn_player() ? low_knight_step_transition : low_knight_step_none;
        }
        if ((g_runtime_frame % ENEMY_REDRAW_DIVIDER) == 0u) {
            queue_dynamic_dirty();
            g_dynamic_redraw_due = 1;
        }
        return g_pending_dirty_count > 0 ? low_knight_step_dirty : low_knight_step_none;
    }

    if (g_player_invulnerable > 0) { g_player_invulnerable--; }
    const uint8_t controls_locked = g_player_hurt_lock > 0;
    if (g_player_hurt_lock > 0) { g_player_hurt_lock--; }
    const uint8_t focusing = update_focus(input, controls_locked);

    int8_t move_x = input->move_x;
    if (move_x < -1) { move_x = -1; }
    if (move_x > 1) { move_x = 1; }
    uint8_t jump_pressed = input->jump_pressed;
    uint8_t jump_released = input->jump_released;
    if (g_player_dash_timer > 0) { g_player_dash_timer--; }

    if (!controls_locked && !focusing && g_player_dash_timer == 0) {
        g_player_vx = (int16_t)move_x * PLAYER_MOVE_SPEED_Q;
        if (move_x < 0) { g_player_facing_left = 1; }
        if (move_x > 0) { g_player_facing_left = 0; }
    } else {
        if (focusing) { g_player_vx = 0; }
        jump_pressed = 0;
        jump_released = 0;
        g_jump_buffer_frames = 0;
    }

    g_player_wall_dir = 0;
    if (!controls_locked && !focusing && !g_player_on_ground &&
        player_has_ability(ABILITY_HEART_OF_ROCK) && move_x != 0 &&
        player_touching_wall(move_x)) {
        g_player_wall_dir = move_x;
        g_player_air_jumps_used = 0;
        g_player_air_dash_available = 1;
        if (g_player_vy > PLAYER_WALL_SLIDE_Q) { g_player_vy = PLAYER_WALL_SLIDE_Q; }
    }

    if (g_player_on_ground) {
        g_coyote_frames = PLAYER_COYOTE_FRAMES;
        g_player_air_jumps_used = 0;
        g_player_air_dash_available = 1;
    } else if (g_coyote_frames > 0) {
        g_coyote_frames--;
    }
    if (jump_pressed) {
        g_jump_buffer_frames = PLAYER_JUMP_BUFFER_FRAMES;
    } else if (g_jump_buffer_frames > 0) {
        g_jump_buffer_frames--;
    }

    if (g_jump_buffer_frames > 0 && g_player_wall_dir != 0) {
        g_player_vx = (int16_t)(-g_player_wall_dir * PLAYER_WALL_JUMP_X_Q);
        g_player_vy = PLAYER_JUMP_Q;
        g_player_facing_left = g_player_wall_dir > 0;
        g_player_on_ground = 0;
        g_coyote_frames = 0;
        g_jump_buffer_frames = 0;
        g_player_air_jumps_used = 0;
        g_player_dash_timer = 0;
    } else if (g_jump_buffer_frames > 0 && g_coyote_frames > 0) {
        g_player_vy = PLAYER_JUMP_Q;
        g_player_on_ground = 0;
        g_coyote_frames = 0;
        g_jump_buffer_frames = 0;
        g_player_air_jumps_used = 0;
        g_player_dash_timer = 0;
    } else if (g_jump_buffer_frames > 0 && g_cheat_enabled) {
        g_player_vy = PLAYER_JUMP_Q;
        g_player_on_ground = 0;
        g_jump_buffer_frames = 0;
        g_player_air_jumps_used = 0;
        g_player_dash_timer = 0;
    } else if (g_jump_buffer_frames > 0 && player_has_ability(ABILITY_SOULWINGS) &&
               g_player_air_jumps_used == 0) {
        g_player_vy = (int16_t)(PLAYER_JUMP_Q * 4 / 5);
        g_player_on_ground = 0;
        g_jump_buffer_frames = 0;
        g_player_air_jumps_used = 1;
        g_player_dash_timer = 0;
        spawn_heal_effect();
    }
    if (jump_released && g_player_vy < PLAYER_JUMP_CUT_Q) { g_player_vy = PLAYER_JUMP_CUT_Q; }

    g_player_vy = (int16_t)(g_player_vy + PLAYER_GRAVITY_Q);
    if (g_player_vy > PLAYER_MAX_FALL_Q) { g_player_vy = PLAYER_MAX_FALL_Q; }

    resolve_player_horizontal();
    resolve_player_vertical();
    if (g_player_on_ground) {
        g_player_air_jumps_used = 0;
        g_player_air_dash_available = 1;
        g_player_dash_timer = 0;
    }
    const uint8_t bench_used = interact_with_bench(input);
    if (check_item_pickups()) { return low_knight_step_dirty; }
    if (try_room_transition()) { return low_knight_step_transition; }
    update_blood_particles();
    update_enemies();
    if (!bench_used) { update_strike(input); }
    check_player_damage();
    if ((g_runtime_frame % ENEMY_REDRAW_DIVIDER) == 0u) {
        queue_dynamic_dirty();
        g_dynamic_redraw_due = 1;
    }
    if (update_camera(0) || before_camera_x != g_camera_x || before_camera_y != g_camera_y) {
        return low_knight_step_transition;
    }

    if (before.x != g_player.x || before.y != g_player.y || before_facing_left != g_player_facing_left ||
        before_moving != (g_player_vx != 0) || before_dash_timer != g_player_dash_timer ||
        before_wall_dir != g_player_wall_dir) {
        mark_dirty_rect(player_rect_for(before, 2));
        mark_dirty_rect(player_rect_for(g_player, 2));
        mark_dirty_rect(nav_hud_rect());
    }
    if ((before_invulnerable > 0 || g_player_invulnerable > 0) &&
        (((g_runtime_frame & 1u) == 0u) || before_invulnerable == 1u)) {
        mark_dirty_rect(player_rect_for(g_player, 2));
    }
    if (before_strike_timer == 0 && g_strike_timer > 0) {
        mark_dirty_rect(
            strike_rect_at(g_strike_x, g_strike_y, g_strike_facing_left, g_strike_direction));
    } else if (before_strike_timer > 0 && g_strike_timer == 0) {
        mark_dirty_rect(before_strike_rect);
    }
    return g_pending_dirty_count > 0 ? low_knight_step_dirty : low_knight_step_none;
}

uint8_t Low_Knight_Runtime_Move_Player(int8_t dx, int8_t dy) {
    const int16_t max_x = (int16_t)g_room.width * PICO_TILE_SIZE - 1;
    const int16_t max_y = (int16_t)g_room.height * PICO_TILE_SIZE - 1;
    const Low_Knight_Vec2i before = g_player;
    g_player.x = (int16_t)(g_player.x + dx);
    g_player.y = (int16_t)(g_player.y + dy);
    if (g_player.x < 0) { g_player.x = 0; }
    if (g_player.y < 0) { g_player.y = 0; }
    if (g_player.x > max_x) { g_player.x = max_x; }
    if (g_player.y > max_y) { g_player.y = max_y; }
    g_player_qx = (int32_t)g_player.x * PHYSICS_Q_ONE;
    g_player_qy = (int32_t)g_player.y * PHYSICS_Q_ONE;
    return before.x != g_player.x || before.y != g_player.y;
}

Low_Knight_Vec2i Low_Knight_Runtime_Get_Player(void) { return g_player; }
