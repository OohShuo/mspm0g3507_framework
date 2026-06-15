#include "video_asset.h"

#include <stddef.h>
#include <string.h>

#include "storage.h"

#define VIDEO_ASSET_MAX_DECODE_ROW_BYTES 512u
#define VIDEO_ASSET_FRAME_CACHE_SIZE     2048u

static uint8_t g_frame_cache[VIDEO_ASSET_FRAME_CACHE_SIZE];
static uint8_t g_rle_row_buffer[VIDEO_ASSET_MAX_DECODE_ROW_BYTES];
static uint8_t g_index1_row_buffer[VIDEO_ASSET_MAX_DECODE_ROW_BYTES];

#if FRAMEWORK_USE_LFS
static uint16_t read_u16_le(const uint8_t* data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_u32_le(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint8_t read_exact(Video_asset* video, uint32_t offset, void* data, uint32_t size) {
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL || video == NULL || data == NULL || size == 0) { return 0; }

    Storage_Lock();
    const lfs_soff_t seek_result =
        video->next_read_offset == offset
            ? (lfs_soff_t)offset
            : lfs_file_seek(lfs, &video->file, (lfs_soff_t)offset, LFS_SEEK_SET);
    const lfs_ssize_t read_result =
        seek_result < 0 ? seek_result : lfs_file_read(lfs, &video->file, data, size);
    Storage_Unlock();

    video->next_read_offset =
        read_result == (lfs_ssize_t)size ? offset + size : UINT32_MAX;
    return read_result == (lfs_ssize_t)size;
}

static uint8_t read_frame_data_offset(Video_asset* video, uint32_t frame_index, uint32_t* offset) {
    if (video == NULL || offset == NULL || frame_index >= video->frame_count) { return 0; }
    if ((video->flags & VIDEO_ASSET_FLAG_RLE) == 0u) {
        *offset = video->data_offset + frame_index * video->frame_size;
        return 1;
    }

    uint8_t bytes[4];
    uint32_t palette_size = 0;
    if ((video->flags & VIDEO_ASSET_FLAG_INDEX8) != 0u) {
        palette_size = 512u;
    } else if ((video->flags & VIDEO_ASSET_FLAG_INDEX1) != 0u) {
        palette_size = 4u;
    }
    const uint32_t table_offset =
        VIDEO_ASSET_HEADER_SIZE + palette_size;
    if (!read_exact(video, table_offset + frame_index * 4u, bytes, sizeof(bytes))) {
        return 0;
    }

    const uint32_t relative_offset = read_u32_le(bytes);
    if (relative_offset >= video->data_size) { return 0; }
    *offset = video->data_offset + relative_offset;
    return 1;
}

static uint8_t read_frame_data_size(
    Video_asset* video, uint32_t frame_index, uint32_t* offset, uint32_t* size) {
    if (video == NULL || offset == NULL || size == NULL) { return 0; }
    if (!read_frame_data_offset(video, frame_index, offset)) { return 0; }

    uint32_t next_offset = video->data_offset + video->data_size;
    if (frame_index + 1u < video->frame_count &&
        !read_frame_data_offset(video, frame_index + 1u, &next_offset)) {
        return 0;
    }
    if (next_offset < *offset || next_offset > video->data_offset + video->data_size) {
        return 0;
    }

    *size = next_offset - *offset;
    return 1;
}

static uint8_t load_frame_cache(Video_asset* video, uint32_t frame_index) {
    if (video == NULL || (video->flags & VIDEO_ASSET_FLAG_RLE) == 0u) { return 0; }
    if (video->cached_frame_valid && video->cached_frame_index == frame_index) { return 1; }

    uint32_t offset = 0;
    uint32_t size = 0;
    if (!read_frame_data_size(video, frame_index, &offset, &size) ||
        size == 0u || size > sizeof(g_frame_cache)) {
        video->cached_frame_valid = 0;
        return 0;
    }
    if (!read_exact(video, offset, g_frame_cache, size)) {
        video->cached_frame_valid = 0;
        return 0;
    }

    video->cached_frame_index = frame_index;
    video->cached_frame_size = (uint16_t)size;
    video->cached_frame_valid = 1;
    return 1;
}

static void expand_index1_row(
    const uint8_t* packed, uint16_t width, const uint16_t* palette, uint16_t* pixels) {
    for (uint16_t x = 0; x < width; x++) {
        const uint8_t bit = (uint8_t)((packed[x / 8u] >> (7u - (x & 7u))) & 1u);
        pixels[x] = palette[bit];
    }
}

static uint8_t decode_rle_encoded_row(
    Video_asset* video, uint16_t encoded_len, uint16_t* pixels) {
    uint16_t written = 0;
    uint16_t read_index = 0;
    const uint8_t uses_index1 = (video->flags & VIDEO_ASSET_FLAG_INDEX1) != 0u;
    const uint16_t expected_units =
        uses_index1 ? (uint16_t)(((uint32_t)video->width + 7u) / 8u) : video->width;

    while (read_index < encoded_len && written < expected_units) {
        const uint8_t token = g_rle_row_buffer[read_index++];
        const uint16_t count = (uint16_t)((token & 0x7fu) + 1u);
        if ((token & 0x80u) != 0u) {
            if (written + count > expected_units) { return 0; }
            uint16_t pixel = 0;
            if (uses_index1) {
                if (read_index + 1u > encoded_len) { return 0; }
                const uint8_t packed = g_rle_row_buffer[read_index++];
                for (uint16_t i = 0; i < count; i++) { g_index1_row_buffer[written++] = packed; }
                continue;
            } else if ((video->flags & VIDEO_ASSET_FLAG_INDEX8) != 0u) {
                if (read_index + 1u > encoded_len) { return 0; }
                pixel = video->palette[g_rle_row_buffer[read_index++]];
            } else {
                if (read_index + 2u > encoded_len) { return 0; }
                pixel = read_u16_le(&g_rle_row_buffer[read_index]);
                read_index = (uint16_t)(read_index + 2u);
            }
            for (uint16_t i = 0; i < count; i++) { pixels[written++] = pixel; }
        } else {
            if (written + count > expected_units) { return 0; }
            if (uses_index1) {
                if (read_index + count > encoded_len) { return 0; }
                memcpy(&g_index1_row_buffer[written], &g_rle_row_buffer[read_index], count);
                read_index = (uint16_t)(read_index + count);
                written = (uint16_t)(written + count);
            } else if ((video->flags & VIDEO_ASSET_FLAG_INDEX8) != 0u) {
                if (read_index + count > encoded_len) { return 0; }
                for (uint16_t i = 0; i < count; i++) {
                    pixels[written++] = video->palette[g_rle_row_buffer[read_index++]];
                }
            } else {
                const uint16_t byte_count = (uint16_t)(count * 2u);
                if (read_index + byte_count > encoded_len) { return 0; }
                memcpy(&pixels[written], &g_rle_row_buffer[read_index], byte_count);
                read_index = (uint16_t)(read_index + byte_count);
                written = (uint16_t)(written + count);
            }
        }
    }

    if (read_index != encoded_len || written != expected_units) { return 0; }
    if (uses_index1) { expand_index1_row(g_index1_row_buffer, video->width, video->palette, pixels); }
    return 1;
}

static uint8_t read_cached_rle_row(Video_asset* video, uint16_t y, uint16_t* pixels) {
    uint32_t offset = 0;
    for (uint16_t row = 0; row < y; row++) {
        if (offset + 2u > video->cached_frame_size) { return 0; }
        const uint16_t encoded_len = read_u16_le(&g_frame_cache[offset]);
        offset += 2u + encoded_len;
        if (offset > video->cached_frame_size) { return 0; }
    }

    if (offset + 2u > video->cached_frame_size) { return 0; }
    const uint16_t encoded_len = read_u16_le(&g_frame_cache[offset]);
    offset += 2u;
    if (encoded_len == 0u || encoded_len > sizeof(g_rle_row_buffer) ||
        offset + encoded_len > video->cached_frame_size) {
        return 0;
    }

    memcpy(g_rle_row_buffer, &g_frame_cache[offset], encoded_len);
    return decode_rle_encoded_row(video, encoded_len, pixels);
}
#endif

uint8_t Video_Asset_Open(Video_asset* video, const char* path) {
    if (video == NULL || path == NULL) { return 0; }
    Video_Asset_Close(video);

#if FRAMEWORK_USE_LFS
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    uint8_t header[VIDEO_ASSET_HEADER_SIZE];
    Storage_Lock();
    int result = lfs_file_open(lfs, &video->file, path, LFS_O_RDONLY);
    if (result == 0) {
        result = (int)lfs_file_read(lfs, &video->file, header, sizeof(header));
    }
    const lfs_soff_t file_size =
        result == (int)sizeof(header) ? lfs_file_size(lfs, &video->file) : -1;
    Storage_Unlock();

    if (result != (int)sizeof(header) || file_size < (lfs_soff_t)sizeof(header) ||
        memcmp(header, "V565", 4) != 0 || header[4] != VIDEO_ASSET_VERSION) {
        if (result >= 0) {
            Storage_Lock();
            lfs_file_close(lfs, &video->file);
            Storage_Unlock();
        }
        memset(video, 0, sizeof(*video));
        return 0;
    }

    video->flags = header[5];
    video->width = read_u16_le(header + 6);
    video->height = read_u16_le(header + 8);
    video->fps = read_u16_le(header + 10);
    video->frame_count = read_u32_le(header + 12);
    video->frame_size = read_u32_le(header + 16);
    video->data_size = read_u32_le(header + 20);

    const uint32_t expected_frame_size = (uint32_t)video->width * video->height * 2u;
    const uint8_t uses_rle = (video->flags & VIDEO_ASSET_FLAG_RLE) != 0u;
    const uint8_t uses_index8 = (video->flags & VIDEO_ASSET_FLAG_INDEX8) != 0u;
    const uint8_t uses_index1 = (video->flags & VIDEO_ASSET_FLAG_INDEX1) != 0u;
    const uint8_t supported_flags =
        VIDEO_ASSET_FLAG_RLE | VIDEO_ASSET_FLAG_INDEX8 | VIDEO_ASSET_FLAG_INDEX1;
    const uint32_t packed_row_size = ((uint32_t)video->width + 7u) / 8u;
    const uint32_t palette_size = uses_index8 ? 512u : (uses_index1 ? 4u : 0u);
    const uint32_t offset_table_size = uses_rle ? video->frame_count * 4u : 0u;
    const uint32_t payload_frame_size = uses_index8 ? (uint32_t)video->width * video->height
                                      : uses_index1 ? packed_row_size * video->height
                                                    : expected_frame_size;
    const uint32_t max_plain_row_bytes = uses_index1 ? packed_row_size
                                       : uses_index8 ? (uint32_t)video->width
                                                     : (uint32_t)video->width * 2u;
    video->data_offset = VIDEO_ASSET_HEADER_SIZE + palette_size + offset_table_size;
    if (video->width == 0 || video->height == 0 || video->fps == 0 ||
        video->frame_count == 0 || video->frame_size != expected_frame_size ||
        (video->flags & (uint8_t)~supported_flags) != 0u ||
        (uses_index8 && uses_index1) ||
        (!uses_rle && video->data_size != payload_frame_size * video->frame_count) ||
        (max_plain_row_bytes + (max_plain_row_bytes + 127u) / 128u >
         VIDEO_ASSET_MAX_DECODE_ROW_BYTES) ||
        (uint32_t)file_size < video->data_offset + video->data_size) {
        Storage_Lock();
        lfs_file_close(lfs, &video->file);
        Storage_Unlock();
        memset(video, 0, sizeof(*video));
        return 0;
    }
    if (uses_index8) {
        uint8_t palette_bytes[512];
        if (!read_exact(video, VIDEO_ASSET_HEADER_SIZE, palette_bytes, sizeof(palette_bytes))) {
            Storage_Lock();
            lfs_file_close(lfs, &video->file);
            Storage_Unlock();
            memset(video, 0, sizeof(*video));
            return 0;
        }
        for (uint16_t i = 0; i < 256u; i++) {
            video->palette[i] = read_u16_le(&palette_bytes[i * 2u]);
        }
    } else if (uses_index1) {
        uint8_t palette_bytes[4];
        if (!read_exact(video, VIDEO_ASSET_HEADER_SIZE, palette_bytes, sizeof(palette_bytes))) {
            Storage_Lock();
            lfs_file_close(lfs, &video->file);
            Storage_Unlock();
            memset(video, 0, sizeof(*video));
            return 0;
        }
        video->palette[0] = read_u16_le(&palette_bytes[0]);
        video->palette[1] = read_u16_le(&palette_bytes[2]);
    }

    video->is_open = 1;
    video->next_read_offset = video->data_offset;
    video->rle_next_row_offset = video->data_offset;
    video->rle_current_frame = UINT32_MAX;
    video->rle_next_row = 0;
    video->cached_frame_index = UINT32_MAX;
    video->cached_frame_size = 0;
    video->cached_frame_valid = 0;
    return 1;
#else
    (void)path;
    memset(video, 0, sizeof(*video));
    return 0;
#endif
}

void Video_Asset_Close(Video_asset* video) {
    if (video == NULL || !video->is_open) { return; }
#if FRAMEWORK_USE_LFS
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs != NULL) {
        Storage_Lock();
        lfs_file_close(lfs, &video->file);
        Storage_Unlock();
    }
#endif
    memset(video, 0, sizeof(*video));
}

