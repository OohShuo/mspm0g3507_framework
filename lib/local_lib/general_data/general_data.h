#pragma once

#include <stdint.h>

typedef struct {
    uint8_t len;
    uint8_t* data;
    uint16_t crc16;
} General_data;

uint8_t General_Data_Check(const uint8_t* buffer, uint32_t buf_len);
void General_Data_To_Buffer(const General_data* data, uint8_t* buffer);
void General_Data_From_Buffer(const uint8_t* buffer, General_data* data);
