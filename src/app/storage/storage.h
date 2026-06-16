#pragma once

#include <stdint.h>

#if FRAMEWORK_USE_LFS
    #include "lfs.h"
#endif

uint8_t Storage_Init(void);
uint8_t Storage_Is_Available(void);
uint8_t Storage_Format(void);
uint8_t Storage_Raw_Read(uint32_t address, void* data, uint32_t size);
uint8_t Storage_Raw_Write(uint32_t address, const void* data, uint32_t size);
uint8_t Storage_Raw_Erase(uint32_t address, uint32_t size);

#if FRAMEWORK_USE_LFS
lfs_t* Storage_Get_Lfs(void);
void Storage_Lock(void);
void Storage_Unlock(void);
#endif
