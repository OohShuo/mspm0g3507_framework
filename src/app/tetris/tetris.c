#include "tetris.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   320

#define BOARD_COLS      10
#define BOARD_ROWS      20
#define CELL_SIZE       10
#define BOARD_X         ((SCREEN_WIDTH - BOARD_COLS * CELL_SIZE) / 2)
#define BOARD_Y         52
#define PREVIEW_X       170
#define PREVIEW_Y       56
#define PREVIEW_CELL    8

#define START_DROP_MS   600u
#define MIN_DROP_MS     80u
#define DAS_INITIAL_MS  180u
#define DAS_REPEAT_MS   50u

#define COLOR_BLACK     0x0000u
#define COLOR_WHITE     0xffffu
#define COLOR_BORDER    0x07ffu
#define COLOR_GHOST     0x2124u
#define COLOR_GAME_OVER 0xf81fu

/* clang-format off */
static const uint16_t g_piece_colors[] = {
    0x07ffu, /* I - cyan   */
    0xffe0u, /* O - yellow */
    0x8010u, /* T - purple */
    0x07e0u, /* S - green  */
    0xf800u, /* Z - red    */
    0x001fu, /* J - blue   */
    0xfd20u, /* L - orange */
};

/* 7 pieces × 4 rotations × 4 blocks × 2 coords (x, y) */
static const int8_t g_pieces[7][4][4][2] = {
    {{{0,0},{1,0},{2,0},{3,0}},{{0,0},{0,1},{0,2},{0,3}},{{0,0},{1,0},{2,0},{3,0}},{{0,0},{0,1},{0,2},{0,3}}},
    {{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}}},
    {{{0,0},{1,0},{2,0},{1,1}},{{0,0},{0,1},{0,2},{1,1}},{{1,0},{0,1},{1,1},{2,1}},{{0,0},{0,1},{0,2},{-1,1}}},
    {{{0,0},{1,0},{1,1},{2,1}},{{0,0},{0,1},{-1,1},{-1,2}},{{0,0},{1,0},{1,1},{2,1}},{{0,0},{0,1},{-1,1},{-1,2}}},
    {{{0,0},{1,0},{0,1},{-1,1}},{{0,0},{0,1},{1,1},{1,2}},{{0,0},{1,0},{0,1},{-1,1}},{{0,0},{0,1},{1,1},{1,2}}},
    {{{0,0},{0,1},{0,2},{1,2}},{{0,0},{1,0},{2,0},{0,1}},{{0,0},{1,0},{1,1},{1,2}},{{0,0},{0,1},{-1,1},{-2,1}}},
    {{{0,0},{0,1},{0,2},{1,0}},{{0,0},{1,0},{2,0},{2,1}},{{0,0},{1,0},{1,1},{1,2}},{{0,0},{0,1},{1,1},{2,1}}},
};
/* clang-format on */

typedef enum {
    tetris_state_playing,
    tetris_state_over,
} Tetris_state;

typedef struct {
    int8_t x;
    int8_t y;
    int8_t kind;
    int8_t rotation;
} Piece;

static Game_hardware g_hardware;
static uint8_t g_board[BOARD_ROWS][BOARD_COLS];
static Piece g_piece;
static Piece g_next;
static Tetris_state g_state = tetris_state_playing;
static uint32_t g_score = 0;
static uint16_t g_lines = 0;
static uint16_t g_level = 0;
static uint32_t g_last_drop = 0;
static uint32_t g_last_das = 0;
static uint8_t g_das_fired = 0;
static uint8_t g_das_direction = 0;
static uint32_t g_random_state = 0x2e71b864u;

static uint32_t random_next(void) {
    g_random_state = g_random_state * 1664525u + 1013904223u;
    return g_random_state;
}

static uint8_t collides(int8_t px, int8_t py, int8_t kind, int8_t rotation) {
    for (uint8_t i = 0; i < 4; i++) {
        const int8_t bx = (int8_t)(px + g_pieces[kind][rotation][i][0]);
        const int8_t by = (int8_t)(py + g_pieces[kind][rotation][i][1]);
        if (bx < 0 || bx >= BOARD_COLS || by >= BOARD_ROWS) { return 1; }
        if (by >= 0 && g_board[by][bx] != 0) { return 1; }
    }
    return 0;
}

