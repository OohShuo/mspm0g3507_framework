#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <io.h>
    #define fsync _commit
    #define fileno _fileno
#else
    #include <unistd.h>
#endif

#include "w25q32.h"

#define VM_FLASH_SIZE (4u * 1024u * 1024u) /* 4 MiB W25Q32 */

static W25q32 s_flash;
static FILE* s_backing_file = NULL;
static uint8_t* s_buffer = NULL;
static uint8_t s_dirty = 0;

/* ── Persist buffer to backing file ── */
static void flush_to_file(void) {
    if (s_backing_file == NULL || s_buffer == NULL || !s_dirty) { return; }
    fseek(s_backing_file, 0, SEEK_SET);
    if (fwrite(s_buffer, 1, VM_FLASH_SIZE, s_backing_file) != VM_FLASH_SIZE) { return; }
    fflush(s_backing_file);
    fsync(fileno(s_backing_file));
    s_dirty = 0;
}

/* ── Search for the backing file, return NULL if not found ── */
static FILE* try_open_existing(void) {
    static const char* search_paths[] = {
        "assets/vm_flash/vm_flash.bin",
        "../assets/vm_flash/vm_flash.bin",
        "../../assets/vm_flash/vm_flash.bin",
        "build/vm/vm_flash.bin",
        "vm_flash.bin",
    };
    for (int i = 0; i < (int)(sizeof(search_paths) / sizeof(search_paths[0])); i++) {
        FILE* f = fopen(search_paths[i], "r+b");
        if (f != NULL) { return f; }
    }
    return NULL;
}

static void load_file_into_buffer(void) {
    fseek(s_backing_file, 0, SEEK_END);
    long file_size = ftell(s_backing_file);
    fseek(s_backing_file, 0, SEEK_SET);

    if (file_size >= (long)VM_FLASH_SIZE) {
        size_t n = fread(s_buffer, 1, VM_FLASH_SIZE, s_backing_file);
        if (n < VM_FLASH_SIZE) {
            memset(s_buffer + n, 0xFF, VM_FLASH_SIZE - n);
        }
    } else {
        /* Existing file too small — read what we can, pad with 0xFF */
        if (file_size > 0) {
            size_t n = fread(s_buffer, 1, (size_t)file_size, s_backing_file);
            (void)n;
        }
        fclose(s_backing_file);
        /* Recreate at full size in assets/vm_flash/ */
        s_backing_file = fopen("assets/vm_flash/vm_flash.bin", "w+b");
        if (s_backing_file != NULL) {
            fwrite(s_buffer, 1, VM_FLASH_SIZE, s_backing_file);
            fflush(s_backing_file);
        }
    }
}

W25q32* W25q32_Create(const W25q32_config* c) {
    if (!c) return NULL;
    memset(&s_flash, 0, sizeof(s_flash));
    s_flash.config = *c;
    s_flash.manufacturer_id = 0xEF;
    s_flash.memory_type = 0x40;
    s_flash.capacity = 0x17;

    /* One-time buffer allocation */
    if (s_buffer == NULL) {
        s_buffer = (uint8_t*)malloc(VM_FLASH_SIZE);
        if (s_buffer == NULL) { return NULL; }
        memset(s_buffer, 0xFF, VM_FLASH_SIZE);

        /* Load from existing backing file, or create a blank one */
        s_backing_file = try_open_existing();
        if (s_backing_file != NULL) {
            load_file_into_buffer();
        } else {
            s_backing_file = fopen("assets/vm_flash/vm_flash.bin", "w+b");
            if (s_backing_file != NULL) {
                fwrite(s_buffer, 1, VM_FLASH_SIZE, s_backing_file);
                fflush(s_backing_file);
            }
        }
    }

    return &s_flash;
}

uint8_t W25q32_Init(W25q32* o) {
    (void)o;
    return (s_buffer != NULL) ? 1 : 0;
}

void W25q32_Read_Jedec_Id(W25q32* o, uint8_t* id) {
    (void)o;
    if (id) {
        id[0] = 0xEF;
        id[1] = 0x40;
        id[2] = 0x17;
    }
}

uint8_t W25q32_Read_Status_Reg_1(W25q32* o) {
    (void)o;
    return 0; /* Always ready */
}

void W25q32_Write_Enable(W25q32* o) { (void)o; }
void W25q32_Write_Status_Reg_1(W25q32* o, uint8_t s) {
    (void)o;
    (void)s;
}
void W25q32_Wait_Busy(W25q32* o) { (void)o; }

void W25q32_Read(W25q32* o, uint32_t addr, uint8_t* data, uint32_t len) {
    (void)o;
    if (data == NULL || len == 0 || s_buffer == NULL) { return; }
    if (addr >= VM_FLASH_SIZE) { return; }
    uint32_t remaining = VM_FLASH_SIZE - addr;
    if (len > remaining) { len = remaining; }
    memcpy(data, s_buffer + addr, len);
}

void W25q32_Page_Program(W25q32* o, uint32_t addr, const uint8_t* data, uint32_t len) {
    (void)o;
    if (data == NULL || len == 0 || s_buffer == NULL) { return; }
    if (addr >= VM_FLASH_SIZE) { return; }
    uint32_t remaining = VM_FLASH_SIZE - addr;
    if (len > remaining) { len = remaining; }
    memcpy(s_buffer + addr, data, len);
    s_dirty = 1;
    flush_to_file();  /* persist every write — scores must survive exit */
}

void W25q32_Sector_Erase(W25q32* o, uint32_t addr) {
    (void)o;
    if (s_buffer == NULL || addr >= VM_FLASH_SIZE) { return; }
    flush_to_file();
    memset(s_buffer + addr, 0xFF, 4096);
    s_dirty = 1;
}

void W25q32_Block_Erase_32K(W25q32* o, uint32_t addr) {
    (void)o;
    if (s_buffer == NULL || addr >= VM_FLASH_SIZE) { return; }
    flush_to_file();
    uint32_t size = 32u * 1024u;
    if (addr + size > VM_FLASH_SIZE) { size = VM_FLASH_SIZE - addr; }
    memset(s_buffer + addr, 0xFF, size);
    s_dirty = 1;
}

void W25q32_Block_Erase_64K(W25q32* o, uint32_t addr) {
    (void)o;
    if (s_buffer == NULL || addr >= VM_FLASH_SIZE) { return; }
    flush_to_file();
    uint32_t size = 64u * 1024u;
    if (addr + size > VM_FLASH_SIZE) { size = VM_FLASH_SIZE - addr; }
    memset(s_buffer + addr, 0xFF, size);
    s_dirty = 1;
}

void W25q32_Chip_Erase(W25q32* o) {
    (void)o;
    if (s_buffer == NULL) { return; }
    flush_to_file();
    memset(s_buffer, 0xFF, VM_FLASH_SIZE);
    s_dirty = 1;
}

void W25q32_Power_Down(W25q32* o) {
    (void)o;
    flush_to_file();
}

void W25q32_Release_Power_Down(W25q32* o) { (void)o; }
void W25q32_Reset(W25q32* o) {
    (void)o;
    flush_to_file();
}
