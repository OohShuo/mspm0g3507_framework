#pragma once

#include <stdint.h>

#if FRAMEWORK_USE_LFS
#include "lfs.h"
#endif

#define LOW_KNIGHT_RESOURCE_PATH "/low_knight.p8r"
#define LOW_KNIGHT_GFX_SIZE      8192u
#define LOW_KNIGHT_GFF_SIZE      256u
#define LOW_KNIGHT_MAP_SIZE      4096u

typedef struct {
#if FRAMEWORK_USE_LFS
    lfs_file_t file;
#endif
    uint32_t gfx_offset;
    uint32_t gff_offset;
    uint32_t map_offset;
    uint32_t gfx_size;
    uint32_t gff_size;
    uint32_t map_size;
    uint16_t crc16;
    uint8_t is_open;
} Low_Knight_Resources;

uint8_t Low_Knight_Resources_Open(Low_Knight_Resources* resources, const char* path);
void Low_Knight_Resources_Close(Low_Knight_Resources* resources);
uint8_t Low_Knight_Resources_Read_Gfx(
    Low_Knight_Resources* resources, uint32_t offset, void* data, uint32_t size);
uint8_t Low_Knight_Resources_Read_Map(
    Low_Knight_Resources* resources, uint32_t offset, void* data, uint32_t size);
uint8_t Low_Knight_Resources_Read_Gff(
    Low_Knight_Resources* resources, uint32_t offset, void* data, uint32_t size);
