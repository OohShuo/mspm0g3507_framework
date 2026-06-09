/**
 * @file   lfs_port.c
 * @brief  littlefs block-device callbacks that talk to a W25Q32 driver.
 *
 * Why this is an app and not a hal:
 *   The w25q32 hal implements the *raw* flash protocol (JEDEC ID, read,
 *   page program, sector erase, busy wait). Putting littlefs on top of
 *   it is a deliberate policy choice — another project might want FAT,
 *   or a custom key-value store, or nothing at all. Keeping lfs in app/
 *   means swapping filesystems doesn't drag the hal around.
 *
 * Concurrency model:
 *   The w25q32 driver is single-owner (one SPI bus = one task at a time
 *   for the bus). lfs itself is non-reentrant. So this port assumes a
 *   single task drives the Lfs_Port and that task also owns the SPI
 *   bus for the duration. If you need multi-task access, wrap the
 *   callbacks below in a mutex — don't try to make lfs itself
 *   thread-safe via the lock/unlock fields (we don't define
 *   LFS_THREADSAFE here).
 */

#include "lfs_port.h"

#include <stddef.h>
#include <string.h>

#include "board_config.h" /* for lfs_port sizing */

/* ------------------------------------------------------------------ */
/* Buffer size tuning (must match the array sizes in Lfs_Port)         */
/* ------------------------------------------------------------------ */

#define LFS_PORT_READ_SIZE   256   /* smallest read granularity */
#define LFS_PORT_PROG_SIZE   256   /* W25Q32 page size          */
#define LFS_PORT_BLOCK_SIZE  4096  /* W25Q32 sector size        */
#define LFS_PORT_CACHE_SIZE  256   /* lfs read+prog cache       */
#define LFS_PORT_LOOKAHEAD   16    /* bytes; tracks 128 blocks  */
#define LFS_PORT_BLOCK_CYCLES 100  /* lfs metadata rotate freq  */

/* Sanity: size must be a non-zero multiple of the block size. */
#define LFS_PORT_MIN_SIZE    LFS_PORT_BLOCK_SIZE

/* ------------------------------------------------------------------ */
/* Block device callbacks                                              */
/* ------------------------------------------------------------------ */

static int bd_read(const struct lfs_config* c, lfs_block_t block,
                   lfs_off_t off, void* buffer, lfs_size_t size) {
    const Lfs_port* self = (const Lfs_port*)c->context;
    uint32_t addr = self->port.start + block * LFS_PORT_BLOCK_SIZE + off;
    W25q32_Read(self->port.flash, addr, (uint8_t*)buffer, size);
    return 0;
}

static int bd_prog(const struct lfs_config* c, lfs_block_t block,
                   lfs_off_t off, const void* buffer, lfs_size_t size) {
    const Lfs_port* self = (const Lfs_port*)c->context;
    uint32_t addr = self->port.start + block * LFS_PORT_BLOCK_SIZE + off;

    /* lfs splits progs at prog_size boundaries, so this is normally a
     * single W25Q32_Page_Program call. We still defend against the
     * unusual case where the block-relative offset is not page-aligned
     * (it shouldn't be, but W25Q32 will silently wrap if it ever is). */
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
    /* W25Q32_Page_Program and W25Q32_Sector_Erase both wait for SR1.BUSY
     * to clear before returning, so by the time prog/erase return the
     * write has hit the silicon. sync is effectively a no-op. */
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

Lfs_port* Lfs_Port_Create(const Lfs_port_config* cfg) {
    if (cfg == NULL || cfg->flash == NULL) { return NULL; }
    if (cfg->size == 0 || (cfg->size % LFS_PORT_MIN_SIZE) != 0) { return NULL; }

    static Lfs_port g_port;          /* single-instance; lfs is one lfs_t per mount */
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
