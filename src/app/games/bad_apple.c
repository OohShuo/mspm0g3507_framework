#include <stddef.h>
#include <string.h>

#include "badapple_video.h"
#include "bsp_time.h"
#include "buzzer.h"
#include "game_graphics.h"
#include "game_registry.h"
#include "st7789.h"

#if FRAMEWORK_USE_LFS
    #include "lfs.h"
    #include "storage.h"
#endif

#define SCREEN_WIDTH             240
#define SCREEN_HEIGHT            320

#define COLOR_BLACK              0x0000u
#define COLOR_CYAN               0x07FFu

/* ── Tune streaming config ── */
#define BADAPPLE_TUNE_LFS_PATH   "/badapple.tune"
#define BADAPPLE_TUNE_NOTE_BYTES 8u
#define BADAPPLE_TUNE_GATE       100u /* gate percent */
#define BADAPPLE_TUNE_VOLUME     100u /* volume percent */
#define BADAPPLE_TUNE_OCTAVE     2u   /* frequency multiplier (1=none, 2=+1 octave) */

static St7789* g_lcd = NULL;
static Buzzer* g_buzzer = NULL;

#if FRAMEWORK_USE_LFS
static lfs_file_t g_tune_file = {0};
static uint8_t g_tune_file_open = 0;

static uint8_t g_tune_active = 0;
static Buzzer_note g_tune_current;
static uint32_t g_tune_note_start = 0;
static uint8_t g_tune_note_silenced = 0;
#endif

#if FRAMEWORK_USE_LFS

/* ── Read next note from LFS file ── */
static uint8_t tune_read_next_note(Buzzer_note* out_note) {
    if (!g_tune_file_open) { return 0; }

    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    Storage_Lock();
    lfs_ssize_t rc = lfs_file_read(lfs, &g_tune_file, out_note, BADAPPLE_TUNE_NOTE_BYTES);
    Storage_Unlock();

    return rc == (lfs_ssize_t)BADAPPLE_TUNE_NOTE_BYTES;
}

/* ── Tune cleanup ── */
static void tune_cleanup(void) {
    g_tune_active = 0;

    if (g_tune_file_open) {
        lfs_t* lfs = Storage_Get_Lfs();
        if (lfs != NULL) {
            Storage_Lock();
            lfs_file_close(lfs, &g_tune_file);
            Storage_Unlock();
        }
        g_tune_file_open = 0;
        memset(&g_tune_file, 0, sizeof(g_tune_file));
    }

    if (g_buzzer != NULL) { Buzzer_Note_Off(g_buzzer); }
}

/* ── Start playing the current note ── */
static void tune_note_on(void) {
    if (g_tune_current.frequency_hz == 0) { return; }
    uint16_t freq = g_tune_current.frequency_hz * BADAPPLE_TUNE_OCTAVE;
    Buzzer_Note_On(g_buzzer, freq, BADAPPLE_TUNE_VOLUME);
}

/* ── Tune update per frame ── */
static void tune_update(void) {
    if (!g_tune_active || g_buzzer == NULL) { return; }

    uint32_t now = Bsp_Get_Tick_Ms();
    uint32_t elapsed = now - g_tune_note_start;
    uint32_t duration = g_tune_current.duration_ms;
    if (duration == 0) { duration = 1; }
    uint32_t gate_time = duration * BADAPPLE_TUNE_GATE / 100u;

    /* Gate: silence note after gate_time */
    if (!g_tune_note_silenced && elapsed >= gate_time) {
        Buzzer_Note_Off(g_buzzer);
        g_tune_note_silenced = 1;
    }

    /* Note finished? */
    if (elapsed < duration) { return; }

    /* Advance to next note */
    if (!tune_read_next_note(&g_tune_current)) {
        /* EOF — loop back to start, resync to current time */
        lfs_t* lfs = Storage_Get_Lfs();
        if (lfs != NULL) {
            Storage_Lock();
            lfs_file_seek(lfs, &g_tune_file, 0, LFS_SEEK_SET);
            Storage_Unlock();
        }
        if (!tune_read_next_note(&g_tune_current)) {
            g_tune_active = 0;
            Buzzer_Note_Off(g_buzzer);
            return;
        }
        g_tune_note_start = now;
        g_tune_note_silenced = 0;
        tune_note_on();
        return;
    }

    g_tune_note_start += duration;
    g_tune_note_silenced = 0;
    tune_note_on();
}