uint8_t Video_Asset_Read_Frame_Span(Video_asset* video, uint32_t frame_index,
    uint16_t y, uint16_t x, uint16_t width, uint16_t* pixels) {
    if (video == NULL || pixels == NULL || !video->is_open || width == 0 ||
        frame_index >= video->frame_count || y >= video->height ||
        x >= video->width || x + width > video->width) {
        return 0;
    }

#if FRAMEWORK_USE_LFS
    if ((video->flags & VIDEO_ASSET_FLAG_RLE) != 0u) {
        if (x != 0 || width != video->width) { return 0; }
        return Video_Asset_Read_Frame_Row(video, frame_index, y, pixels);
    }
    if ((video->flags & VIDEO_ASSET_FLAG_INDEX8) != 0u) {
        if (width > sizeof(g_rle_row_buffer)) { return 0; }
        const uint32_t index_offset =
            frame_index * ((uint32_t)video->width * video->height) +
            (uint32_t)y * video->width + x;
        if (!read_exact(video, video->data_offset + index_offset, g_rle_row_buffer, width)) {
            return 0;
        }
        for (uint16_t i = 0; i < width; i++) { pixels[i] = video->palette[g_rle_row_buffer[i]]; }
        return 1;
    } else if ((video->flags & VIDEO_ASSET_FLAG_INDEX1) != 0u) {
        if (x != 0 || width != video->width) { return 0; }
        const uint32_t packed_row_size = ((uint32_t)video->width + 7u) / 8u;
        if (packed_row_size > sizeof(g_index1_row_buffer)) { return 0; }
        const uint32_t packed_frame_size = packed_row_size * video->height;
        const uint32_t packed_offset = frame_index * packed_frame_size + (uint32_t)y * packed_row_size;
        if (!read_exact(video, video->data_offset + packed_offset, g_index1_row_buffer, packed_row_size)) {
            return 0;
        }
        expand_index1_row(g_index1_row_buffer, video->width, video->palette, pixels);
        return 1;
    } else {
        const uint32_t pixel_offset =
            frame_index * video->frame_size + ((uint32_t)y * video->width + x) * 2u;
        const uint32_t byte_count = (uint32_t)width * 2u;
        return read_exact(video, video->data_offset + pixel_offset, pixels, byte_count);
    }
#else
    (void)frame_index;
    (void)y;
    (void)x;
    (void)width;
    return 0;
#endif
}

