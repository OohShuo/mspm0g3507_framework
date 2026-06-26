#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "lfs.h"

#ifdef _WIN32
    #include <direct.h>
    #define mkdir_p(path) _mkdir(path)
    #include <windows.h>
typedef HANDLE dir_handle_t;
#else
    #include <dirent.h>
    #include <unistd.h>
    #define mkdir_p(path) mkdir(path, 0755)
typedef DIR* dir_handle_t;
#endif

#define LFS_ERR_OK      0
#define LFS_ERR_IO      -5
#define LFS_ERR_CORRUPT -84
#define LFS_ERR_NOENT   -2
#define LFS_ERR_EXIST   -17
#define LFS_ERR_INVAL   -22
#define LFS_ERR_NOSPC   -28
#define LFS_ERR_NOMEM   -12

/* ── Resolve the assets/vm_flash root directory ── */
static const char* find_vm_root(void) {
    static char root[LFS_VM_PATH_MAX];
    static int found = 0;
    if (found) { return root; }

    static const char* search[] = {
        "assets/vm_flash",
        "../assets/vm_flash",
        "../../assets/vm_flash",
    };
    for (int i = 0; i < (int)(sizeof(search) / sizeof(search[0])); i++) {
#ifdef _WIN32
        DWORD attr = GetFileAttributesA(search[i]);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
#else
        DIR* d = opendir(search[i]);
        if (d != NULL) {
            closedir(d);
#endif
            snprintf(root, sizeof(root), "%s", search[i]);
            found = 1;
            return root;
        }
    }
    /* Fallback */
    snprintf(root, sizeof(root), "assets/vm_flash");
    found = 1;
    return root;
}

static void make_host_path(const lfs_t* lfs, const char* path, char* out, size_t out_size) {
    const char* root = (lfs != NULL && lfs->root[0] != '\0') ? lfs->root : find_vm_root();
    if (path[0] == '/') { path++; }
    size_t rlen = strlen(root);
    size_t plen = strlen(path);
    if (rlen >= out_size) { rlen = out_size - 1; }
    memcpy(out, root, rlen);
    out[rlen] = '/';
    size_t remain = out_size - rlen - 1;
    if (plen >= remain) { plen = remain > 0 ? remain - 1 : 0; }
    if (plen > 0) { memcpy(out + rlen + 1, path, plen); }
    out[rlen + 1 + plen] = '\0';
}

static int host_file_exists(const char* host_path) {
    FILE* f = fopen(host_path, "rb");
    if (f != NULL) {
        fclose(f);
        return 1;
    }
    return 0;
}

static void host_mkdirs(const char* host_path) {
    char tmp[LFS_VM_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", host_path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir_p(tmp);
            *p = '/';
        }
    }
}

/* ── Filesystem operations ── */

int lfs_mount(lfs_t* lfs, const struct lfs_config* cfg) {
    (void)cfg;
    if (lfs == NULL) { return LFS_ERR_INVAL; }
    if (lfs->mounted) { return LFS_ERR_OK; }
    const char* root = find_vm_root();
    snprintf(lfs->root, sizeof(lfs->root), "%s", root);
    lfs->mounted = 1;
    return LFS_ERR_OK;
}

int lfs_unmount(lfs_t* lfs) {
    if (lfs == NULL) { return LFS_ERR_INVAL; }
    lfs->mounted = 0;
    return LFS_ERR_OK;
}

int lfs_format(lfs_t* lfs, const struct lfs_config* cfg) {
    (void)cfg;
    if (lfs == NULL) { return LFS_ERR_INVAL; }
    const char* root = find_vm_root();
    mkdir_p(root);
    snprintf(lfs->root, sizeof(lfs->root), "%s", root);
    return LFS_ERR_OK;
}

int lfs_fs_size(lfs_t* lfs) {
    (void)lfs;
    return 3 * 1024 * 1024; /* dummy: 3 MiB */
}

int lfs_remove(lfs_t* lfs, const char* path) {
    if (lfs == NULL || path == NULL) { return LFS_ERR_INVAL; }
    char host_path[800];
    make_host_path(lfs, path, host_path, sizeof(host_path));
    return remove(host_path) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

int lfs_stat(lfs_t* lfs, const char* path, struct lfs_info* info) {
    if (lfs == NULL || path == NULL || info == NULL) { return LFS_ERR_INVAL; }
    char host_path[800];
    make_host_path(lfs, path, host_path, sizeof(host_path));

    FILE* f = fopen(host_path, "rb");
    if (f != NULL) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fclose(f);
        memset(info, 0, sizeof(*info));
        info->type = LFS_TYPE_REG;
        info->size = (lfs_size_t)sz;
        const char* name = strrchr(path, '/');
        snprintf(info->name, sizeof(info->name), "%s", name ? name + 1 : path);
        return LFS_ERR_OK;
    }
    return LFS_ERR_NOENT;
}

