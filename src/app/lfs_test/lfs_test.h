#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char* name;
    bool passed;
    int lfs_err; /**< raw lfs error code (0 = ok) for diagnosis */
} Lfs_Test_Result;

void App_Lfs_Test_Init(void);
void App_Lfs_Test_Loop(void);

const Lfs_Test_Result* App_Lfs_Test_Get_Results(void);
uint8_t App_Lfs_Test_Get_Result_Count(void);
bool App_Lfs_Test_All_Passed(void);