static void lock_piece(void) {
    for (uint8_t i = 0; i < 4; i++) {
        const int8_t bx = (int8_t)(g_piece.x + g_pieces[g_piece.kind][g_piece.rotation][i][0]);
        const int8_t by = (int8_t)(g_piece.y + g_pieces[g_piece.kind][g_piece.rotation][i][1]);
        if (by >= 0 && by < BOARD_ROWS && bx >= 0 && bx < BOARD_COLS) {
            g_board[by][bx] = (uint8_t)(g_piece.kind + 1);
        }
    }
}

static void clear_lines(void) {
    uint8_t cleared = 0;
    for (int8_t row = BOARD_ROWS - 1; row >= 0; row--) {
        uint8_t full = 1;
        for (int8_t col = 0; col < BOARD_COLS; col++) {
            if (g_board[row][col] == 0) {
                full = 0;
                break;
            }
        }
        if (!full) { continue; }
        cleared++;
        for (int8_t r = row; r > 0; r--) {
            for (int8_t c = 0; c < BOARD_COLS; c++) { g_board[r][c] = g_board[r - 1][c]; }
        }
        for (int8_t c = 0; c < BOARD_COLS; c++) { g_board[0][c] = 0; }
        row++;
    }

    static const uint16_t scores[] = {0, 100, 300, 500, 800};
    if (cleared > 0) {
        g_score += (uint32_t)scores[cleared] * (g_level + 1);
        g_lines = (uint16_t)(g_lines + cleared);
        g_level = g_lines / 10u;
        Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_explosion);
    }
}

static int8_t g_ghost_y = -99;

static int8_t ghost_y(void) {
    int8_t gy = g_piece.y;
    while (!collides(g_piece.x, (int8_t)(gy + 1), g_piece.kind, g_piece.rotation)) { gy++; }
    return gy;
}

static void new_piece(void) {
    g_piece = g_next;
    g_piece.x = (int8_t)(BOARD_COLS / 2 - 1);
    g_piece.y = -2;
    g_next.kind = (int8_t)(random_next() % 7u);
    g_next.rotation = 0;
    g_next.x = 0;
    g_next.y = 0;
    g_last_drop = Bsp_Get_Tick_Ms();
}

static void render_cell(int32_t screen_x, int32_t screen_y, uint8_t color_index) {
    const uint16_t color = color_index == 0 ? COLOR_BLACK : g_piece_colors[color_index - 1];
    const int32_t inner = 1;
    Game_Graphics_Fill_Rect(g_hardware.lcd, screen_x, screen_y, CELL_SIZE, CELL_SIZE, COLOR_BLACK);
    Game_Graphics_Fill_Rect(g_hardware.lcd, (int32_t)(screen_x + inner), (int32_t)(screen_y + inner),
        CELL_SIZE - inner * 2, CELL_SIZE - inner * 2, color);
}

static void render_board(void) {
    for (int8_t row = 0; row < BOARD_ROWS; row++) {
        for (int8_t col = 0; col < BOARD_COLS; col++) {
            render_cell(BOARD_X + col * CELL_SIZE, BOARD_Y + row * CELL_SIZE, g_board[row][col]);
        }
    }
}

static void render_piece(Piece* piece, uint16_t color_override) {
    for (uint8_t i = 0; i < 4; i++) {
        const int8_t bx = (int8_t)(piece->x + g_pieces[piece->kind][piece->rotation][i][0]);
        const int8_t by = (int8_t)(piece->y + g_pieces[piece->kind][piece->rotation][i][1]);
        if (by < 0) { continue; }
        const int32_t sx = BOARD_X + bx * CELL_SIZE;
        const int32_t sy = BOARD_Y + by * CELL_SIZE;
        const uint16_t color = color_override != 0 ? color_override : g_piece_colors[piece->kind];
        const int32_t inner = 1;
        Game_Graphics_Fill_Rect(g_hardware.lcd, sx, sy, CELL_SIZE, CELL_SIZE, COLOR_BLACK);
        Game_Graphics_Fill_Rect(
            g_hardware.lcd, sx + inner, sy + inner, CELL_SIZE - inner * 2, CELL_SIZE - inner * 2, color);
    }
}

static void clear_ghost(void) {
    if (g_ghost_y < 0 || g_ghost_y == g_piece.y) { return; }
    Piece old = g_piece;
    old.y = g_ghost_y;
    render_piece(&old, COLOR_BLACK);
    g_ghost_y = -99;
}

static void render_ghost(void) {
    const int8_t gy = ghost_y();
    clear_ghost();
    if (gy == g_piece.y) { return; }
    Piece ghost = g_piece;
    ghost.y = gy;
    render_piece(&ghost, COLOR_GHOST);
    g_ghost_y = gy;
}

