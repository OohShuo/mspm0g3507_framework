#pragma once

#include <stdint.h>

// Test status, exposed so a host-side tool (or a debug log) can read
// the current pattern name. The panel is write-only on this board
// (no MISO), so there's no readback status to report.
typedef struct {
    const char* pattern_name;
    uint8_t pattern_idx;
    uint8_t init_done;
} Lcd_test_status;

// Wire the LCD instance and run the ST7789 init sequence. Must be called
// from a task (the init uses busy-wait delays but no FreeRTOS APIs).
void App_Lcd_Test_Init(void);

// Step the test pattern forward. Call from a periodic task. Idempotent
// when `pattern_hold_ms` has not elapsed — re-runs the same pattern.
void App_Lcd_Test_Loop(void);

const Lcd_test_status* App_Lcd_Test_Get_Status(void);
