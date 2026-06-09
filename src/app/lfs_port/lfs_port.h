#pragma once

/**
 * @file   lfs_port.h
 * @brief  Project-wide littlefs block-device port for W25Q32 SPI flash.
 *
 * Lives in src/app/ (not src/hal/) because littlefs is an *application*
 * concern: a filesystem is something you choose to put on a flash chip,
 * not a property of the chip itself. The hal layer stays focused on the
 * raw flash protocol.
 *
 * Threading: this module is meant to be driven from a single task at a
 * time. lfs itself is not reentrant — concurrent file I/O on the same
 * lfs_t is undefined. Cross-task sharing should go through a mutex.
 */

#include <stdbool.h>
#include <stdint.h>

#include "lfs.h"
#include "w25q32.h"

typedef struct {
    W25q32* flash;       /**< Backing flash, must outlive the Lfs_Port. */
    uint32_t start;      /**< First byte of the lfs region (e.g. 0). */
    uint32_t size;       /**< Region size in bytes; must be a multiple
                              of 4096. lfs block_count = size / 4096. */
} Lfs_port_config;

typedef struct {
    lfs_t lfs;
    struct lfs_config cfg;
    Lfs_port_config port;

    /* Statically allocated lfs working buffers. Sized for the typical
     * small-MCU case (4 KiB sectors, 256 B pages, 1 MiB region) — see
     * LFS_PORT_* constants in lfs_port.c for the exact values. */
    uint8_t read_buffer[256];
    uint8_t prog_buffer[256];
    uint8_t lookahead_buffer[16];
} Lfs_port;

/**
 * @brief Construct a port. Does not touch the flash; safe to call before
 *        the chip is initialised as long as you don't mount/format yet.
 */
Lfs_port* Lfs_Port_Create(const Lfs_port_config* cfg);

/**
 * @brief Erase and re-create the lfs superblock. Destroys all data in
 *        the region. Returns 0 on success, negative lfs error code on
 *        failure.
 */
int Lfs_Port_Format(Lfs_port* obj);

/**
 * @brief Mount an existing lfs filesystem. Returns 0 on success or a
 *        negative lfs error code. On LFS_ERR_CORRUPT the caller may
 *        want to fall back to Lfs_Port_Format().
 */
int Lfs_Port_Mount(Lfs_port* obj);

/** Unmount and release lfs state. */
int Lfs_Port_Unmount(Lfs_port* obj);

/** Direct access to the underlying lfs_t for file/dir APIs. */
lfs_t* Lfs_Port_Get_Lfs(Lfs_port* obj);
