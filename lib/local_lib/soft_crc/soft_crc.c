#include "soft_crc.h"

#include <stdlib.h>
#include <string.h>

Soft_crc16* soft_crc16_default = NULL;
Soft_crc8* soft_crc8_default = NULL;

static uint16_t bit_reverse_u16(uint16_t x) {
    uint16_t result = 0;
    for (uint32_t i = 0; i < 16; i++) {
        result |= (uint16_t)(((x >> i) & 1u) << (15 - i));
    }
    return result;
}

static uint8_t bit_reverse_u8(uint8_t x) {
    uint8_t result = 0;
    for (uint32_t i = 0; i < 8; i++) {
        result |= (uint8_t)(((x >> i) & 1u) << (7 - i));
    }
    return result;
}

Soft_crc16* Soft_Crc16_Create(const Soft_crc16_config* config) {
    if (config == NULL) { return NULL; }
    Soft_crc16* obj = (Soft_crc16*)malloc(sizeof(Soft_crc16));
    if (obj == NULL) { return NULL; }

    uint16_t table_poly = bit_reverse_u16(config->poly);
    for (uint32_t i = 0; i < 256; i++) {
        uint16_t crc = 0;
        uint16_t c = (uint16_t)i;
        for (uint32_t j = 0; j < 8; j++) {
            if ((crc ^ c) & 0x0001u) {
                crc = (uint16_t)((crc >> 1) ^ table_poly);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
            c = (uint16_t)(c >> 1);
        }
        obj->table[i] = crc;
    }
    obj->crc_init = config->crc_init;
    return obj;
}

Soft_crc8* Soft_Crc8_Create(const Soft_crc8_config* config) {
    if (config == NULL) { return NULL; }
    Soft_crc8* obj = (Soft_crc8*)malloc(sizeof(Soft_crc8));
    if (obj == NULL) { return NULL; }

    uint8_t table_poly = bit_reverse_u8(config->poly);
    for (uint32_t i = 0; i < 256; i++) {
        uint8_t crc = 0;
        uint8_t c = (uint8_t)i;
        for (uint32_t j = 0; j < 8; j++) {
            if ((crc ^ c) & 0x01u) {
                crc = (uint8_t)((crc >> 1) ^ table_poly);
            } else {
                crc = (uint8_t)(crc >> 1);
            }
            c = (uint8_t)(c >> 1);
        }
        obj->table[i] = crc;
    }
    obj->crc_init = config->crc_init;
    return obj;
}

void Soft_Crc_Init(void) {
    static uint8_t inited = 0;
    if (inited) { return; }
    inited = 1;

    Soft_crc16_config crc16_cfg = {
        .poly = SOFT_CRC16_CCITT_POLY,
        .crc_init = SOFT_CRC16_MODBUS_INIT,
    };
    soft_crc16_default = Soft_Crc16_Create(&crc16_cfg);

    Soft_crc8_config crc8_cfg = {
        .poly = SOFT_CRC8_MAXIM_POLY,
        .crc_init = SOFT_CRC8_MODBUS_INIT,
    };
    soft_crc8_default = Soft_Crc8_Create(&crc8_cfg);
}

uint16_t Soft_Crc16_Calc(const Soft_crc16* obj, const uint8_t* data, uint32_t num_bytes) {
    if (obj == NULL || data == NULL) { return 0; }
    uint16_t crc = obj->crc_init;
    for (uint32_t i = 0; i < num_bytes; i++) {
        crc = (uint16_t)((crc >> 8) ^ obj->table[(crc ^ data[i]) & 0xFFu]);
    }
    return crc;
}

uint8_t Soft_Crc8_Calc(const Soft_crc8* obj, const uint8_t* data, uint32_t num_bytes) {
    if (obj == NULL || data == NULL) { return 0; }
    uint8_t crc = obj->crc_init;
    for (uint32_t i = 0; i < num_bytes; i++) {
        crc = obj->table[(crc ^ data[i]) & 0xFFu];
    }
    return crc;
}
