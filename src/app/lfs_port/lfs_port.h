#pragma once

#if FRAMEWORK_USE_LFS

// clang-format off

#include <stdint.h>

#include "lfs.h"
#include "w25q32.h"

typedef struct {
    W25q32* flash;
    uint32_t start;
    uint32_t size;
} Lfs_port_config;

typedef struct {
    lfs_t lfs;
    struct lfs_config cfg;
    Lfs_port_config port;

    uint8_t read_buffer[256];
    uint8_t prog_buffer[256];
    uint8_t lookahead_buffer[16];
} Lfs_port;

Lfs_port* Lfs_Port_Create(const Lfs_port_config* cfg);

int Lfs_Port_Format(Lfs_port* obj);
int Lfs_Port_Mount(Lfs_port* obj);
int Lfs_Port_Unmount(Lfs_port* obj);
lfs_t* Lfs_Port_Get_Lfs(Lfs_port* obj);

// clang-format on

#endif
