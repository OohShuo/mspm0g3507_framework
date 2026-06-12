#pragma once

#if FRAMEWORK_USE_LFS

// clang-format off

#include <stdint.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "lfs.h"
#include "w25q32.h"

/**
 * @brief Configuration for the lfs port (block-device binding).
 *
 * The caller must supply an already-initialised W25q32 handle and a
 * mutex that protects the shared SPI bus.  The mutex is optional:
 * pass NULL when the flash is the sole SPI device or the application
 * guarantees no concurrent bus access.
 */
typedef struct {
    W25q32*            flash;
    uint32_t           start;      /* byte offset into the W25Q32 chip     */
    uint32_t           size;       /* region size in bytes (multiple of 4K) */
    SemaphoreHandle_t  spi_mutex;  /* NULL = no mutex protection           */
} Lfs_port_config;

/**
 * @brief lfs_port instance — bundles the LittleFS handle, config, and
 *        the static buffers required by the lfs_config.
 */
typedef struct {
    lfs_t            lfs;
    struct lfs_config cfg;
    Lfs_port_config  port;

    /* lfs_config backing buffers (allocated inside the struct, not on the
     * FreeRTOS heap, so they are available even when the heap is fragmented) */
    uint8_t read_buffer[256];
    uint8_t prog_buffer[256];
    uint8_t lookahead_buffer[16];
} Lfs_port;

/* ---- lifecycle --------------------------------------------------------- */

/**
 * @brief Create and initialise an lfs_port.
 *
 * @param cfg  Flash handle, region geometry, and optional SPI mutex.
 * @return     Pointer to the (statically allocated) port, or NULL on error.
 */
Lfs_port* Lfs_Port_Create(const Lfs_port_config* cfg);

/**
 * @brief Format the region with a fresh LittleFS superblock.
 *
 * Destroys all existing data in the configured region.
 */
int Lfs_Port_Format(Lfs_port* obj);

/**
 * @brief Mount an existing LittleFS filesystem on the configured region.
 */
int Lfs_Port_Mount(Lfs_port* obj);

/**
 * @brief Unmount the filesystem, flushing any cached data.
 */
int Lfs_Port_Unmount(Lfs_port* obj);

/* ---- accessors --------------------------------------------------------- */

lfs_t* Lfs_Port_Get_Lfs(Lfs_port* obj);

// clang-format on

#endif
