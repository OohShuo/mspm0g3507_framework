#pragma once

#include <stdint.h>

#if FRAMEWORK_USE_LFS
#include "lfs.h"
#endif

#define IMAGE_ASSET_FLAG_MASK 0x01u

typedef struct {
#if FRAMEWORK_USE_LFS
    lfs_file_t file;
#endif
    uint16_t width;
    uint16_t height;
    uint8_t flags;
    uint8_t is_open;
    uint16_t asset_id;
    uint32_t pixel_data_size;
    uint32_t next_read_offset;
    uint32_t raw_pixel_address;
} Image_asset;

uint8_t Image_Asset_Open(Image_asset* image, const char* path);
void Image_Asset_Close(Image_asset* image);
uint8_t Image_Asset_Prepare_Raw_Cache(
    Image_asset* image, uint32_t address, uint32_t capacity);
uint8_t Image_Asset_Read_Span(Image_asset* image, uint16_t y, uint16_t x,
    uint16_t width, uint16_t* pixels);
