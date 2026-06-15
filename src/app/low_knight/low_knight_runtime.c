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
static Low_Knight_Room g_room;
static uint8_t g_room_tiles[LOW_KNIGHT_MAX_ROOM_W * LOW_KNIGHT_MAX_ROOM_H];
static uint8_t g_background_tiles[LOW_KNIGHT_BG_TILE_W * LOW_KNIGHT_BG_TILE_H];
static Tile_Row_Cache_Entry g_tile_row_cache[TILE_ROW_CACHE_SIZE];
static Low_Knight_Vec2i g_player;
static Low_Knight_Vec2i g_last_drawn_player;

static uint8_t sample_packed_pixel(const uint8_t* bytes, uint8_t pixel_index) {
    const uint8_t packed = bytes[pixel_index / 2u];
    return (pixel_index & 1u) == 0 ? (uint8_t)(packed >> 4) : (uint8_t)(packed & 0x0fu);
}

static uint8_t is_transparent_color(uint8_t color) {
    /*
     * Low Knight rebuilds PICO-8 draw palettes from the spritesheet and only
     * marks color 14 transparent. Color 0 is a visible ink color here.
     */
    return color == 14u;
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

static void compose_sprite_tile_line(uint16_t* line, int16_t region_x, int16_t region_width,
    int16_t screen_y, uint8_t tile, int16_t sprite_x, int16_t sprite_y, uint8_t scale) {
    uint8_t row_data[4];

    const int16_t local_y = (int16_t)(screen_y - sprite_y);
    if (local_y < 0 || local_y >= (int16_t)(PICO_TILE_SIZE * scale)) { return; }
    if (!read_gfx_tile_row(tile, (uint8_t)(local_y / scale), row_data)) { return; }

    const int16_t start = sprite_x > region_x ? sprite_x : region_x;
    const int16_t end = sprite_x + PICO_TILE_SIZE * scale < region_x + region_width
                            ? (int16_t)(sprite_x + PICO_TILE_SIZE * scale)
                            : (int16_t)(region_x + region_width);
    for (int16_t screen_x = start; screen_x < end; screen_x++) {
        const int16_t local_x = (int16_t)(screen_x - sprite_x);
        if (local_x < 0 || local_x >= (int16_t)(PICO_TILE_SIZE * scale)) { continue; }
        const uint8_t color = sample_packed_pixel(row_data, (uint8_t)(local_x / scale));
        if (!is_transparent_color(color)) { line[screen_x - region_x] = g_pico_palette[color]; }
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
        (int16_t)(view.x1 - 2 * scale), view.y1, 19, map_height, scale);
    compose_map_region_line(line, region_x, region_width, screen_y, LOW_KNIGHT_BG_TILE_X, LOW_KNIGHT_BG_TILE_Y,
        (int16_t)(view.x1 - 2 * scale), (int16_t)(view.y1 + 4 * scale), 19, map_height, scale);
}

static void compose_level_line(uint16_t* line, int16_t region_x, int16_t region_width, int16_t screen_y) {
    const Low_Knight_Rect view = screen_rect();
    const uint8_t scale = 2;
    const int16_t pico_y = (int16_t)((screen_y - view.y1) / scale);
    if (pico_y < 0 || pico_y >= PICO_SCREEN_SIZE) { return; }

    const uint8_t tile_y = (uint8_t)(pico_y / PICO_TILE_SIZE);
    if (tile_y >= g_room.height) { return; }
    const uint8_t tile_row = (uint8_t)(pico_y & 7);

    int16_t x = region_x;
    while (x < region_x + region_width) {
        const int16_t pico_x = (int16_t)((x - view.x1) / scale);
        if (pico_x < 0 || pico_x >= PICO_SCREEN_SIZE) {
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

        const int16_t tile_screen_x = (int16_t)(view.x1 + tile_x * PICO_TILE_SIZE * scale);
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
    const int16_t x = (int16_t)(view.x1 + g_player.x * scale);
    const int16_t y = (int16_t)(view.y1 + g_player.y * scale);

    compose_sprite_tile_line(line, region_x, region_width, screen_y, 78, (int16_t)(x - 8 * scale),
        (int16_t)(y - 4 * scale), scale);
    compose_sprite_tile_line(line, region_x, region_width, screen_y, 79, x, (int16_t)(y - 4 * scale), scale);
    compose_sprite_tile_line(line, region_x, region_width, screen_y, 47, (int16_t)(x - 4 * scale),
        (int16_t)(y - 12 * scale), scale);
}

static void compose_line(int16_t x, int16_t y, int16_t width) {
    uint16_t* line = Game_Graphics_Get_Line_Buffer();
    for (int16_t col = 0; col < width; col++) { line[col] = g_pico_palette[1]; }
    compose_background_line(line, x, width, y);
    compose_level_line(line, x, width, y);
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
    const int16_t x = (int16_t)(view.x1 + player.x * scale);
    const int16_t y = (int16_t)(view.y1 + player.y * scale);
    return (Low_Knight_Rect){
        (int16_t)(x - 8 * scale - 2),
        (int16_t)(y - 12 * scale - 2),
        (int16_t)(x + 8 * scale + 2),
        (int16_t)(y + 4 * scale + 2),
    };
}

uint8_t Low_Knight_Runtime_Init(Low_Knight_Resources* resources) {
    if (resources == NULL || !resources->is_open) { return 0; }
    g_resources = resources;
    g_room = g_rooms[LOW_KNIGHT_START_ROOM];
    memset(g_tile_row_cache, 0, sizeof(g_tile_row_cache));
    g_player.x = (int16_t)(892 - (int16_t)g_room.base_x * PICO_TILE_SIZE);
    g_player.y = (int16_t)(583 - (int16_t)g_room.base_y * PICO_TILE_SIZE);
    g_last_drawn_player = g_player;
    if (!unpack_room(&g_room)) { return 0; }
    return g_room.no_background || cache_background_map();
}

void Low_Knight_Runtime_Draw(St7789* lcd) {
    if (lcd == NULL || g_resources == NULL) { return; }

    Game_Graphics_Fill_Rect(lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    render_region(lcd, screen_rect());
    g_last_drawn_player = g_player;
}

void Low_Knight_Runtime_Draw_Dirty(St7789* lcd) {
    if (lcd == NULL || g_resources == NULL) { return; }

    Low_Knight_Rect dirty = rect_union(player_rect_for(g_last_drawn_player, 2), player_rect_for(g_player, 2));
    dirty = rect_intersect(dirty, screen_rect());
    dirty = rect_intersect(dirty, (Low_Knight_Rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT});
    if (rect_is_empty(dirty)) { return; }

    render_region(lcd, dirty);
    g_last_drawn_player = g_player;
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
    return before.x != g_player.x || before.y != g_player.y;
}

Low_Knight_Vec2i Low_Knight_Runtime_Get_Player(void) { return g_player; }
