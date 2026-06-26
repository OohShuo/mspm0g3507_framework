#pragma once

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Type definitions matching littlefs ── */

typedef int32_t  lfs_soff_t;
typedef int32_t  lfs_ssize_t;
typedef uint32_t lfs_size_t;
typedef uint32_t lfs_off_t;
typedef uint32_t lfs_block_t;

enum lfs_open_flags {
    LFS_O_RDONLY  = 1,
    LFS_O_WRONLY  = 2,
    LFS_O_RDWR    = 3,
    LFS_O_CREAT   = 0x010,
    LFS_O_EXCL    = 0x020,
    LFS_O_TRUNC   = 0x040,
    LFS_O_APPEND  = 0x080,
};

enum lfs_seek_flags {
    LFS_SEEK_SET = 0,
    LFS_SEEK_CUR = 1,
    LFS_SEEK_END = 2,
};

enum lfs_type {
    LFS_TYPE_REG = 0x001,
    LFS_TYPE_DIR = 0x002,
};

struct lfs_info {
    uint8_t     type;
    lfs_size_t  size;
    char        name[256];
};

struct lfs_config;

typedef struct {
    char  root[512];
    uint8_t mounted;
} lfs_t;

typedef struct {
    FILE*  fh;
    char   path[512];
    int    flags;
} lfs_file_t;

typedef struct {
    void*  handle;     /* platform-specific directory iterator */
    char   path[512];
    struct lfs_info current;
} lfs_dir_t;

/* ── Filesystem ── */

int lfs_mount(lfs_t* lfs, const struct lfs_config* cfg);
int lfs_unmount(lfs_t* lfs);
int lfs_format(lfs_t* lfs, const struct lfs_config* cfg);
int lfs_fs_size(lfs_t* lfs);
int lfs_remove(lfs_t* lfs, const char* path);
int lfs_stat(lfs_t* lfs, const char* path, struct lfs_info* info);
int lfs_getattr(lfs_t* lfs, const char* path, uint8_t type, void* buf, lfs_size_t size);

/* ── File I/O ── */

int lfs_file_open(lfs_t* lfs, lfs_file_t* file, const char* path, int flags);
lfs_ssize_t lfs_file_read(lfs_t* lfs, lfs_file_t* file, void* buf, lfs_size_t size);
lfs_ssize_t lfs_file_write(lfs_t* lfs, lfs_file_t* file, const void* buf, lfs_size_t size);
lfs_soff_t lfs_file_seek(lfs_t* lfs, lfs_file_t* file, lfs_soff_t off, int whence);
lfs_soff_t lfs_file_size(lfs_t* lfs, lfs_file_t* file);
lfs_soff_t lfs_file_tell(lfs_t* lfs, lfs_file_t* file);
int lfs_file_truncate(lfs_t* lfs, lfs_file_t* file, lfs_off_t size);
int lfs_file_sync(lfs_t* lfs, lfs_file_t* file);
int lfs_file_close(lfs_t* lfs, lfs_file_t* file);

/* ── Directory ── */

int lfs_dir_open(lfs_t* lfs, lfs_dir_t* dir, const char* path);
int lfs_dir_read(lfs_t* lfs, lfs_dir_t* dir, struct lfs_info* info);
int lfs_dir_close(lfs_t* lfs, lfs_dir_t* dir);

#ifdef __cplusplus
}
#endif
