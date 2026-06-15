#pragma once

#include <stdint.h>

#if FRAMEWORK_USE_LFS
#include "lfs.h"
#endif

#define VIDEO_ASSET_HEADER_SIZE 24u
#define VIDEO_ASSET_VERSION     1u
#define VIDEO_ASSET_FLAG_RLE    0x01u
#define VIDEO_ASSET_FLAG_INDEX8 0x02u
#define VIDEO_ASSET_FLAG_INDEX1 0x04u

typedef struct {
#if FRAMEWORK_USE_LFS
    lfs_file_t file;
#endif
    uint16_t width;
    uint16_t height;
    uint16_t fps;
    uint32_t frame_count;
    uint32_t frame_size;
    uint32_t data_size;
    uint32_t data_offset;
    uint32_t rle_next_row_offset;
    uint32_t rle_current_frame;
    uint32_t cached_frame_index;
    uint16_t cached_frame_size;
    uint16_t rle_next_row;
    uint16_t palette[256];
    uint8_t flags;
    uint8_t is_open;
    uint8_t cached_frame_valid;
    uint32_t next_read_offset;
} Video_asset;

uint8_t Video_Asset_Open(Video_asset* video, const char* path);
void Video_Asset_Close(Video_asset* video);
uint8_t Video_Asset_Read_Frame_Span(Video_asset* video, uint32_t frame_index,
    uint16_t y, uint16_t x, uint16_t width, uint16_t* pixels);
uint8_t Video_Asset_Read_Frame_Row(
    Video_asset* video, uint32_t frame_index, uint16_t y, uint16_t* pixels);