#endif /* FRAMEWORK_USE_LFS */

static void bad_apple_init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    g_buzzer = hardware->buzzer;

    /* Play from SPI flash via LittleFS */
    if (!Badapple_Video_Init(g_lcd, badapple_video_source_lfs_file)) {
        Game_Graphics_Draw_Top_Bar(g_lcd, "BAD APPLE");
        Game_Graphics_Draw_Text(g_lcd, 30, 130, "NO BAD APPLE VIDEO", 2, COLOR_CYAN);
        return;
    }

    Game_Graphics_Draw_Top_Bar(g_lcd, "BAD APPLE");

#if FRAMEWORK_USE_LFS
    /* Init tune playback */
    if (g_buzzer != NULL && Storage_Is_Available()) {
        lfs_t* lfs = Storage_Get_Lfs();
        if (lfs != NULL) {
            Storage_Lock();
            int rc = lfs_file_open(lfs, &g_tune_file, BADAPPLE_TUNE_LFS_PATH, LFS_O_RDONLY);
            Storage_Unlock();
            if (rc >= 0) {
                g_tune_file_open = 1;
                if (tune_read_next_note(&g_tune_current)) {
                    g_tune_active = 1;
                    g_tune_note_start = Bsp_Get_Tick_Ms();
                    g_tune_note_silenced = 0;
                    tune_note_on();
                }
            }
        }
    }
#endif
}

static Game_result bad_apple_update(const Game_input* input) {
    if (input->back_requested) {
        Badapple_Video_Stop();
#if FRAMEWORK_USE_LFS
        tune_cleanup();
#endif
        return game_result_exit;
    }

    /* Video and audio run independently — each loops on its own timeline */
    Badapple_Video_Update();
#if FRAMEWORK_USE_LFS
    tune_update();
#endif

    return game_result_running;
}

static uint32_t bad_apple_get_score(void) { return 0; }

static void bad_apple_draw_icon(St7789* lcd, int32_t x, int32_t y) {
    x += 24;
    y += 22;
    /* White background */
    Game_Graphics_Fill_Rect(lcd, x - 16, y - 17, 32, 36, 0xffffu);

    /* Stem */
    Game_Graphics_Fill_Rect(lcd, x - 1, y - 15, 3, 7, 0x0000u);

    /* Apple body: two top lobes */
    Game_Graphics_Fill_Rect(lcd, x - 10, y - 11, 9, 3, 0x0000u);
    Game_Graphics_Fill_Rect(lcd, x + 2, y - 11, 10, 3, 0x0000u);

    /* Apple body: main silhouette */
    Game_Graphics_Fill_Rect(lcd, x - 13, y - 8, 27, 4, 0x0000u);
    Game_Graphics_Fill_Rect(lcd, x - 15, y - 4, 31, 5, 0x0000u);
    Game_Graphics_Fill_Rect(lcd, x - 15, y + 1, 31, 5, 0x0000u);
    Game_Graphics_Fill_Rect(lcd, x - 14, y + 6, 29, 4, 0x0000u);
    Game_Graphics_Fill_Rect(lcd, x - 12, y + 10, 25, 4, 0x0000u);
    Game_Graphics_Fill_Rect(lcd, x - 9, y + 14, 19, 3, 0x0000u);

    /* Top notch */
    Game_Graphics_Fill_Rect(lcd, x - 1, y - 11, 3, 3, 0xffffu);
}

const Game_descriptor game_bad_apple_entry = {
    .draw_icon = bad_apple_draw_icon,
    .name_color = 0x07ffu,
    .name = "?",
    .id = game_id_bad_apple,
    .control_hint = NULL,
    .info_text =
        "DESCRIPTION\nBad Apple shadow art video.\nBlack-and-white animation.\n\n"
        "CONTROLS\nB BACK",
    .is_game = 0,
    .init = bad_apple_init,
    .update = bad_apple_update,
    .get_score = bad_apple_get_score,
};
