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
#define TILE_ROW_CACHE_SIZE    128u

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
#define PLAYER_COYOTE_FRAMES   3u
#define PLAYER_JUMP_BUFFER_FRAMES 4u
#define ROOM_ENTRY_INSET       24
#define ROOM_CORNER_MARGIN     24
#define LOW_KNIGHT_MAX_ENEMIES 12u
#define ENEMY_BUSH_TILE        207u
#define BUSH_MOVE_SPEED_Q      192
#define BUSH_GRAVITY_Q         96
#define BUSH_MAX_FALL_Q        768
#define LOW_KNIGHT_MAX_DIRTY_RECTS 16u

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
    int16_t x;
    int16_t y;
    int16_t vx;
    int16_t vy;
    int8_t way;
    uint8_t type;
    uint8_t active;
} Low_Knight_Enemy;

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

static Low_Knight_Resources* g_resources = NULL;
static uint8_t g_room_index;
static Low_Knight_Room g_room;
static uint8_t g_room_tiles[LOW_KNIGHT_MAX_ROOM_W * LOW_KNIGHT_MAX_ROOM_H];
static uint8_t g_background_tiles[LOW_KNIGHT_BG_TILE_W * LOW_KNIGHT_BG_TILE_H];
static uint8_t g_tile_flags[LOW_KNIGHT_GFF_SIZE];
static Tile_Row_Cache_Entry g_tile_row_cache[TILE_ROW_CACHE_SIZE];
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
static int16_t g_camera_x;
static int16_t g_camera_y;
static Low_Knight_Enemy g_enemies[LOW_KNIGHT_MAX_ENEMIES];
static uint8_t g_enemy_count;
static Low_Knight_Rect g_pending_dirty[LOW_KNIGHT_MAX_DIRTY_RECTS];
static uint8_t g_pending_dirty_count;

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

static void spawn_room_enemies(void) {
    memset(g_enemies, 0, sizeof(g_enemies));
    g_enemy_count = 0;

    for (uint8_t tile_y = 0; tile_y < g_room.height; tile_y++) {
        for (uint8_t tile_x = 0; tile_x < g_room.width; tile_x++) {
            uint8_t* tile = &g_room_tiles[(uint16_t)tile_y * g_room.width + tile_x];
            if (*tile != ENEMY_BUSH_TILE || g_enemy_count >= LOW_KNIGHT_MAX_ENEMIES) { continue; }

            Low_Knight_Enemy* enemy = &g_enemies[g_enemy_count++];
            enemy->x = (int16_t)(tile_x * PICO_TILE_SIZE);
            enemy->y = (int16_t)(tile_y * PICO_TILE_SIZE);
            enemy->qx = (int32_t)enemy->x * PHYSICS_Q_ONE;
            enemy->qy = (int32_t)enemy->y * PHYSICS_Q_ONE;
            enemy->way = tile_x >= g_room.width / 2u ? -1 : 1;
            enemy->type = ENEMY_BUSH_TILE;
            enemy->active = 1;
            *tile = 0;
        }
    }
}

static uint8_t read_gfx_tile_row(uint8_t tile, uint8_t row, uint8_t* out_row) {
    if (out_row == NULL) { return 0; }

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
    if (!unpack_room(&g_room)) { return 0; }
    spawn_room_enemies();
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
    const int16_t x = (int16_t)(view.x1 + (enemy->x - g_camera_x) * scale);
    const int16_t y = (int16_t)(view.y1 + (enemy->y - g_camera_y) * scale);
    return (Low_Knight_Rect){
        (int16_t)(x - 12 * scale),
        (int16_t)(y - 6 * scale),
        (int16_t)(x + 12 * scale),
        (int16_t)(y + 6 * scale),
    };
}

