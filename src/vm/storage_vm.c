#include <stdint.h>
#include <stdio.h>

#include "lfs.h"

/* ── VM storage layer — uses host filesystem directly (no W25Q32 / LittleFS) ── */

static lfs_t g_vm_lfs;
static uint8_t g_available = 0;

uint8_t Storage_Init(void) {
    if (g_available) { return 1; }

    int rc = lfs_mount(&g_vm_lfs, NULL);
    if (rc != 0) {
        rc = lfs_format(&g_vm_lfs, NULL);
        if (rc == 0) { rc = lfs_mount(&g_vm_lfs, NULL); }
    }
    if (rc != 0) {
        printf("[STORAGE] VM filesystem init failed: %d\n", rc);
        return 0;
    }

    g_available = 1;
    printf("[STORAGE] VM filesystem ready (host: %s)\n", g_vm_lfs.root);
    return 1;
}

uint8_t Storage_Is_Available(void) { return g_available; }

lfs_t* Storage_Get_Lfs(void) { return g_available ? &g_vm_lfs : NULL; }

uint8_t Storage_Format(void) {
    g_available = 0;
    int rc = lfs_format(&g_vm_lfs, NULL);
    if (rc == 0) { rc = lfs_mount(&g_vm_lfs, NULL); }
    if (rc == 0) { g_available = 1; }
    return rc == 0;
}

void Storage_Lock(void) { /* no-op on host */ }
void Storage_Unlock(void) { /* no-op on host */ }

uint8_t Storage_Raw_Read(uint32_t address, void* data, uint32_t size) {
    (void)address; (void)data; (void)size; return 0;
}
uint8_t Storage_Raw_Write(uint32_t address, const void* data, uint32_t size) {
    (void)address; (void)data; (void)size; return 0;
}
uint8_t Storage_Raw_Erase(uint32_t address, uint32_t size) {
    (void)address; (void)size; return 0;
}
