#if FRAMEWORK_USE_LFS

// clang-format off

#include "lfs_port.h"

#include <stddef.h>
#include <string.h>

#include "board_config.h"

#define LFS_PORT_READ_SIZE    256
#define LFS_PORT_PROG_SIZE    256
#define LFS_PORT_BLOCK_SIZE   4096
#define LFS_PORT_CACHE_SIZE   256
#define LFS_PORT_LOOKAHEAD    16
#define LFS_PORT_BLOCK_CYCLES 100

#define LFS_PORT_MIN_SIZE     LFS_PORT_BLOCK_SIZE

static int bd_read(
    const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size);
static int bd_prog(
    const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size);
static int bd_erase(const struct lfs_config* c, lfs_block_t block);
static int bd_sync(const struct lfs_config* c);

static Lfs_port g_port;

Lfs_port* Lfs_Port_Create(const Lfs_port_config* cfg) {
    if (cfg == NULL || cfg->flash == NULL) { return NULL; }
    if (cfg->size == 0 || (cfg->size % LFS_PORT_MIN_SIZE) != 0) { return NULL; }

    memset(&g_port, 0, sizeof(g_port));

    g_port.port = *cfg;
    g_port.cfg.context = &g_port;
    g_port.cfg.read = bd_read;
    g_port.cfg.prog = bd_prog;
    g_port.cfg.erase = bd_erase;
    g_port.cfg.sync = bd_sync;

    g_port.cfg.read_size = LFS_PORT_READ_SIZE;
    g_port.cfg.prog_size = LFS_PORT_PROG_SIZE;
    g_port.cfg.block_size = LFS_PORT_BLOCK_SIZE;
    g_port.cfg.block_count = cfg->size / LFS_PORT_BLOCK_SIZE;
    g_port.cfg.block_cycles = LFS_PORT_BLOCK_CYCLES;
    g_port.cfg.cache_size = LFS_PORT_CACHE_SIZE;
    g_port.cfg.lookahead_size = LFS_PORT_LOOKAHEAD;

    g_port.cfg.read_buffer = g_port.read_buffer;
    g_port.cfg.prog_buffer = g_port.prog_buffer;
    g_port.cfg.lookahead_buffer = g_port.lookahead_buffer;

    return &g_port;
}

int Lfs_Port_Format(Lfs_port* obj) { return lfs_format(&obj->lfs, &obj->cfg); }

int Lfs_Port_Mount(Lfs_port* obj) { return lfs_mount(&obj->lfs, &obj->cfg); }

int Lfs_Port_Unmount(Lfs_port* obj) { return lfs_unmount(&obj->lfs); }

lfs_t* Lfs_Port_Get_Lfs(Lfs_port* obj) { return &obj->lfs; }

static int bd_read(
    const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size) {
    const Lfs_port* self = (const Lfs_port*)c->context;
    uint32_t addr = self->port.start + block * LFS_PORT_BLOCK_SIZE + off;
    W25q32_Read(self->port.flash, addr, (uint8_t*)buffer, size);
    return 0;
}

static int bd_prog(
    const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size) {
    const Lfs_port* self = (const Lfs_port*)c->context;
    uint32_t addr = self->port.start + block * LFS_PORT_BLOCK_SIZE + off;

    while (size > 0) {
        uint32_t page_off = addr & (LFS_PORT_PROG_SIZE - 1);
        uint32_t chunk = LFS_PORT_PROG_SIZE - page_off;
        if (chunk > size) { chunk = size; }
        W25q32_Page_Program(self->port.flash, addr, (const uint8_t*)buffer, chunk);
        addr += chunk;
        buffer = (const uint8_t*)buffer + chunk;
        size -= chunk;
    }

    return 0;
}

static int bd_erase(const struct lfs_config* c, lfs_block_t block) {
    const Lfs_port* self = (const Lfs_port*)c->context;
    uint32_t addr = self->port.start + block * LFS_PORT_BLOCK_SIZE;
    W25q32_Sector_Erase(self->port.flash, addr);

    return 0;
}

static int bd_sync(const struct lfs_config* c) {
    (void)c;

    return 0;
}

// clang-format on

#endif
