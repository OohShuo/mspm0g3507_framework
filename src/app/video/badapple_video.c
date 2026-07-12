#include "badapple_video.h"

#include <stddef.h>
#include <string.h>

/* ── Bad Apple video config ── */
#define BADAPPLE_VIDEO_USE_BUILTIN  0
#define BADAPPLE_VIDEO_USE_LFS_FILE FRAMEWORK_USE_LFS
#define BADAPPLE_VIDEO_LFS_PATH     "/badapple.bard"

#include "bsp_time.h"
#include "game_graphics.h"
#include "storage.h"

#if BADAPPLE_VIDEO_USE_LFS_FILE
    #include "lfs.h"
#endif

/* ── Constants ── */
#define SCREEN_WIDTH                   240
#define SCREEN_HEIGHT                  320

#define BARD_HEADER_SIZE               64u
#define BARD_INDEX_ENTRY_SIZE          8u
#define BARD_RECT_FORMAT_RECT5         1u
#define BARD_RECT5_SIZE                5u
#define BARD_FLAG_LOOP                 (1u << 0)

#define BADAPPLE_COLOR_BLACK           0x0000u
#define BADAPPLE_COLOR_WHITE           0xFFFFu

#define BADAPPLE_RECT_CACHE_COUNT      16u
#define BADAPPLE_MAX_FRAMES_PER_UPDATE 2u
#define BADAPPLE_CATCHUP_MAX           12u

/* ── Static buffers ── */
static St7789* g_lcd = NULL;
static uint8_t g_header_buf[BARD_HEADER_SIZE];
static uint8_t g_index_buf[BARD_INDEX_ENTRY_SIZE];
static uint8_t g_rect_cache[BADAPPLE_RECT_CACHE_COUNT * BARD_RECT5_SIZE];

/* ── Parsed header fields ── */
static uint16_t g_encoded_w, g_encoded_h;
static uint16_t g_display_x, g_display_y;
static uint16_t g_fps_num, g_fps_den;
static uint32_t g_frame_count;
static uint32_t g_index_offset;
static uint32_t g_payload_offset;
static uint32_t g_total_size;
static uint16_t g_flags;

/* ── Playback state ── */
static Badapple_video_source g_source = badapple_video_source_builtin;
static uint32_t g_start_ms = 0;
static uint32_t g_current_frame = 0;
static uint8_t g_active = 0;

/* ── LFS file handle (only for lfs_file source) ── */
#if BADAPPLE_VIDEO_USE_LFS_FILE
static lfs_file_t g_file = {0};
static uint8_t g_file_open = 0;
#endif

/* ── Little-endian read helpers ── */
static uint16_t rd_u16_le(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

static uint32_t rd_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ── Unified read from current source ── */
#if BADAPPLE_VIDEO_USE_BUILTIN
    #include "badapple_builtin_video.h"
#endif

static uint8_t badapple_read(uint32_t offset, void* dst, uint32_t size) {
    if (dst == NULL || size == 0) { return 0; }

    if (g_source == badapple_video_source_builtin) {
#if BADAPPLE_VIDEO_USE_BUILTIN
        if (offset + size > g_badapple_bard_builtin_size) { return 0; }
        memcpy(dst, &g_badapple_bard_builtin[offset], size);
        return 1;
#else
        (void)offset;
        return 0;
#endif
    }

#if BADAPPLE_VIDEO_USE_LFS_FILE
    /* LFS file source */
    if (!g_file_open) { return 0; }

    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    Storage_Lock();
    int seek_rc = lfs_file_seek(lfs, &g_file, (lfs_soff_t)offset, LFS_SEEK_SET);
    lfs_ssize_t read_rc = seek_rc < 0 ? (lfs_ssize_t)seek_rc : lfs_file_read(lfs, &g_file, dst, size);
    Storage_Unlock();

    return read_rc == (lfs_ssize_t)size;
#else
    (void)offset;
    return 0;
#endif
}

/* ── Header parsing ── */
static uint8_t badapple_parse_header(void) {
    if (!badapple_read(0, g_header_buf, BARD_HEADER_SIZE)) { return 0; }

    /* Magic */
    if (g_header_buf[0] != 'B' || g_header_buf[1] != 'A' || g_header_buf[2] != 'R' ||
        g_header_buf[3] != 'D') {
        return 0;
    }

    uint16_t version = rd_u16_le(&g_header_buf[4]);
    uint16_t header_sz = rd_u16_le(&g_header_buf[6]);

    if (version != 1 || header_sz != BARD_HEADER_SIZE) { return 0; }

    g_encoded_w = rd_u16_le(&g_header_buf[8]);
    g_encoded_h = rd_u16_le(&g_header_buf[10]);
    g_display_x = rd_u16_le(&g_header_buf[12]);
    g_display_y = rd_u16_le(&g_header_buf[14]);
    g_fps_num = rd_u16_le(&g_header_buf[16]);
    g_fps_den = rd_u16_le(&g_header_buf[18]);
    g_frame_count = rd_u32_le(&g_header_buf[20]);
    g_index_offset = rd_u32_le(&g_header_buf[24]);
    g_payload_offset = rd_u32_le(&g_header_buf[28]);
    g_total_size = rd_u32_le(&g_header_buf[32]);

    uint16_t rect_format = rd_u16_le(&g_header_buf[36]);
    g_flags = rd_u16_le(&g_header_buf[38]);

    if (rect_format != BARD_RECT_FORMAT_RECT5) { return 0; }
    if (g_encoded_w == 0 || g_encoded_h == 0 || g_frame_count == 0 || g_encoded_w > 256 ||
        g_encoded_h > 256) {
        return 0;
    }
    if (g_fps_den == 0) { g_fps_den = 1; }
    if (g_fps_num == 0) { g_fps_num = 24; }

    return 1;
}

/* ── Rect drawing ── */
static void badapple_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color_bit) {
    int32_t sx = (int32_t)g_display_x + (int32_t)x;
    int32_t sy = (int32_t)g_display_y + (int32_t)y;

    if (sx < 0 || sy < 0 || (int32_t)(sx + w) > SCREEN_WIDTH || (int32_t)(sy + h) > SCREEN_HEIGHT || w == 0 ||
        h == 0) {
        g_active = 0;
        return;
    }

    uint16_t color = color_bit ? BADAPPLE_COLOR_WHITE : BADAPPLE_COLOR_BLACK;
    Game_Graphics_Fill_Rect(g_lcd, sx, sy, (int32_t)w, (int32_t)h, color);
}

