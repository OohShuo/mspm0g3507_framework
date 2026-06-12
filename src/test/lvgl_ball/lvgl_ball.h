#pragma once

// Classic "bouncing ball screensaver" on the ST7789 panel, implemented
// on top of LVGL v9. The play field is a 230x230 region inset 5 px
// from each edge of the 240x240 panel; a 10x10 ball moves at constant
// pixel-per-frame speed and reverses its X or Y component when it
// touches the inset border.
//
// Wire the panel + run the ST7789 init sequence. Must be called from a
// task context.
void App_Lvgl_Ball_Init(void);

// Step the simulation by one frame. Drives lv_timer_handler() and
// updates the ball's LVGL object position. Call from a periodic task.
void App_Lvgl_Ball_Loop(void);
