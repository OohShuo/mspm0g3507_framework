#pragma once

#include <stdint.h>
#include "st7789.h"

/**
 * Delta-frame video player for ST7789 (1-bit packed).
 *
 * Each delta frame stores only the changed row-segments.  Pixel data
 * is packed 8 pixels (1-bit black/white) per byte.
 *
 * Delta frame binary format:
 *   [0..1]   uint16 LE  packed_bytes   (packed pixel data size)
 *   [2..3]   uint16 LE  num_segments
 *   For each segment:
 *     [0]    uint8  row
 *     [1]    uint8  col_start
 *     [2]    uint8  col_end
 *     [3..]  uint8  packed_bits[ceil((col_end-col_start+1)/8)]
 */

typedef struct {
    uint32_t offset;           // byte offset into delta_data[]
    uint16_t packed_bytes;     // packed pixel data bytes in this frame
    uint16_t num_segments;     // number of row segments
} LcdDeltaFrame;

void Lcd_Video_Play(St7789* lcd,
                    const uint8_t* delta_data,
                    const LcdDeltaFrame* frames,
                    uint32_t count,
                    uint32_t fps,
                    uint32_t loop_count);