/* ── Decode and draw one frame ── */
static uint8_t badapple_decode_frame(uint32_t frame_idx) {
    if (frame_idx >= g_frame_count) { return 0; }

    /* Read frame index entry */
    uint32_t idx_addr = g_index_offset + frame_idx * BARD_INDEX_ENTRY_SIZE;
    if (!badapple_read(idx_addr, g_index_buf, BARD_INDEX_ENTRY_SIZE)) { return 0; }

    uint32_t frame_offset = rd_u32_le(&g_index_buf[0]);
    uint32_t frame_size = rd_u32_le(&g_index_buf[4]);

    if (frame_offset < g_payload_offset || frame_offset + frame_size > g_total_size || frame_size < 2) {
        g_active = 0;
        return 0;
    }

    /* Read rect_count */
    uint8_t count_buf[2];
    if (!badapple_read(frame_offset, count_buf, 2)) { return 0; }
    uint16_t rect_count = rd_u16_le(count_buf);
    uint32_t payload_start = frame_offset + 2u;

    if (frame_size != 2u + (uint32_t)rect_count * BARD_RECT5_SIZE) {
        g_active = 0;
        return 0;
    }

    /* Read and draw rects in chunks */
    uint16_t rects_left = rect_count;
    while (rects_left > 0) {
        uint16_t chunk_count = rects_left;
        if (chunk_count > BADAPPLE_RECT_CACHE_COUNT) { chunk_count = BADAPPLE_RECT_CACHE_COUNT; }
        uint32_t chunk_bytes = (uint32_t)chunk_count * BARD_RECT5_SIZE;

        if (!badapple_read(payload_start, g_rect_cache, chunk_bytes)) { return 0; }

        for (uint16_t i = 0; i < chunk_count; i++) {
            const uint8_t* p = &g_rect_cache[i * BARD_RECT5_SIZE];
            uint8_t rx = p[0];
            uint8_t ry = p[1];
            uint8_t rw = (uint8_t)(p[2] + 1u);
            uint8_t rh = (uint8_t)(p[3] + 1u);
            uint8_t color = p[4] & 1u;
            badapple_draw_rect(rx, ry, rw, rh, color);
            if (!g_active) { return 0; }
        }

        payload_start += chunk_bytes;
        rects_left -= chunk_count;
    }

    return 1;
}

