#pragma once

// Init: wires LVGL v9 to the ST7789 panel driven by soft_spi via the
// built-in lv_st7789 driver (lv_lcd_generic_mipi under the hood) and
// drops a "Hello world" label on the screen. The send_cmd / send_color
// callbacks in lvgl_hello.c bridge the MIPI driver to the St7789 HAL.
//
// Init is one-shot — call once from a FreeRTOS task (it touches
// Bsp_Get_Tick_Ms through the lv_tick_set_cb trampoline).
void App_Lvgl_Hello_Init(void);

// Step LVGL. Call from a periodic task (~5 ms). Idempotent if the task
// is delayed — lv_timer_handler() catches up internally.
void App_Lvgl_Hello_Loop(void);
