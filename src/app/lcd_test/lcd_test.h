#pragma once

// LCD test: creates the St7789 instance, sends the init sequence, and
// (when enabled) cycles the screen through a few solid colors to verify
// the full pipeline (SPI + DMA + DC + init sequence).
//
// Currently the per-frame flush is disabled in lcd_test.c (kept as a
// reference implementation). To re-enable, uncomment the loop body in
// App_Lcd_Test_Loop and add a task in main.c that calls it.

void App_Lcd_Test_Init(void);
void App_Lcd_Test_Loop(void);
