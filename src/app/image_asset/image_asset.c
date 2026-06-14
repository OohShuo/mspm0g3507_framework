#include "image_asset.h"

#include <stddef.h>
#include <string.h>

#include "storage.h"

#define IMAGE_ASSET_HEADER_SIZE 16u
#define IMAGE_ASSET_VERSION     1u

#if FRAMEWORK_USE_LFS
static uint16_t read_u16_le(const uint8_t* data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_u32_le(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}
#endif

uint8_t Image_Asset_Open(Image_asset* image, const char* path) {
    if (image == NULL || path == NULL) { return 0; }
    Image_Asset_Close(image);

#if FRAMEWORK_USE_LFS
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    uint8_t header[IMAGE_ASSET_HEADER_SIZE];
    Storage_Lock();
    int result = lfs_file_open(lfs, &image->file, path, LFS_O_RDONLY);
    if (result == 0) {
        result = (int)lfs_file_read(lfs, &image->file, header, sizeof(header));
    }
    const lfs_soff_t file_size =
        result == (int)sizeof(header) ? lfs_file_size(lfs, &image->file) : -1;
    Storage_Unlock();

    if (result != (int)sizeof(header) || file_size < (lfs_soff_t)sizeof(header) ||
        memcmp(header, "R565", 4) != 0 || header[4] != IMAGE_ASSET_VERSION) {
        if (result >= 0) {
            Storage_Lock();
            lfs_file_close(lfs, &image->file);
            Storage_Unlock();
        }
        memset(image, 0, sizeof(*image));
        return 0;
    }

    image->flags = header[5];
    image->width = read_u16_le(header + 6);
    image->height = read_u16_le(header + 8);
    image->pixel_data_size = read_u32_le(header + 12);
    const uint32_t expected_pixels = (uint32_t)image->width * image->height * 2u;
    const uint32_t mask_size = (image->flags & IMAGE_ASSET_FLAG_MASK) != 0
                                   ? (uint32_t)((image->width + 7u) / 8u) * image->height
                                   : 0u;
    if (image->width == 0 || image->height == 0 ||
        image->pixel_data_size != expected_pixels ||
        (uint32_t)file_size < IMAGE_ASSET_HEADER_SIZE + expected_pixels + mask_size) {
        Storage_Lock();
        lfs_file_close(lfs, &image->file);
        Storage_Unlock();
        memset(image, 0, sizeof(*image));
        return 0;
    }

    image->is_open = 1;
    image->next_read_offset = IMAGE_ASSET_HEADER_SIZE;
    return 1;
#else
    (void)path;
    memset(image, 0, sizeof(*image));
    return 0;
#endif
}

void Image_Asset_Close(Image_asset* image) {
    if (image == NULL || !image->is_open) { return; }
#if FRAMEWORK_USE_LFS
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs != NULL) {
        Storage_Lock();
        lfs_file_close(lfs, &image->file);
        Storage_Unlock();
    }
#endif
    memset(image, 0, sizeof(*image));
}

uint8_t Image_Asset_Read_Span(Image_asset* image, uint16_t y, uint16_t x,
    uint16_t width, uint16_t* pixels) {
    if (image == NULL || pixels == NULL || !image->is_open || width == 0 ||
        y >= image->height || x >= image->width || x + width > image->width) {
        return 0;
    }

#if FRAMEWORK_USE_LFS
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    const lfs_soff_t offset =
        IMAGE_ASSET_HEADER_SIZE + ((lfs_soff_t)y * image->width + x) * 2;
    const lfs_size_t byte_count = (lfs_size_t)width * 2u;
    Storage_Lock();
    const lfs_soff_t seek_result =
        image->next_read_offset == (uint32_t)offset
            ? offset
            : lfs_file_seek(lfs, &image->file, offset, LFS_SEEK_SET);
    const lfs_ssize_t read_result =
        seek_result < 0 ? seek_result : lfs_file_read(lfs, &image->file, pixels, byte_count);
    Storage_Unlock();
    image->next_read_offset =
        read_result == (lfs_ssize_t)byte_count ? (uint32_t)(offset + byte_count) : UINT32_MAX;
    return read_result == (lfs_ssize_t)byte_count;
#else
    (void)y;
    (void)x;
    (void)width;
    return 0;
#endif
}
