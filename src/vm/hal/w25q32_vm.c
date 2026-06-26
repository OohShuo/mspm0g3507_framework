#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "w25q32.h"

#define VM_FLASH_SIZE (4u * 1024u * 1024u) /* 4 MiB W25Q32 */

static W25q32 s_flash;
static FILE* s_backing_file = NULL;
static uint8_t* s_buffer = NULL;

/* ── Search for the backing file in common locations ── */
static FILE* open_backing_file(const char* mode) {
    static const char* search_paths[] = {
        "build/vm/vm_flash.bin",
        "vm_flash.bin",
        "../build/vm/vm_flash.bin",
        "../vm_flash.bin",
        "../../build/vm/vm_flash.bin",
    };
    for (int i = 0; i < (int)(sizeof(search_paths) / sizeof(search_paths[0])); i++) {
        FILE* f = fopen(search_paths[i], mode);
        if (f != NULL) { return f; }
    }
    /* Create a new blank flash image in build/vm/ */
    return fopen("build/vm/vm_flash.bin", "w+b");
}

W25q32* W25q32_Create(const W25q32_config* c) {
    if (!c) return NULL;
    memset(&s_flash, 0, sizeof(s_flash));
    s_flash.config = *c;
    s_flash.manufacturer_id = 0xEF;
    s_flash.memory_type = 0x40;
    s_flash.capacity = 0x17;

    /* Allocate in-memory buffer */
    if (s_buffer == NULL) { s_buffer = (uint8_t*)malloc(VM_FLASH_SIZE); }
    if (s_buffer == NULL) { return NULL; }
    memset(s_buffer, 0xFF, VM_FLASH_SIZE);

    /* Try to load from backing file */
    if (s_backing_file == NULL) {
        s_backing_file = open_backing_file("r+b");
        if (s_backing_file != NULL) {
            fseek(s_backing_file, 0, SEEK_END);
            long file_size = ftell(s_backing_file);
            if (file_size >= (long)VM_FLASH_SIZE) {
                fseek(s_backing_file, 0, SEEK_SET);
                size_t n = fread(s_buffer, 1, VM_FLASH_SIZE, s_backing_file);
                if (n != VM_FLASH_SIZE) {
                    /* File too short, fill rest with 0xFF */
                    memset(s_buffer + n, 0xFF, VM_FLASH_SIZE - n);
                }
            } else {
                /* File exists but is smaller than expected — use what we can */
                fseek(s_backing_file, 0, SEEK_SET);
                size_t nread = fread(s_buffer, 1, (size_t)file_size, s_backing_file);
                (void)nread;
                fclose(s_backing_file);
                /* Re-create it at the right size */
                s_backing_file = fopen("build/vm/vm_flash.bin", "w+b");
                if (s_backing_file != NULL) {
                    fwrite(s_buffer, 1, VM_FLASH_SIZE, s_backing_file);
                    fflush(s_backing_file);
                }
            }
        } else {
            /* No backing file — create one */
            s_backing_file = fopen("build/vm/vm_flash.bin", "w+b");
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

static void flush_to_file(void) {
    if (s_backing_file == NULL || s_buffer == NULL) { return; }
    fseek(s_backing_file, 0, SEEK_SET);
    fwrite(s_buffer, 1, VM_FLASH_SIZE, s_backing_file);
    fflush(s_backing_file);
}

void W25q32_Page_Program(W25q32* o, uint32_t addr, const uint8_t* data, uint32_t len) {
    (void)o;
    if (data == NULL || len == 0 || s_buffer == NULL) { return; }
    if (addr >= VM_FLASH_SIZE) { return; }
    uint32_t remaining = VM_FLASH_SIZE - addr;
    if (len > remaining) { len = remaining; }
    /* W25Q32 Page_Program can only clear bits (1→0); write emulates this */
    memcpy(s_buffer + addr, data, len);
    flush_to_file();
}

void W25q32_Sector_Erase(W25q32* o, uint32_t addr) {
    (void)o;
    if (s_buffer == NULL || addr >= VM_FLASH_SIZE) { return; }
    memset(s_buffer + addr, 0xFF, 4096);
    flush_to_file();
}

void W25q32_Block_Erase_32K(W25q32* o, uint32_t addr) {
    (void)o;
    if (s_buffer == NULL || addr >= VM_FLASH_SIZE) { return; }
    uint32_t size = 32u * 1024u;
    if (addr + size > VM_FLASH_SIZE) { size = VM_FLASH_SIZE - addr; }
    memset(s_buffer + addr, 0xFF, size);
    flush_to_file();
}

void W25q32_Block_Erase_64K(W25q32* o, uint32_t addr) {
    (void)o;
    if (s_buffer == NULL || addr >= VM_FLASH_SIZE) { return; }
    uint32_t size = 64u * 1024u;
    if (addr + size > VM_FLASH_SIZE) { size = VM_FLASH_SIZE - addr; }
    memset(s_buffer + addr, 0xFF, size);
    flush_to_file();
}

void W25q32_Chip_Erase(W25q32* o) {
    (void)o;
    if (s_buffer == NULL) { return; }
    memset(s_buffer, 0xFF, VM_FLASH_SIZE);
    flush_to_file();
}

void W25q32_Power_Down(W25q32* o) { (void)o; }
void W25q32_Release_Power_Down(W25q32* o) { (void)o; }
void W25q32_Reset(W25q32* o) { (void)o; }