int lfs_getattr(lfs_t* lfs, const char* path, uint8_t type, void* buf, lfs_size_t size) {
    (void)lfs;
    (void)path;
    (void)type;
    (void)buf;
    (void)size;
    return LFS_ERR_INVAL;
}

/* ── File operations ── */

int lfs_file_open(lfs_t* lfs, lfs_file_t* file, const char* path, int flags) {
    if (lfs == NULL || file == NULL || path == NULL) { return LFS_ERR_INVAL; }
    memset(file, 0, sizeof(*file));

    char host_path[800];
    make_host_path(lfs, path, host_path, sizeof(host_path));
    snprintf(file->path, sizeof(file->path), "%s", host_path);
    file->flags = flags;

    int exists = host_file_exists(host_path);

    /* Handle O_CREAT | O_EXCL */
    if ((flags & LFS_O_CREAT) && (flags & LFS_O_EXCL) && exists) { return LFS_ERR_EXIST; }

    /* Determine mode */
    const char* mode;
    if ((flags & LFS_O_CREAT) && !exists) {
        host_mkdirs(host_path);
        mode = "w+b";
    } else if ((flags & LFS_O_RDWR) == LFS_O_RDWR) {
        mode = "r+b";
    } else if ((flags & LFS_O_WRONLY) == LFS_O_WRONLY) {
        mode = exists ? "r+b" : "w+b";
    } else {
        mode = "rb";
    }

    file->fh = fopen(host_path, mode);
    if (file->fh == NULL) { return LFS_ERR_IO; }

    /* Handle O_TRUNC */
    if ((flags & LFS_O_TRUNC) && (flags & (LFS_O_WRONLY | LFS_O_RDWR))) {
        /* Truncate by closing and reopening in write mode */
        fclose(file->fh);
        file->fh = fopen(host_path, "w+b");
        if (file->fh == NULL) { return LFS_ERR_IO; }
    }

    /* Handle O_APPEND */
    if (flags & LFS_O_APPEND) { fseek(file->fh, 0, SEEK_END); }

    return LFS_ERR_OK;
}

lfs_ssize_t lfs_file_read(lfs_t* lfs, lfs_file_t* file, void* buf, lfs_size_t size) {
    (void)lfs;
    if (file == NULL || file->fh == NULL || buf == NULL) { return LFS_ERR_IO; }
    size_t n = fread(buf, 1, size, file->fh);
    return (lfs_ssize_t)n;
}

lfs_ssize_t lfs_file_write(lfs_t* lfs, lfs_file_t* file, const void* buf, lfs_size_t size) {
    (void)lfs;
    if (file == NULL || file->fh == NULL || buf == NULL) { return LFS_ERR_IO; }
    size_t n = fwrite(buf, 1, size, file->fh);
    return (lfs_ssize_t)n;
}

lfs_soff_t lfs_file_seek(lfs_t* lfs, lfs_file_t* file, lfs_soff_t off, int whence) {
    (void)lfs;
    if (file == NULL || file->fh == NULL) { return LFS_ERR_IO; }
    int w = SEEK_SET;
    if (whence == LFS_SEEK_CUR) {
        w = SEEK_CUR;
    } else if (whence == LFS_SEEK_END) {
        w = SEEK_END;
    }
    if (fseek(file->fh, (long)off, w) != 0) { return LFS_ERR_IO; }
    long pos = ftell(file->fh);
    return (lfs_soff_t)pos;
}

lfs_soff_t lfs_file_size(lfs_t* lfs, lfs_file_t* file) {
    (void)lfs;
    if (file == NULL || file->fh == NULL) { return LFS_ERR_IO; }
    long cur = ftell(file->fh);
    fseek(file->fh, 0, SEEK_END);
    long sz = ftell(file->fh);
    fseek(file->fh, cur, SEEK_SET);
    return (lfs_soff_t)sz;
}

lfs_soff_t lfs_file_tell(lfs_t* lfs, lfs_file_t* file) {
    (void)lfs;
    if (file == NULL || file->fh == NULL) { return LFS_ERR_IO; }
    long pos = ftell(file->fh);
    return (lfs_soff_t)pos;
}

