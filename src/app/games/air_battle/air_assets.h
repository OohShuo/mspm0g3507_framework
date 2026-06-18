#pragma once

#include <stdint.h>

#define AIR_BACKGROUND_WIDTH  80
#define AIR_BACKGROUND_HEIGHT 107
#define AIR_BACKGROUND_SCALE  3

/* Sprite stored as 16-color palette + 4-bit indexed data (2 pixels per byte).
   Index 0 is always transparent. Use Game_Graphics_Draw_Pal4_Bitmap to render. */
typedef struct {
    uint8_t width;
    uint8_t height;
    const uint16_t* palette; /* 16-entry RGB565 palette */
    const uint8_t* data;     /* 4-bit indices, 2 pixels per byte, row-major */
} Air_sprite;

extern const uint16_t air_background[AIR_BACKGROUND_WIDTH * AIR_BACKGROUND_HEIGHT];
extern const Air_sprite air_sprite_hero;
extern const Air_sprite air_sprite_mob;
extern const Air_sprite air_sprite_elite;
extern const Air_sprite air_sprite_elite_pro;
extern const Air_sprite air_sprite_boss;
extern const Air_sprite air_sprite_bullet_hero;
extern const Air_sprite air_sprite_bullet_enemy;
extern const Air_sprite air_sprite_prop_blood;
extern const Air_sprite air_sprite_prop_bomb;
extern const Air_sprite air_sprite_prop_bullet;