/* ── Public: Init ── */
uint8_t Badapple_Video_Init(St7789* lcd, Badapple_video_source source) {
    if (lcd == NULL) { return 0; }

    /* Close any previously open LFS file */
#if BADAPPLE_VIDEO_USE_LFS_FILE
    if (g_file_open) {
        lfs_t* lfs = Storage_Get_Lfs();
        if (lfs != NULL) {
            Storage_Lock();
            lfs_file_close(lfs, &g_file);
            Storage_Unlock();
        }
        g_file_open = 0;
    }
#endif

    g_lcd = lcd;
    g_source = source;
    g_current_frame = 0;
    g_active = 0;

    /* Open LFS file if needed */
    if (source == badapple_video_source_lfs_file) {
#if BADAPPLE_VIDEO_USE_LFS_FILE
        if (!Storage_Is_Available()) { return 0; }
        lfs_t* lfs = Storage_Get_Lfs();
        if (lfs == NULL) { return 0; }

        Storage_Lock();
        int rc = lfs_file_open(lfs, &g_file, BADAPPLE_VIDEO_LFS_PATH, LFS_O_RDONLY);
        Storage_Unlock();
        if (rc < 0) { return 0; }
        g_file_open = 1;
#else
        return 0;
#endif
    }

    if (!badapple_parse_header()) {
#if BADAPPLE_VIDEO_USE_LFS_FILE
        if (g_file_open) {
            lfs_t* lfs = Storage_Get_Lfs();
            if (lfs != NULL) {
                Storage_Lock();
                lfs_file_close(lfs, &g_file);
                Storage_Unlock();
            }
            g_file_open = 0;
        }
#endif
        return 0;
    }

    /* Clear game area (below top bar, above bottom bar) */
    Game_Graphics_Fill_Rect(g_lcd, 0, 30, SCREEN_WIDTH, 270, BADAPPLE_COLOR_BLACK);

    g_start_ms = Bsp_Get_Tick_Ms();
    g_active = 1;
    return 1;
}

void Badapple_Video_Stop(void) {
    g_active = 0;

#if BADAPPLE_VIDEO_USE_LFS_FILE
    if (g_file_open) {
        lfs_t* lfs = Storage_Get_Lfs();
        if (lfs != NULL) {
            Storage_Lock();
            lfs_file_close(lfs, &g_file);
            Storage_Unlock();
        }
        g_file_open = 0;
    }
#endif
}

uint8_t Badapple_Video_Is_Active(void) { return g_active; }

/* ── Public: Update ── */
void Badapple_Video_Update(void) {
    if (!g_active || g_lcd == NULL) { return; }

    uint32_t now = Bsp_Get_Tick_Ms();
    uint32_t elapsed = now - g_start_ms;
    uint32_t target_frame = (uint32_t)(((uint64_t)elapsed * g_fps_num) / (1000u * (uint64_t)g_fps_den));

    if (target_frame > g_current_frame + BADAPPLE_CATCHUP_MAX) {
        Badapple_Video_Init(g_lcd, g_source);
        return;
    }

    uint8_t decoded_this_update = 0;
    while (g_active && g_current_frame <= target_frame && g_current_frame < g_frame_count) {
        if (!badapple_decode_frame(g_current_frame)) {
            g_active = 0;
            return;
        }
        g_current_frame++;
        decoded_this_update++;

        if (decoded_this_update >= BADAPPLE_MAX_FRAMES_PER_UPDATE) { break; }
    }

    if (g_active && g_current_frame >= g_frame_count) {
        if (g_flags & BARD_FLAG_LOOP) {
            Badapple_Video_Init(g_lcd, g_source);
        } else {
            g_active = 0;
        }
    }
}

/* ── Public: Is_Source_Available ── */
uint8_t Badapple_Video_Is_Source_Available(Badapple_video_source source) {
    if (source == badapple_video_source_builtin) {
#if BADAPPLE_VIDEO_USE_BUILTIN
        /* Quick check: verify magic bytes in builtin array */
        return (g_badapple_bard_builtin_size >= BARD_HEADER_SIZE && g_badapple_bard_builtin[0] == 'B' &&
                g_badapple_bard_builtin[1] == 'A' && g_badapple_bard_builtin[2] == 'R' &&
                g_badapple_bard_builtin[3] == 'D');
#else
        return 0;
#endif
    }

#if BADAPPLE_VIDEO_USE_LFS_FILE
    if (!Storage_Is_Available()) { return 0; }

    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    lfs_file_t tmp_file;
    Storage_Lock();
    int rc = lfs_file_open(lfs, &tmp_file, BADAPPLE_VIDEO_LFS_PATH, LFS_O_RDONLY);
    if (rc >= 0) { lfs_file_close(lfs, &tmp_file); }
    Storage_Unlock();
    return rc >= 0 ? 1 : 0;
#else
    return 0;
#endif
}