int lfs_file_truncate(lfs_t* lfs, lfs_file_t* file, lfs_off_t size) {
    (void)lfs;
    if (file == NULL || file->fh == NULL) { return LFS_ERR_IO; }
#ifdef _WIN32
    return _chsize_s(_fileno(file->fh), (long)size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
#else
    return ftruncate(fileno(file->fh), (off_t)size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
#endif
}

int lfs_file_sync(lfs_t* lfs, lfs_file_t* file) {
    (void)lfs;
    if (file == NULL || file->fh == NULL) { return LFS_ERR_IO; }
    fflush(file->fh);
#ifdef _WIN32
    _commit(_fileno(file->fh));
#else
    fsync(fileno(file->fh));
#endif
    return LFS_ERR_OK;
}

int lfs_file_close(lfs_t* lfs, lfs_file_t* file) {
    (void)lfs;
    if (file == NULL || file->fh == NULL) { return LFS_ERR_IO; }
    fclose(file->fh);
    memset(file, 0, sizeof(*file));
    return LFS_ERR_OK;
}

/* ── Directory operations ── */

int lfs_dir_open(lfs_t* lfs, lfs_dir_t* dir, const char* path) {
    if (lfs == NULL || dir == NULL || path == NULL) { return LFS_ERR_INVAL; }
    memset(dir, 0, sizeof(*dir));

    char host_path[800];
    make_host_path(lfs, path, host_path, sizeof(host_path));

#ifdef _WIN32
    char search_path[820];
    snprintf(search_path, sizeof(search_path), "%s/*", host_path);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search_path, &fd);
    if (h == INVALID_HANDLE_VALUE) { return LFS_ERR_IO; }
    dir->handle = (void*)h;
    snprintf(dir->path, sizeof(dir->path), "%s", host_path);
    /* Store first result */
    if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
        dir->current.type = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? LFS_TYPE_DIR : LFS_TYPE_REG;
        dir->current.size = (lfs_size_t)fd.nFileSizeLow;
        snprintf(dir->current.name, sizeof(dir->current.name), "%s", fd.cFileName);
    }
#else
    DIR* d = opendir(host_path);
    if (d == NULL) { return LFS_ERR_IO; }
    dir->handle = d;
    snprintf(dir->path, sizeof(dir->path), "%s", host_path);
#endif
    return LFS_ERR_OK;
}

int lfs_dir_read(lfs_t* lfs, lfs_dir_t* dir, struct lfs_info* info) {
    (void)lfs;
    if (dir == NULL || info == NULL) { return LFS_ERR_IO; }
    memset(info, 0, sizeof(*info));

#ifdef _WIN32
    for (;;) {
        if (dir->current.name[0] != '\0') {
            memcpy(info, &dir->current, sizeof(*info));
            memset(&dir->current, 0, sizeof(dir->current));
        } else {
            WIN32_FIND_DATAA fd;
            if (!FindNextFileA((HANDLE)dir->handle, &fd)) { return 0; }
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) { continue; }
            info->type = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? LFS_TYPE_DIR : LFS_TYPE_REG;
            info->size = (lfs_size_t)fd.nFileSizeLow;
            snprintf(info->name, sizeof(info->name), "%s", fd.cFileName);
        }
        /* Skip .bin flash images */
        size_t nlen = strlen(info->name);
        if (nlen > 4 && strcmp(info->name + nlen - 4, ".bin") == 0) { continue; }
        return 1;
    }
#else
    struct dirent* ent;
    do {
        ent = readdir((DIR*)dir->handle);
        if (ent == NULL) { return 0; }
    } while (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0);

    char host_path[1100];
    snprintf(host_path, sizeof(host_path), "%s/%s", dir->path, ent->d_name);

    struct stat st;
    if (stat(host_path, &st) == 0) {
        info->type = S_ISDIR(st.st_mode) ? LFS_TYPE_DIR : LFS_TYPE_REG;
        info->size = (lfs_size_t)st.st_size;
    } else {
        info->type = LFS_TYPE_REG;
        info->size = 0;
    }
    snprintf(info->name, sizeof(info->name), "%s", ent->d_name);

    /* Skip .bin flash images */
    size_t nlen = strlen(info->name);
    if (nlen > 4 && strcmp(info->name + nlen - 4, ".bin") == 0) {
        return lfs_dir_read(lfs, dir, info); /* recurse to get next entry */
    }
    return 1;
#endif
}

int lfs_dir_close(lfs_t* lfs, lfs_dir_t* dir) {
    (void)lfs;
    if (dir == NULL) { return LFS_ERR_INVAL; }
#ifdef _WIN32
    if (dir->handle != NULL) { FindClose((HANDLE)dir->handle); }
#else
    if (dir->handle != NULL) { closedir((DIR*)dir->handle); }
#endif
    memset(dir, 0, sizeof(*dir));
    return LFS_ERR_OK;
}