static void compose_sprite_rect_line(uint16_t* line, int16_t region_x, int16_t region_width,
    int16_t screen_y, uint8_t tile, uint8_t width_tiles, uint8_t height_tiles, int16_t sprite_x,
    int16_t sprite_y, uint8_t scale, uint8_t hflip) {
    const int16_t local_y = (int16_t)(screen_y - sprite_y);
    if (local_y < 0 || local_y >= (int16_t)(height_tiles * PICO_TILE_SIZE * scale)) { return; }

    const uint8_t source_y = (uint8_t)(local_y / scale);
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

static void compose_enemies_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;

    for (uint8_t i = 0; i < g_enemy_count; i++) {
        const Low_Knight_Enemy* enemy = &g_enemies[i];
        if (!enemy->active || enemy->type != ENEMY_BUSH_TILE) { continue; }

        const int16_t x = (int16_t)(view.x1 + (enemy->x - g_camera_x) * scale);
        const int16_t y = (int16_t)(view.y1 + (enemy->y - g_camera_y) * scale);
        const uint8_t flip = enemy->way < 0;
        const int16_t trail = (int16_t)(enemy->way * -3 * scale);
        compose_sprite_rect_line(line, region_x, region_width, screen_y, 205, 1, 1,
            (int16_t)(x - 4 * scale + trail * 2), (int16_t)(y - 4 * scale), scale, flip);
        compose_sprite_rect_line(line, region_x, region_width, screen_y, 206, 1, 1,
            (int16_t)(x - 4 * scale + trail), (int16_t)(y - 4 * scale), scale, flip);
        compose_sprite_rect_line(line, region_x, region_width, screen_y, 207, 1, 1,
            (int16_t)(x - 4 * scale), (int16_t)(y - 4 * scale), scale, flip);
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
    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    const int16_t x = (int16_t)(view.x1 + (g_player.x - g_camera_x) * scale);
    const int16_t y = (int16_t)(view.y1 + (g_player.y - g_camera_y) * scale);
    const uint8_t body_tile = g_player_vx != 0 ? 76u : 78u;

    compose_sprite_rect_line(line, region_x, region_width, screen_y, body_tile, 2, 1,
        (int16_t)(x - 8 * scale), (int16_t)(y - 4 * scale), scale, g_player_facing_left);
    compose_sprite_rect_line(line, region_x, region_width, screen_y, 47, 1, 1,
        (int16_t)(x - 4 * scale), (int16_t)(y - 12 * scale), scale, g_player_facing_left);
}

static void compose_line(int16_t x, int16_t y, int16_t width) {
    uint16_t* line = Game_Graphics_Get_Line_Buffer();
    for (int16_t col = 0; col < width; col++) { line[col] = g_pico_palette[1]; }
    compose_background_line(line, x, width, y);
    compose_level_line(line, x, width, y);
    compose_enemies_line(line, x, width, y);
    compose_player_line(line, x, width, y);
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

static void update_bush(Low_Knight_Enemy* enemy) {
    const Low_Knight_Rect before_rect = enemy_rect_for(enemy);

    const int16_t ahead_x = (int16_t)(enemy->x + enemy->way * 7);
    const int16_t ahead_y = (int16_t)(enemy->y + 4);
    if (enemy->vy == 0 && !tile_is_solid_at(floor_div_tile(ahead_x), floor_div_tile(ahead_y))) {
        enemy->way = -enemy->way;
    }

    enemy->vx = (int16_t)(enemy->way * BUSH_MOVE_SPEED_Q);
    enemy->qx += enemy->vx;
    enemy->x = q_to_pixel(enemy->qx);
    Low_Knight_Box box = bush_hitbox_at(enemy->x, enemy->y);
    if (box_overlaps_solid(box)) {
        const int16_t push = enemy->vx > 0 ? -1 : 1;
        for (uint8_t i = 0; i < PICO_TILE_SIZE * 2u && box_overlaps_solid(box); i++) {
            enemy->x = (int16_t)(enemy->x + push);
            box = bush_hitbox_at(enemy->x, enemy->y);
        }
        enemy->qx = (int32_t)enemy->x * PHYSICS_Q_ONE;
        enemy->way = -enemy->way;
    }

    enemy->vy = (int16_t)(enemy->vy + BUSH_GRAVITY_Q);
    if (enemy->vy > BUSH_MAX_FALL_Q) { enemy->vy = BUSH_MAX_FALL_Q; }
    enemy->qy += enemy->vy;
    enemy->y = q_to_pixel(enemy->qy);
    box = bush_hitbox_at(enemy->x, enemy->y);
    if (box_overlaps_solid(box)) {
        const int16_t push = enemy->vy > 0 ? -1 : 1;
        for (uint8_t i = 0; i < PICO_TILE_SIZE * 2u && box_overlaps_solid(box); i++) {
            enemy->y = (int16_t)(enemy->y + push);
            box = bush_hitbox_at(enemy->x, enemy->y);
        }
        enemy->qy = (int32_t)enemy->y * PHYSICS_Q_ONE;
        enemy->vy = 0;
    }

    const Low_Knight_Rect after_rect = enemy_rect_for(enemy);
    if (before_rect.x1 != after_rect.x1 || before_rect.y1 != after_rect.y1 ||
        before_rect.x2 != after_rect.x2 || before_rect.y2 != after_rect.y2) {
        mark_dirty_rect(before_rect);
        mark_dirty_rect(after_rect);
    }
}

static void update_enemies(void) {
    for (uint8_t i = 0; i < g_enemy_count; i++) {
        Low_Knight_Enemy* enemy = &g_enemies[i];
        if (!enemy->active) { continue; }
        if (enemy->type == ENEMY_BUSH_TILE) { update_bush(enemy); }
    }
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
    update_camera(1);
    return 1;
}

uint8_t Low_Knight_Runtime_Init(Low_Knight_Resources* resources) {
    if (resources == NULL || !resources->is_open) { return 0; }
    g_resources = resources;
    g_room = g_rooms[LOW_KNIGHT_START_ROOM];
    memset(g_tile_row_cache, 0, sizeof(g_tile_row_cache));
    if (!Low_Knight_Resources_Read_Gff(g_resources, 0, g_tile_flags, sizeof(g_tile_flags))) { return 0; }
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
    g_last_drawn_player = g_player;
    if (!load_room(LOW_KNIGHT_START_ROOM)) { return 0; }
    update_camera(1);
    return 1;
}

void Low_Knight_Runtime_Draw(St7789* lcd) {
    if (lcd == NULL || g_resources == NULL) { return; }

    Game_Graphics_Fill_Rect(lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    render_region(lcd, screen_rect());
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
    g_last_drawn_player = g_player;
    g_pending_dirty_count = 0;
}

Low_Knight_Step_Result Low_Knight_Runtime_Step(const Low_Knight_Input* input) {
    if (input == NULL || g_resources == NULL) { return low_knight_step_none; }

    g_pending_dirty_count = 0;
    const Low_Knight_Vec2i before = g_player;
    const uint8_t before_facing_left = g_player_facing_left;
    const uint8_t before_moving = g_player_vx != 0;
    const int16_t before_camera_x = g_camera_x;
    const int16_t before_camera_y = g_camera_y;
    int8_t move_x = input->move_x;
    if (move_x < -1) { move_x = -1; }
    if (move_x > 1) { move_x = 1; }
    uint8_t jump_pressed = input->jump_pressed;
    uint8_t jump_released = input->jump_released;

    g_player_vx = (int16_t)move_x * PLAYER_MOVE_SPEED_Q;
    if (move_x < 0) { g_player_facing_left = 1; }
    if (move_x > 0) { g_player_facing_left = 0; }

    if (g_player_on_ground) {
        g_coyote_frames = PLAYER_COYOTE_FRAMES;
    } else if (g_coyote_frames > 0) {
        g_coyote_frames--;
    }
    if (jump_pressed) {
        g_jump_buffer_frames = PLAYER_JUMP_BUFFER_FRAMES;
    } else if (g_jump_buffer_frames > 0) {
        g_jump_buffer_frames--;
    }

    if (g_jump_buffer_frames > 0 && g_coyote_frames > 0) {
        g_player_vy = PLAYER_JUMP_Q;
        g_player_on_ground = 0;
        g_coyote_frames = 0;
        g_jump_buffer_frames = 0;
    }
    if (jump_released && g_player_vy < PLAYER_JUMP_CUT_Q) { g_player_vy = PLAYER_JUMP_CUT_Q; }

    g_player_vy = (int16_t)(g_player_vy + PLAYER_GRAVITY_Q);
    if (g_player_vy > PLAYER_MAX_FALL_Q) { g_player_vy = PLAYER_MAX_FALL_Q; }

    resolve_player_horizontal();
    resolve_player_vertical();
    if (try_room_transition()) { return low_knight_step_transition; }
    update_enemies();
    if (update_camera(0) || before_camera_x != g_camera_x || before_camera_y != g_camera_y) {
        return low_knight_step_transition;
    }

    if (before.x != g_player.x || before.y != g_player.y || before_facing_left != g_player_facing_left ||
        before_moving != (g_player_vx != 0)) {
        mark_dirty_rect(player_rect_for(before, 2));
        mark_dirty_rect(player_rect_for(g_player, 2));
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
