#if FRAMEWORK_USE_LFS

// clang-format off

#include "lfs_port.h"

#include <stddef.h>
#include <string.h>

#include "board_config.h"

/* ------------------------------------------------------------------ */
/* Block-device geometry                                               */
/*                                                                     */
/* W25Q32 page size is 256 B; sector size (smallest erasable unit) is */
/* 4 KiB.  These values are chosen to match the flash physics.         */
/* ------------------------------------------------------------------ */

#define LFS_PORT_READ_SIZE    256
#define LFS_PORT_PROG_SIZE    256
#define LFS_PORT_BLOCK_SIZE   4096
#define LFS_PORT_CACHE_SIZE   256
#define LFS_PORT_LOOKAHEAD    16
#define LFS_PORT_BLOCK_CYCLES 100

#define LFS_PORT_MIN_SIZE     LFS_PORT_BLOCK_SIZE

/* ------------------------------------------------------------------ */
/* Block-device callbacks                                              */
/* ------------------------------------------------------------------ */

static int bd_read(
    const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
    void* buffer, lfs_size_t size);
static int bd_prog(
    const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
    const void* buffer, lfs_size_t size);
static int bd_erase(const struct lfs_config* c, lfs_block_t block);
static int bd_sync(const struct lfs_config* c);

/* ------------------------------------------------------------------ */
/* Mutex helper                                                        */
/* ------------------------------------------------------------------ */

static inline void mutex_take(SemaphoreHandle_t m) {
    if (m != NULL) { xSemaphoreTake(m, portMAX_DELAY); }
}

static inline void mutex_give(SemaphoreHandle_t m) {
    if (m != NULL) { xSemaphoreGive(m); }
}

/* ------------------------------------------------------------------ */
/* Singleton instance                                                  */
/* ------------------------------------------------------------------ */

static Lfs_port g_port;
static uint8_t g_port_inited = 0;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

Lfs_port* Lfs_Port_Create(const Lfs_port_config* cfg) {
    if (cfg == NULL || cfg->flash == NULL) { return NULL; }
    if (cfg->size == 0 || (cfg->size % LFS_PORT_MIN_SIZE) != 0) { return NULL; }
    if (g_port_inited) { return NULL; }

    memset(&g_port, 0, sizeof(g_port));
    g_port.port = *cfg;

    g_port.cfg.context  = &g_port;
    g_port.cfg.read     = bd_read;
    g_port.cfg.prog     = bd_prog;
    g_port.cfg.erase    = bd_erase;
    g_port.cfg.sync     = bd_sync;

    g_port.cfg.read_size      = LFS_PORT_READ_SIZE;
    g_port.cfg.prog_size      = LFS_PORT_PROG_SIZE;
    g_port.cfg.block_size     = LFS_PORT_BLOCK_SIZE;
    g_port.cfg.block_count    = cfg->size / LFS_PORT_BLOCK_SIZE;
    g_port.cfg.block_cycles   = LFS_PORT_BLOCK_CYCLES;
    g_port.cfg.cache_size     = LFS_PORT_CACHE_SIZE;
    g_port.cfg.lookahead_size = LFS_PORT_LOOKAHEAD;

    g_port.cfg.read_buffer      = g_port.read_buffer;
    g_port.cfg.prog_buffer      = g_port.prog_buffer;
    g_port.cfg.lookahead_buffer = g_port.lookahead_buffer;

    g_port_inited = 1;
    return &g_port;
}

int Lfs_Port_Format(Lfs_port* obj) {
    if (obj == NULL) { return LFS_ERR_IO; }
    mutex_take(obj->port.spi_mutex);
    int err = lfs_format(&obj->lfs, &obj->cfg);
    mutex_give(obj->port.spi_mutex);
    return err;
}

int Lfs_Port_Mount(Lfs_port* obj) {
    if (obj == NULL) { return LFS_ERR_IO; }
    mutex_take(obj->port.spi_mutex);
    int err = lfs_mount(&obj->lfs, &obj->cfg);
    mutex_give(obj->port.spi_mutex);
    return err;
}

int Lfs_Port_Unmount(Lfs_port* obj) {
    if (obj == NULL) { return LFS_ERR_IO; }
    mutex_take(obj->port.spi_mutex);
    int err = lfs_unmount(&obj->lfs);
    mutex_give(obj->port.spi_mutex);
    return err;
}

lfs_t* Lfs_Port_Get_Lfs(Lfs_port* obj) { return (obj != NULL) ? &obj->lfs : NULL; }

/* ------------------------------------------------------------------ */
/* Block-device ops                                                    */
/* ------------------------------------------------------------------ */

static int bd_read(
    const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
    void* buffer, lfs_size_t size) {
    const Lfs_port* self = (const Lfs_port*)c->context;
    uint32_t addr = self->port.start + block * LFS_PORT_BLOCK_SIZE + off;

    W25q32_Read(self->port.flash, addr, (uint8_t*)buffer, size);

    return 0;
}

static int bd_prog(
    const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
    const void* buffer, lfs_size_t size) {
    const Lfs_port* self = (const Lfs_port*)c->context;
    uint32_t addr = self->port.start + block * LFS_PORT_BLOCK_SIZE + off;

    while (size > 0) {
        uint32_t page_off = addr & (LFS_PORT_PROG_SIZE - 1);
        uint32_t chunk    = LFS_PORT_PROG_SIZE - page_off;
        if (chunk > size) { chunk = size; }
        W25q32_Page_Program(self->port.flash, addr, (const uint8_t*)buffer, chunk);
        addr   += chunk;
        buffer  = (const uint8_t*)buffer + chunk;
        size   -= chunk;
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
