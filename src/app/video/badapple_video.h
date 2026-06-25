#pragma once

#include <stdint.h>

#include "st7789.h"

/* ── Video source ── */
typedef enum {
    badapple_video_source_builtin = 0,
    badapple_video_source_lfs_file = 1,
} Badapple_video_source;

/* ── Public API ── */
uint8_t Badapple_Video_Init(St7789* lcd, Badapple_video_source source);
void Badapple_Video_Update(void);
void Badapple_Video_Stop(void);
uint8_t Badapple_Video_Is_Active(void);
uint8_t Badapple_Video_Is_Source_Available(Badapple_video_source source);
