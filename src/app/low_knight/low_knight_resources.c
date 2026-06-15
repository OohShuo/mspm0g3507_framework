#include "low_knight_resources.h"

#include <stddef.h>
#include <string.h>

#include "storage.h"

#define LOW_KNIGHT_RESOURCE_MAGIC       0x52504b4cu
#define LOW_KNIGHT_RESOURCE_VERSION     1u
#define LOW_KNIGHT_RESOURCE_HEADER_SIZE 24u

static uint16_t read_u16_le(const uint8_t* data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_u32_le(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint8_t read_region(
    Low_Knight_Resources* resources, uint32_t region_offset, uint32_t region_size,
    uint32_t offset, void* data, uint32_t size) {
    if (resources == NULL || data == NULL || !resources->is_open || size == 0 ||
        offset >= region_size || size > region_size - offset) {
        return 0;
    }

#if FRAMEWORK_USE_LFS
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    Storage_Lock();
    const lfs_soff_t seek_result =
        lfs_file_seek(lfs, &resources->file, (lfs_soff_t)(region_offset + offset), LFS_SEEK_SET);
    const lfs_ssize_t read_result =
        seek_result < 0 ? seek_result : lfs_file_read(lfs, &resources->file, data, size);
    Storage_Unlock();
    return read_result == (lfs_ssize_t)size;
#else
    (void)region_offset;
    return 0;
#endif
}

uint8_t Low_Knight_Resources_Open(Low_Knight_Resources* resources, const char* path) {
    if (resources == NULL || path == NULL) { return 0; }
    Low_Knight_Resources_Close(resources);

#if FRAMEWORK_USE_LFS
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    uint8_t header[LOW_KNIGHT_RESOURCE_HEADER_SIZE];
    Storage_Lock();
    int result = lfs_file_open(lfs, &resources->file, path, LFS_O_RDONLY);
    if (result == 0) { result = (int)lfs_file_read(lfs, &resources->file, header, sizeof(header)); }
    Storage_Unlock();

    if (result != (int)sizeof(header) ||
        read_u32_le(header) != LOW_KNIGHT_RESOURCE_MAGIC ||
        header[4] != LOW_KNIGHT_RESOURCE_VERSION) {
        if (result >= 0) {
            Storage_Lock();
            lfs_file_close(lfs, &resources->file);
            Storage_Unlock();
        }
        memset(resources, 0, sizeof(*resources));
        return 0;
    }

    resources->crc16 = read_u16_le(header + 6);
    resources->gfx_size = read_u32_le(header + 8);
    resources->gff_size = read_u32_le(header + 12);
    resources->map_size = read_u32_le(header + 16);
    if (resources->gfx_size != LOW_KNIGHT_GFX_SIZE ||
        resources->gff_size != LOW_KNIGHT_GFF_SIZE ||
        resources->map_size != LOW_KNIGHT_MAP_SIZE) {
        Storage_Lock();
        lfs_file_close(lfs, &resources->file);
        Storage_Unlock();
        memset(resources, 0, sizeof(*resources));
        return 0;
    }

    resources->gfx_offset = LOW_KNIGHT_RESOURCE_HEADER_SIZE;
    resources->gff_offset = resources->gfx_offset + resources->gfx_size;
    resources->map_offset = resources->gff_offset + resources->gff_size;
    resources->is_open = 1;
    return 1;
#else
    (void)path;
    memset(resources, 0, sizeof(*resources));
    return 0;
#endif
}

void Low_Knight_Resources_Close(Low_Knight_Resources* resources) {
    if (resources == NULL || !resources->is_open) { return; }
#if FRAMEWORK_USE_LFS
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs != NULL) {
        Storage_Lock();
        lfs_file_close(lfs, &resources->file);
        Storage_Unlock();
    }
#endif
    memset(resources, 0, sizeof(*resources));
}

uint8_t Low_Knight_Resources_Read_Gfx(
    Low_Knight_Resources* resources, uint32_t offset, void* data, uint32_t size) {
    return read_region(resources, resources->gfx_offset, resources->gfx_size, offset, data, size);
}

uint8_t Low_Knight_Resources_Read_Map(
    Low_Knight_Resources* resources, uint32_t offset, void* data, uint32_t size) {
    return read_region(resources, resources->map_offset, resources->map_size, offset, data, size);
}

uint8_t Low_Knight_Resources_Read_Gff(
    Low_Knight_Resources* resources, uint32_t offset, void* data, uint32_t size) {
    return read_region(resources, resources->gff_offset, resources->gff_size, offset, data, size);
}
