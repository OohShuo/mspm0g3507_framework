#pragma once

#include <stdint.h>

#define SOFT_CRC16_CCITT_POLY  0x1021
#define SOFT_CRC16_MODBUS_INIT 0xFFFF
#define SOFT_CRC8_MAXIM_POLY   0x31
#define SOFT_CRC8_MODBUS_INIT  0xFF

typedef struct {
    uint16_t poly;
    uint16_t crc_init;
} Soft_crc16_config;

typedef struct {
    uint16_t table[256];
    uint16_t crc_init;
} Soft_crc16;

typedef struct {
    uint8_t poly;
    uint8_t crc_init;
} Soft_crc8_config;

typedef struct {
    uint8_t table[256];
    uint8_t crc_init;
} Soft_crc8;

extern Soft_crc16* soft_crc16_default;
extern Soft_crc8* soft_crc8_default;

void Soft_Crc_Init(void);

Soft_crc16* Soft_Crc16_Create(const Soft_crc16_config* config);
uint16_t Soft_Crc16_Calc(const Soft_crc16* obj, const uint8_t* data, uint32_t num_bytes);

Soft_crc8* Soft_Crc8_Create(const Soft_crc8_config* config);
uint8_t Soft_Crc8_Calc(const Soft_crc8* obj, const uint8_t* data, uint32_t num_bytes);
