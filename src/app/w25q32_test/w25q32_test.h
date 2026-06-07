#pragma once

#include <stdbool.h>
#include <stdint.h>

// Per-test result entry. `name` is a static string literal, `passed` is
// the outcome of that step, `raw` is an optional raw byte (e.g. SR1 value)
// for diagnostic visibility.
typedef struct {
    const char* name;
    bool passed;
    uint8_t raw;
} W25q32_test_result;

// Set up the W25Q32 instance and run the JEDEC ID sanity check. Lights
// PWR_LED on success. Must be called from a task after the scheduler starts
// (uses blocking SPI).
void App_W25q32_Test_Init(void);

// Run the full read/erase/program test battery. PWR_LED: ON if all pass,
// OFF if any fail. Re-runnable; safe to call periodically.
void App_W25q32_Test_Loop(void);

// Accessors for the most recent test run. Read-only; updated by
// App_W25q32_Test_Loop before it returns.
const W25q32_test_result* App_W25q32_Test_Get_Results(void);
uint8_t App_W25q32_Test_Get_Result_Count(void);
bool App_W25q32_Test_All_Passed(void);
