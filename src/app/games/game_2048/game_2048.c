#include "game_2048.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH   240
#define SCREEN_HEIGHT  320
#define HUD_HEIGHT     GAME_TOP_BAR_H

#define GRID_SIZE      4
#define CELL_SIZE      44
#define GAP            4
#define BOARD_W        (GRID_SIZE * CELL_SIZE + (GRID_SIZE - 1) * GAP)
#define BOARD_X        ((SCREEN_WIDTH - BOARD_W) / 2)
#define BOARD_Y        57

#define COLOR_BLACK    0x0000u
#define COLOR_WHITE    0xffffu
#define COLOR_CYAN     0x07ffu
#define COLOR_BG       0x630cu
#define COLOR_EMPTY    0x8410u
#define COLOR_GAMEOVER 0xf81fu
#define COLOR_WIN      0xffe0u

/* Tile colours by exponent (2^1=2 through 2^11=2048) */
static const uint16_t g_tile_bg[] = {
    0xdefbu, /* 2    */
    0xce79u, /* 4    */
    0xfda0u, /* 8    */
    0xfc40u, /* 16   */
    0xf8c0u, /* 32   */
    0xf800u, /* 64   */
    0xfec0u, /* 128  */
    0xfda0u, /* 256  */
    0xfd40u, /* 512  */
    0xfe20u, /* 1024 */
    0xff80u, /* 2048 */
};

typedef enum { state_playing, state_over, state_win } Game_state;

static Game_hardware g_hardware;
static uint16_t g_grid[GRID_SIZE][GRID_SIZE];
static Game_state g_state = state_playing;
static uint32_t g_score = 0;
static uint8_t g_moved = 0;
static uint8_t g_merged = 0;
static Game_rng g_rng;

static int32_t cell_x(uint8_t c) { return BOARD_X + c * (CELL_SIZE + GAP); }
static int32_t cell_y(uint8_t r) { return BOARD_Y + r * (CELL_SIZE + GAP); }

static uint16_t tile_bg(uint16_t val) {
    uint8_t exp = 0;
    while (val > 1u) {
        val >>= 1;
        exp++;
    }
    return exp > 0 && exp <= 11 ? g_tile_bg[exp - 1] : COLOR_EMPTY;
}

static void render_tile(uint8_t r, uint8_t c) {
    const int32_t x = cell_x(c);
    const int32_t y = cell_y(r);
    const uint16_t val = g_grid[r][c];
    const uint16_t bg = val ? tile_bg(val) : COLOR_EMPTY;
    Game_Graphics_Fill_Rect(g_hardware.lcd, x, y, CELL_SIZE, CELL_SIZE, bg);
    if (val == 0) { return; }

    /* Center the number */
    uint8_t digits = 1;
    uint16_t tmp = val;
    while (tmp >= 10u) {
        tmp /= 10u;
        digits++;
    }
    const int32_t tx = x + (int32_t)(CELL_SIZE - digits * 6) / 2;
    const int32_t ty = y + (CELL_SIZE - 7) / 2;
    Game_Graphics_Draw_U32(g_hardware.lcd, tx, ty, val, digits, 1, COLOR_BLACK);
}

static void render_board(void) {
    Game_Graphics_Fill_Rect(
        g_hardware.lcd, 0, HUD_HEIGHT, SCREEN_WIDTH, GAME_AREA_BOTTOM - HUD_HEIGHT, COLOR_BG);
    for (uint8_t r = 0; r < GRID_SIZE; r++) {
        for (uint8_t c = 0; c < GRID_SIZE; c++) { render_tile(r, c); }
    }
}

static void render_hud(void) {
    /* "SC:012345"=54px */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 174, 4, 64, 8, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_Text(g_hardware.lcd, 179, 4, "SC:", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 199, 4, g_score, 6, 1, COLOR_WIN);
}