static void render_preview(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, PREVIEW_X - 4, PREVIEW_Y - 18, 52, 52, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, PREVIEW_X, PREVIEW_Y - 18, "NEXT", 1, COLOR_WHITE);

    const int8_t min_x = g_pieces[g_next.kind][0][0][0];
    const int32_t offset_x = PREVIEW_X + 8 - min_x * PREVIEW_CELL;
    const int32_t offset_y = PREVIEW_Y + 8;
    for (uint8_t i = 0; i < 4; i++) {
        const int8_t bx = g_pieces[g_next.kind][0][i][0];
        const int8_t by = g_pieces[g_next.kind][0][i][1];
        const int32_t sx = offset_x + bx * PREVIEW_CELL;
        const int32_t sy = offset_y + by * PREVIEW_CELL;
        const int32_t inner = 1;
        Game_Graphics_Fill_Rect(g_hardware.lcd, sx, sy, PREVIEW_CELL, PREVIEW_CELL, COLOR_BLACK);
        Game_Graphics_Fill_Rect(g_hardware.lcd, (int32_t)(sx + inner), (int32_t)(sy + inner),
            PREVIEW_CELL - inner * 2, PREVIEW_CELL - inner * 2, g_piece_colors[g_next.kind]);
    }
}

static void render_hud(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, 42, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 6, 6, "SCORE", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 54, 6, g_score, 6, 1, COLOR_BORDER);
    Game_Graphics_Draw_Text(g_hardware.lcd, 6, 22, "LINES", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 54, 22, g_lines, 3, 1, COLOR_BORDER);
    Game_Graphics_Draw_Text(g_hardware.lcd, 100, 6, "LEVEL", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 148, 6, g_level, 2, 1, 0xffe0u);

    if (g_state == tetris_state_over) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 51, 300, "PRESS RESTART", 1, COLOR_GAME_OVER);
    } else {
        Game_Graphics_Draw_Text(g_hardware.lcd, 58, 300, "HOLD FOR MENU", 1, COLOR_WHITE);
    }
}

static void render_full(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    Game_Graphics_Fill_Rect(
        g_hardware.lcd, BOARD_X - 2, BOARD_Y - 2, BOARD_COLS * CELL_SIZE + 4, 2, COLOR_BORDER);
    Game_Graphics_Fill_Rect(g_hardware.lcd, BOARD_X - 2, BOARD_Y + BOARD_ROWS * CELL_SIZE,
        BOARD_COLS * CELL_SIZE + 4, 2, COLOR_BORDER);
    Game_Graphics_Fill_Rect(
        g_hardware.lcd, BOARD_X - 2, BOARD_Y - 2, 2, BOARD_ROWS * CELL_SIZE + 4, COLOR_BORDER);
    Game_Graphics_Fill_Rect(g_hardware.lcd, BOARD_X + BOARD_COLS * CELL_SIZE, BOARD_Y - 2, 2,
        BOARD_ROWS * CELL_SIZE + 4, COLOR_BORDER);
    render_board();
    render_preview();
    render_piece(&g_piece, 0);
    render_ghost();
    render_hud();
}

static void restart_game(void) {
    for (int8_t r = 0; r < BOARD_ROWS; r++) {
        for (int8_t c = 0; c < BOARD_COLS; c++) { g_board[r][c] = 0; }
    }
    g_score = 0;
    g_lines = 0;
    g_level = 0;
    g_state = tetris_state_playing;
    g_das_fired = 0;

    g_next.kind = (int8_t)(random_next() % 7u);
    g_next.rotation = 0;
    new_piece();
    render_full();
    Buzzer_Play_Music(g_hardware.buzzer, music_idx_snake_theme, 1);
}

static uint32_t drop_interval(void) {
    const uint32_t reduction = g_level * 45u;
    return reduction >= START_DROP_MS - MIN_DROP_MS ? MIN_DROP_MS : START_DROP_MS - reduction;
}

void Tetris_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

