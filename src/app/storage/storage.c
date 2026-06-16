#include "storage.h"

#include <stddef.h>

#include "board_config.h"

#if FRAMEWORK_USE_LFS

    #include "FreeRTOS.h"
    #include "lfs_port.h"
    #include "rtt_log.h"
    #include "semphr.h"
    #include "w25q32.h"

    #define STORAGE_LFS_START   (2u * 1024u * 1024u)
    #define STORAGE_LFS_SIZE    (2u * 1024u * 1024u)
    #define STORAGE_RAW_LIMIT   STORAGE_LFS_START
    #define STORAGE_SECTOR_SIZE 4096u
    #define STORAGE_PAGE_SIZE   256u

static W25q32* g_flash = NULL;
static Lfs_port* g_port = NULL;
static SemaphoreHandle_t g_mutex = NULL;
static uint8_t g_available = 0;

uint8_t Storage_Init(void) {
    if (g_available) { return 1; }

    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL) {
        printf("[STORAGE] mutex allocation failed\n");
        return 0;
    }

    const W25q32_config flash_config = {
        .spi_idx = SPI_LCD_IDX,
        .cs_gpio_idx = GPIO_SPI_CS_IDX,
    };
    g_flash = W25q32_Create(&flash_config);
    if (g_flash == NULL || !W25q32_Init(g_flash)) {
        printf("[STORAGE] W25Q32 unavailable\n");
        return 0;
    }

    const Lfs_port_config port_config = {
        .flash = g_flash,
        .start = STORAGE_LFS_START,
        .size = STORAGE_LFS_SIZE,
        .spi_mutex = NULL,
    };
    g_port = Lfs_Port_Create(&port_config);
    if (g_port == NULL) {
        printf("[STORAGE] LittleFS port unavailable\n");
        return 0;
    }

    int result = Lfs_Port_Mount(g_port);
    if (result != 0) {
        printf("[STORAGE] LittleFS mount failed: %d\n", result);
        return 0;
    }

    g_available = 1;
    printf("[STORAGE] external filesystem ready\n");
    return 1;
}

uint8_t Storage_Is_Available(void) { return g_available; }

uint8_t Storage_Raw_Read(uint32_t address, void* data, uint32_t size) {
    if (!g_available || g_flash == NULL || data == NULL || size == 0 || address >= STORAGE_RAW_LIMIT ||
        size > STORAGE_RAW_LIMIT - address) {
        return 0;
    }
    uint8_t* destination = (uint8_t*)data;
    Storage_Lock();
    while (size > 0) {
        uint32_t chunk = size > 512u ? 512u : size;
        W25q32_Read(g_flash, address, destination, chunk);
        address += chunk;
        destination += chunk;
        size -= chunk;
    }
    Storage_Unlock();
    return 1;
}

uint8_t Storage_Raw_Write(uint32_t address, const void* data, uint32_t size) {
    if (!g_available || g_flash == NULL || data == NULL || size == 0 || address >= STORAGE_RAW_LIMIT ||
        size > STORAGE_RAW_LIMIT - address) {
        return 0;
    }

    const uint8_t* source = (const uint8_t*)data;
    Storage_Lock();
    while (size > 0) {
        uint32_t chunk = STORAGE_PAGE_SIZE - (address & (STORAGE_PAGE_SIZE - 1u));
        if (chunk > size) { chunk = size; }
        W25q32_Page_Program(g_flash, address, source, chunk);
        address += chunk;
        source += chunk;
        size -= chunk;
    }
    Storage_Unlock();
    return 1;
}

uint8_t Storage_Raw_Erase(uint32_t address, uint32_t size) {
    if (!g_available || g_flash == NULL || size == 0 || (address & (STORAGE_SECTOR_SIZE - 1u)) != 0 ||
        address >= STORAGE_RAW_LIMIT || size > STORAGE_RAW_LIMIT - address) {
        return 0;
    }

    const uint32_t end = (address + size + STORAGE_SECTOR_SIZE - 1u) & ~(STORAGE_SECTOR_SIZE - 1u);
    Storage_Lock();
    while (address < end) {
        W25q32_Sector_Erase(g_flash, address);
        address += STORAGE_SECTOR_SIZE;
    }
    Storage_Unlock();
    return 1;
}

uint8_t Storage_Format(void) {
    if (g_port == NULL) { return 0; }

    Storage_Lock();
    if (g_available) { (void)Lfs_Port_Unmount(g_port); }
    g_available = 0;

    int result = Lfs_Port_Format(g_port);
    if (result == 0) { result = Lfs_Port_Mount(g_port); }
    if (result == 0) { g_available = 1; }
    Storage_Unlock();
    return result == 0;
}

lfs_t* Storage_Get_Lfs(void) { return g_available && g_port != NULL ? Lfs_Port_Get_Lfs(g_port) : NULL; }

void Storage_Lock(void) {
    if (g_mutex != NULL) { xSemaphoreTake(g_mutex, portMAX_DELAY); }
}

void Storage_Unlock(void) {
    if (g_mutex != NULL) { xSemaphoreGive(g_mutex); }
}

#else

uint8_t Storage_Init(void) { return 0; }

uint8_t Storage_Is_Available(void) { return 0; }

uint8_t Storage_Format(void) { return 0; }

uint8_t Storage_Raw_Read(uint32_t address, void* data, uint32_t size) {
    (void)address;
    (void)data;
    (void)size;
    return 0;
}

uint8_t Storage_Raw_Write(uint32_t address, const void* data, uint32_t size) {
    (void)address;
    (void)data;
    (void)size;
    return 0;
}

uint8_t Storage_Raw_Erase(uint32_t address, uint32_t size) {
    (void)address;
    (void)size;
    return 0;
}

#endif
