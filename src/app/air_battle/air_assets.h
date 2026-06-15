#pragma once

#include <stdint.h>

#define AIR_BACKGROUND_WIDTH  80
#define AIR_BACKGROUND_HEIGHT 107
#define AIR_BACKGROUND_SCALE  3

typedef struct {
    uint8_t width;
    uint8_t height;
    const uint16_t* pixels;
    const uint8_t* mask;
} Air_sprite;

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