Game_result Tetris_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state != tetris_state_playing) {
        if (input->confirm_pressed) {
            Buzzer_Stop(g_hardware.buzzer);
            restart_game();
        }
        return game_result_running;
    }

    const uint32_t now = Bsp_Get_Tick_Ms();

    /* Directional movement with DAS (Delayed Auto Shift) */
    if (input->direction == game_direction_left || input->direction == game_direction_right) {
        if (input->direction_pressed) {
            g_das_fired = 0;
            g_das_direction = (input->direction == game_direction_left) ? 1u : 2u;
            g_last_das = now;
            const int8_t dx = (input->direction == game_direction_left) ? -1 : 1;
            if (!collides((int8_t)(g_piece.x + dx), g_piece.y, g_piece.kind, g_piece.rotation)) {
                render_piece(&g_piece, COLOR_BLACK);
                render_ghost();
                g_piece.x = (int8_t)(g_piece.x + dx);
                render_ghost();
                render_piece(&g_piece, 0);
            }
        } else if (g_das_direction == 1u || g_das_direction == 2u) {
            const uint32_t das_threshold = g_das_fired ? DAS_REPEAT_MS : DAS_INITIAL_MS;
            while (now - g_last_das >= das_threshold) {
                g_last_das += das_threshold;
                g_das_fired = 1;
                const int8_t dx = (g_das_direction == 1u) ? -1 : 1;
                if (!collides((int8_t)(g_piece.x + dx), g_piece.y, g_piece.kind, g_piece.rotation)) {
                    render_piece(&g_piece, COLOR_BLACK);
                    render_ghost();
                    g_piece.x = (int8_t)(g_piece.x + dx);
                    render_ghost();
                    render_piece(&g_piece, 0);
                }
            }
        }
    } else {
        g_das_direction = 0;
        g_das_fired = 0;
    }

    if (input->direction_pressed && input->direction == game_direction_down) {
        if (!collides(g_piece.x, (int8_t)(g_piece.y + 1), g_piece.kind, g_piece.rotation)) {
            render_piece(&g_piece, COLOR_BLACK);
            g_piece.y = (int8_t)(g_piece.y + 1);
            render_ghost();
            render_piece(&g_piece, 0);
            g_last_drop = now;
        }
    }

    /* Button = rotate clockwise */
    if (input->confirm_pressed) {
        const int8_t new_rot = (int8_t)((g_piece.rotation + 1) % 4);
        /* Try normal rotation, then wall kicks left/right */
        int8_t kick = 0;
        if (collides(g_piece.x, g_piece.y, g_piece.kind, new_rot)) {
            if (!collides((int8_t)(g_piece.x - 1), g_piece.y, g_piece.kind, new_rot)) {
                kick = -1;
            } else if (!collides((int8_t)(g_piece.x + 1), g_piece.y, g_piece.kind, new_rot)) {
                kick = 1;
            } else if (!collides((int8_t)(g_piece.x - 2), g_piece.y, g_piece.kind, new_rot)) {
                kick = -2;
            } else if (!collides((int8_t)(g_piece.x + 2), g_piece.y, g_piece.kind, new_rot)) {
                kick = 2;
            } else {
                kick = -99;
            }
        }
        if (kick != -99) {
            render_piece(&g_piece, COLOR_BLACK);
            render_ghost();
            g_piece.rotation = new_rot;
            g_piece.x = (int8_t)(g_piece.x + kick);
            render_ghost();
            render_piece(&g_piece, 0);
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_menu_move);
        }
    }

    /* Gravity */
    if (now - g_last_drop >= drop_interval()) {
        g_last_drop = now;
        if (!collides(g_piece.x, (int8_t)(g_piece.y + 1), g_piece.kind, g_piece.rotation)) {
            render_piece(&g_piece, COLOR_BLACK);
            g_piece.y = (int8_t)(g_piece.y + 1);
            render_ghost();
            render_piece(&g_piece, 0);
            return game_result_running;
        }

        /* Lock piece */
        if (g_piece.y < 0) {
            g_state = tetris_state_over;
            Buzzer_Play_Music(g_hardware.buzzer, music_idx_defeat, 0);
            render_hud();
            return game_result_running;
        }

        lock_piece();
        clear_lines();
        render_board();
        g_ghost_y = -99;
        new_piece();
        render_preview();
        render_ghost();
        render_piece(&g_piece, 0);
        render_hud();

        /* Check if new piece immediately collides */
        if (collides(g_piece.x, g_piece.y, g_piece.kind, g_piece.rotation)) {
            g_state = tetris_state_over;
            Buzzer_Play_Music(g_hardware.buzzer, music_idx_defeat, 0);
            render_hud();
        }
    }
    return game_result_running;
}

uint32_t Tetris_Get_Score(void) { return g_score; }

uint8_t Tetris_Is_Finished(void) { return g_state != tetris_state_playing; }
