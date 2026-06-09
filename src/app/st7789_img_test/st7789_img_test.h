#pragma once

#include <stdbool.h>
#include <stdint.h>

// Wire the LCD instance and run the ST7789 init sequence. Must be called
// from a task (the init uses busy-wait delays but no FreeRTOS APIs).
void App_St7789_Img_Test_Init(void);

// Step the image-display test. Call from a periodic task. Idempotent when
// the previous flush has not finished.
void App_St7789_Img_Test_Loop(void);

typedef struct {
    const char* image_name;
    uint32_t flush_count;
    bool init_done;
} St7789_img_test_status;

const St7789_img_test_status* App_St7789_Img_Test_Get_Status(void);
