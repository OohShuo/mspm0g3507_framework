#include "lcd_video_player.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "task.h"

void Lcd_Video_Play(St7789* lcd,
                    const uint8_t* delta_data,
                    const LcdDeltaFrame* frames,
                    uint32_t count,
                    uint32_t fps,
                    uint32_t loop_count)
{
    if (lcd == NULL || delta_data == NULL || frames == NULL || count == 0 || fps == 0) {
        return;
    }

    const TickType_t frame_ticks = pdMS_TO_TICKS(1000 / fps);

    /* Blank to black, then flash full white for 200ms to confirm
     * the entire screen is addressable. */
    St7789_Fill_Color(lcd, 0x0000);
    St7789_Fill_Color(lcd, 0xFFFF);
    vTaskDelay(pdMS_TO_TICKS(200));
    St7789_Fill_Color(lcd, 0x0000);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint16_t line_buf[240];

    uint32_t loop = 0;
    while (loop_count == 0 || loop < loop_count) {
        /* Reset to black at the start of each loop so delta frame 0
         * (which was encoded against an all-black baseline) renders
         * correctly even on repeat plays. */
        St7789_Fill_Color(lcd, 0x0000);
        vTaskDelay(pdMS_TO_TICKS(50));

        /* Canonical vTaskDelayUntil pattern: do NOT pre-add frame_ticks. */
        TickType_t next_tick = xTaskGetTickCount();

        for (uint32_t i = 0; i < count; i++) {
            const LcdDeltaFrame* f = &frames[i];
            uint32_t offset = f->offset;

            /* ── 4-byte header ── */
            uint16_t packed_bytes  = (uint16_t)(delta_data[offset]) |
                                     ((uint16_t)(delta_data[offset + 1]) << 8);
            uint16_t num_segments  = (uint16_t)(delta_data[offset + 2]) |
                                     ((uint16_t)(delta_data[offset + 3]) << 8);
            offset += 4;
            /* frame_end = pixel data + segment headers (3 bytes each) */
            uint32_t frame_end = offset + (uint32_t)packed_bytes + (uint32_t)num_segments * 3U;

            /* ── Decode segments ── */
            for (uint16_t s = 0; s < num_segments; s++) {
                if (offset + 3 > frame_end) break;

                uint8_t row       = delta_data[offset++];
                uint8_t col_start = delta_data[offset++];
                uint8_t col_end   = delta_data[offset++];
                uint32_t seg_len  = (uint32_t)(col_end - col_start + 1);
                uint32_t pack_len = (seg_len + 7) / 8;

                if (seg_len > 240 || offset + pack_len > frame_end) break;

                /* Unpack 1-bit pixels → RGB565 */
                for (uint32_t p = 0; p < seg_len; p++) {
                    uint8_t byte = delta_data[offset + p / 8];
                    uint8_t bit  = (byte >> (7 - (p % 8))) & 1;
                    line_buf[p]  = bit ? 0xFFFF : 0x0000;
                }
                offset += pack_len;

                St7789_Flush(lcd,
                             (int32_t)col_start,
                             (int32_t)row,
                             (int32_t)col_end,
                             (int32_t)row,
                             (uint8_t*)line_buf,
                             seg_len * 2);
            }

            /* Wait for next frame — vTaskDelayUntil adds frame_ticks
             * to *next_tick internally, so we don't pre-add. */
            vTaskDelayUntil(&next_tick, frame_ticks);
        }
        loop++;
    }
}
