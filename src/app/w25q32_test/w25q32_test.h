#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char* name;
    bool passed;
    uint8_t raw;
} W25q32_test_result;

void App_W25q32_Test_Init(void);

void App_W25q32_Test_Loop(void);

const W25q32_test_result* App_W25q32_Test_Get_Results(void);
uint8_t App_W25q32_Test_Get_Result_Count(void);
bool App_W25q32_Test_All_Passed(void);