uint8_t Video_Asset_Read_Frame_Row(
    Video_asset* video, uint32_t frame_index, uint16_t y, uint16_t* pixels) {
    if (video == NULL || pixels == NULL || !video->is_open ||
        frame_index >= video->frame_count || y >= video->height) {
        return 0;
    }

#if FRAMEWORK_USE_LFS
    if ((video->flags & VIDEO_ASSET_FLAG_RLE) == 0u) {
        return Video_Asset_Read_Frame_Span(video, frame_index, y, 0, video->width, pixels);
    }

    if (load_frame_cache(video, frame_index)) {
        return read_cached_rle_row(video, y, pixels);
    }

    uint32_t offset = 0;
    uint16_t first_scan_row = 0;
    if (video->rle_current_frame == frame_index && video->rle_next_row <= y) {
        offset = video->rle_next_row_offset;
        first_scan_row = video->rle_next_row;
    } else {
        if (!read_frame_data_offset(video, frame_index, &offset)) { return 0; }
        video->rle_current_frame = frame_index;
        video->rle_next_row = 0;
        video->rle_next_row_offset = offset;
    }

    for (uint16_t row = first_scan_row; row < y; row++) {
        uint8_t length_bytes[2];
        if (!read_exact(video, offset, length_bytes, sizeof(length_bytes))) { return 0; }
        const uint16_t encoded_len = read_u16_le(length_bytes);
        offset += 2u + encoded_len;
        if (offset > video->data_offset + video->data_size) { return 0; }
    }

    uint8_t length_bytes[2];
    if (!read_exact(video, offset, length_bytes, sizeof(length_bytes))) { return 0; }
    const uint16_t encoded_len = read_u16_le(length_bytes);
    offset += 2u;
    if (encoded_len == 0 || encoded_len > sizeof(g_rle_row_buffer) ||
        offset + encoded_len > video->data_offset + video->data_size ||
        !read_exact(video, offset, g_rle_row_buffer, encoded_len)) {
        return 0;
    }
    video->rle_current_frame = frame_index;
    video->rle_next_row = (uint16_t)(y + 1u);
    video->rle_next_row_offset = offset + encoded_len;
    return decode_rle_encoded_row(video, encoded_len, pixels);
#else
    (void)frame_index;
    (void)y;
    return 0;
#endif
}
