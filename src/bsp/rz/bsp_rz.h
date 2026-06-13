#pragma once

#include <stdint.h>

#define RZ_CODE_BUF_SIZE 128

typedef struct Bsp_rz_config_t {
    uint32_t period_ns;
    struct Bsp_rz_code_t {
        uint32_t high_ns;
        uint32_t low_ns;
    } one_code, zero_code;
    uint32_t reset_required_us;
} Bsp_rz_config;

void Bsp_Rz_Init(void);

void Bsp_Rz_Set_Config(uint32_t idx, Bsp_rz_config* config);
void Bsp_Rz_Start(uint32_t idx, uint8_t* data, uint32_t len);
uint8_t Bsp_Rz_Is_Busy(uint32_t idx);

void Bsp_Rz_Iqr_Handler(uint32_t idx);