static uint8_t spawn_tile(void) {
    uint8_t empty[GRID_SIZE * GRID_SIZE][2];
    uint8_t count = 0;
    for (uint8_t r = 0; r < GRID_SIZE; r++) {
        for (uint8_t c = 0; c < GRID_SIZE; c++) {
            if (g_grid[r][c] == 0) {
                empty[count][0] = r;
                empty[count][1] = c;
                count++;
            }
        }
    }
    if (count == 0) { return 0; }
    const uint8_t idx = (uint8_t)Game_Rng_Range(&g_rng, count);
    g_grid[empty[idx][0]][empty[idx][1]] = Game_Rng_Range(&g_rng, 10u) == 0u ? 4u : 2u;
    return 1;
}

/* Slide and merge one row left. Returns 1 if anything changed. */
static uint8_t slide_row(uint8_t r) {
    uint8_t changed = 0;
    /* Compact */
    uint8_t col = 0;
    for (uint8_t c = 0; c < GRID_SIZE; c++) {
        if (g_grid[r][c] == 0) { continue; }
        if (c != col) {
            g_grid[r][col] = g_grid[r][c];
            g_grid[r][c] = 0;
            changed = 1;
        }
        col++;
    }
    /* Merge */
    for (uint8_t c = 0; c + 1 < GRID_SIZE; c++) {
        if (g_grid[r][c] == 0) { continue; }
        if (g_grid[r][c] == g_grid[r][c + 1]) {
            g_grid[r][c] *= 2;
            g_score += g_grid[r][c];
            g_grid[r][c + 1] = 0;
            changed = 1;
            g_merged = 1;
        }
    }
    /* Compact again after merge */
    col = 0;
    for (uint8_t c = 0; c < GRID_SIZE; c++) {
        if (g_grid[r][c] == 0) { continue; }
        if (c != col) {
            g_grid[r][col] = g_grid[r][c];
            g_grid[r][c] = 0;
            changed = 1;
        }
        col++;
    }
    return changed;
}

static uint16_t g_saved[GRID_SIZE][GRID_SIZE];
static void save_grid(void) {
    for (uint8_t r = 0; r < GRID_SIZE; r++)
        for (uint8_t c = 0; c < GRID_SIZE; c++) g_saved[r][c] = g_grid[r][c];
}

static uint8_t slide(uint8_t dir /* 0=L 1=R 2=U 3=D */) {
    save_grid();
    g_merged = 0;
    uint8_t changed = 0;
    if (dir == 0) {
        for (uint8_t r = 0; r < GRID_SIZE; r++) changed |= slide_row(r);
    } else if (dir == 1) {
        /* Reverse each row, slide, reverse back */
        for (uint8_t r = 0; r < GRID_SIZE; r++) {
            for (uint8_t c = 0; c < GRID_SIZE / 2; c++) {
                const uint16_t t = g_grid[r][c];
                g_grid[r][c] = g_grid[r][GRID_SIZE - 1 - c];
                g_grid[r][GRID_SIZE - 1 - c] = t;
            }
        }
        for (uint8_t r = 0; r < GRID_SIZE; r++) changed |= slide_row(r);
        for (uint8_t r = 0; r < GRID_SIZE; r++) {
            for (uint8_t c = 0; c < GRID_SIZE / 2; c++) {
                const uint16_t t = g_grid[r][c];
                g_grid[r][c] = g_grid[r][GRID_SIZE - 1 - c];
                g_grid[r][GRID_SIZE - 1 - c] = t;
            }
        }
    } else if (dir == 2 || dir == 3) {
        /* Transpose, slide horizontally, transpose back */
        for (uint8_t r = 0; r < GRID_SIZE; r++)
            for (uint8_t c = r + 1; c < GRID_SIZE; c++) {
                const uint16_t t = g_grid[r][c];
                g_grid[r][c] = g_grid[c][r];
                g_grid[c][r] = t;
            }
        if (dir == 3) {
            /* Reverse rows for down direction */
            for (uint8_t r = 0; r < GRID_SIZE; r++) {
                for (uint8_t c = 0; c < GRID_SIZE / 2; c++) {
                    const uint16_t t = g_grid[r][c];
                    g_grid[r][c] = g_grid[r][GRID_SIZE - 1 - c];
                    g_grid[r][GRID_SIZE - 1 - c] = t;
                }
            }
        }
        for (uint8_t r = 0; r < GRID_SIZE; r++) changed |= slide_row(r);
        if (dir == 3) {
            for (uint8_t r = 0; r < GRID_SIZE; r++) {
                for (uint8_t c = 0; c < GRID_SIZE / 2; c++) {
                    const uint16_t t = g_grid[r][c];
                    g_grid[r][c] = g_grid[r][GRID_SIZE - 1 - c];
                    g_grid[r][GRID_SIZE - 1 - c] = t;
                }
            }
        }
        for (uint8_t r = 0; r < GRID_SIZE; r++)
            for (uint8_t c = r + 1; c < GRID_SIZE; c++) {
                const uint16_t t = g_grid[r][c];
                g_grid[r][c] = g_grid[c][r];
                g_grid[c][r] = t;
            }
    }
    return changed;
}

static uint8_t can_move(void) {
    for (uint8_t r = 0; r < GRID_SIZE; r++) {
        for (uint8_t c = 0; c < GRID_SIZE; c++) {
            if (g_grid[r][c] == 0) { return 1; }
            if (c + 1 < GRID_SIZE && g_grid[r][c] == g_grid[r][c + 1]) { return 1; }
            if (r + 1 < GRID_SIZE && g_grid[r][c] == g_grid[r + 1][c]) { return 1; }
        }
    }
    return 0;
}

static uint8_t has_2048(void) {
    for (uint8_t r = 0; r < GRID_SIZE; r++)
        for (uint8_t c = 0; c < GRID_SIZE; c++)
            if (g_grid[r][c] >= 2048u) return 1;
    return 0;
}

static void force_render(void) {
    for (uint8_t r = 0; r < GRID_SIZE; r++) {
        for (uint8_t c = 0; c < GRID_SIZE; c++) {
            if (g_grid[r][c] != g_saved[r][c]) render_tile(r, c);
        }
    }
    g_moved = 0;
}

static void restart_game(void) {
    Game_Rng_Seed(&g_rng, Game_Runtime_Get_Tick_Ms() ^ 0x3C8BF915u);
    for (uint8_t r = 0; r < GRID_SIZE; r++)
        for (uint8_t c = 0; c < GRID_SIZE; c++) g_grid[r][c] = 0;
    g_state = state_playing;
    g_score = 0;
    g_moved = 0;
    g_merged = 0;
    spawn_tile();
    spawn_tile();
    Game_Graphics_Clear_Game_Area(g_hardware.lcd);
    render_board();
    render_hud();
}

void Game_2048_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

Game_result Game_2048_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state == state_over) {
        if (input->confirm_pressed) { restart_game(); }
        return game_result_running;
    }

    /* Handle swipe */
    if (input->direction_pressed && input->direction != game_direction_none) {
        uint8_t d = 0;
        if (input->direction == game_direction_left) {
            d = 0;
        } else if (input->direction == game_direction_right) {
            d = 1;
        } else if (input->direction == game_direction_up) {
            d = 2;
        } else {
            d = 3;
        }

        g_moved = slide(d);
        if (g_moved) {
            spawn_tile();
            force_render();
            render_hud();
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_slide);

            if (has_2048() && g_state == state_playing) {
                g_state = state_win;
                render_hud();
            }
            if (!can_move()) {
                g_state = state_over;
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_defeat);
                Vib_Motor_Gpio_Play_Effect(g_hardware.vib_motor, vib_effect_defeat);
                render_hud();
            } else if (g_merged) {
                Vib_Motor_Gpio_Play_Effect(g_hardware.vib_motor, vib_effect_merge);
            }
        }
    }

    return game_result_running;
}

uint32_t Game_2048_Get_Score(void) { return g_score; }

uint8_t Game_2048_Is_Finished(void) { return g_state == state_over; }
