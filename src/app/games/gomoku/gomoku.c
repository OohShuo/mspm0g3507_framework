#include "gomoku.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   320
#define HUD_HEIGHT      46

#define BOARD_SIZE      12
#define CELL_SIZE       16
#define BOARD_X         ((SCREEN_WIDTH - BOARD_SIZE * CELL_SIZE) / 2)
#define BOARD_Y         52

#define STONE_RADIUS    6
#define DAS_INITIAL     200u
#define DAS_REPEAT      60u

#define COLOR_BLACK     0x0000u
#define COLOR_WHITE     0xffffu
#define COLOR_CYAN      0x07ffu
#define COLOR_YELLOW    0xffe0u
#define COLOR_BOARD     0x8e1cu /* #E7C390 goban beige in BGR565 */
#define COLOR_GRID      0x632cu
#define COLOR_CURSOR    0xf800u /* bright red */
#define COLOR_PLAYER    0x0000u /* black stones */
#define COLOR_AI        0xffffu /* white stones */
#define COLOR_HIGHLIGHT 0xf800u
#define COLOR_GAMEOVER  0xf81fu
#define COLOR_WIN       0x07ffu

/* 0=empty, 1=player(black), 2=AI(white) */
typedef enum {
    gomoku_state_player,
    gomoku_state_ai,
    gomoku_state_over,
} Gomoku_state;

static Game_hardware g_hardware;
static uint8_t g_board[BOARD_SIZE][BOARD_SIZE];
static Gomoku_state g_state = gomoku_state_player;
static int8_t g_cursor_x = BOARD_SIZE / 2;
static int8_t g_cursor_y = BOARD_SIZE / 2;
static uint8_t g_winner = 0;
static uint32_t g_score = 0;
static uint8_t g_move_count = 0;
static uint8_t g_das_dir = 0;
static uint32_t g_das_time = 0;
static uint8_t g_das_fired = 0;
static uint8_t g_old_state = 99;

/* ---- helpers ---- */

static uint8_t in_bounds(int8_t x, int8_t y) { return (uint8_t)x < BOARD_SIZE && (uint8_t)y < BOARD_SIZE; }

static int32_t cell_x(int8_t col) { return BOARD_X + col * CELL_SIZE; }
static int32_t cell_y(int8_t row) { return BOARD_Y + row * CELL_SIZE; }

/* Restore a single cell to board background + grid lines.
   At edges, draws T or L instead of full cross since lines don't extend past the board. */
static void restore_cell(int8_t col, int8_t row) {
    const int32_t cx = cell_x(col);
    const int32_t cy = cell_y(row);
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx, cy, CELL_SIZE, CELL_SIZE, COLOR_BOARD);

    const int32_t mid_x = cx + CELL_SIZE / 2;
    const int32_t mid_y = cy + CELL_SIZE / 2;
    const uint8_t left_edge = (col == 0);
    const uint8_t right_edge = (col == BOARD_SIZE - 1);
    const uint8_t top_edge = (row == 0);
    const uint8_t bottom_edge = (row == BOARD_SIZE - 1);

    /* Vertical line: from top of cell to bottom, truncated at board edges */
    const int32_t vy1 = top_edge ? mid_y : cy;
    const int32_t vy2 = bottom_edge ? (mid_y + 1) : (cy + CELL_SIZE);
    Game_Graphics_Fill_Rect(g_hardware.lcd, mid_x, vy1, 1, vy2 - vy1, COLOR_GRID);

    /* Horizontal line: from left of cell to right, truncated at board edges */
    const int32_t hx1 = left_edge ? mid_x : cx;
    const int32_t hx2 = right_edge ? (mid_x + 1) : (cx + CELL_SIZE);
    Game_Graphics_Fill_Rect(g_hardware.lcd, hx1, mid_y, hx2 - hx1, 1, COLOR_GRID);
}

/* ---- win / pattern detection ---- */

static uint8_t check_win_at(int8_t x, int8_t y) {
    const uint8_t stone = g_board[y][x];
    if (stone == 0) { return 0; }
    static const int8_t dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (uint8_t d = 0; d < 4; d++) {
        uint8_t count = 1;
        for (uint8_t s = 0; s < 2; s++) {
            const int8_t dx = dirs[d][0] * (s ? -1 : 1);
            const int8_t dy = dirs[d][1] * (s ? -1 : 1);
            int8_t cx = (int8_t)(x + dx), cy = (int8_t)(y + dy);
            while (in_bounds(cx, cy) && g_board[cy][cx] == stone) {
                count++;
                cx = (int8_t)(cx + dx);
                cy = (int8_t)(cy + dy);
            }
        }
        if (count >= 5) { return 1; }
    }
    return 0;
}

static void scan_dir(
    int8_t x, int8_t y, int8_t dx, int8_t dy, uint8_t stone, uint8_t* out_count, uint8_t* out_open) {
    uint8_t count = 0;
    int8_t cx = (int8_t)(x + dx), cy = (int8_t)(y + dy);
    while (in_bounds(cx, cy) && g_board[cy][cx] == stone) {
        count++;
        cx = (int8_t)(cx + dx);
        cy = (int8_t)(cy + dy);
    }
    *out_open = in_bounds(cx, cy) && g_board[cy][cx] == 0;
    *out_count = count;
}

static int32_t score_position(int8_t x, int8_t y, uint8_t stone) {
    if (g_board[y][x] != 0) { return -1; }
    const uint8_t opponent = (uint8_t)(stone == 1 ? 2 : 1);

    g_board[y][x] = stone;
    if (check_win_at(x, y)) {
        g_board[y][x] = 0;
        return 100000;
    }

    g_board[y][x] = opponent;
    int32_t score = check_win_at(x, y) ? 50000 : 0;
    g_board[y][x] = stone;

    static const int8_t dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (uint8_t d = 0; d < 4; d++) {
        uint8_t c1, o1, c2, o2;
        scan_dir(x, y, dirs[d][0], dirs[d][1], stone, &c1, &o1);
        scan_dir(x, y, (int8_t)-dirs[d][0], (int8_t)-dirs[d][1], stone, &c2, &o2);
        const uint8_t total = (uint8_t)(c1 + c2);
        const uint8_t opens = (uint8_t)(o1 + o2);
        if (total >= 4 && opens >= 1) {
            score += 8000;
        } else if (total == 3 && opens == 2) {
            score += 3000;
        } else if (total == 3 && opens == 1) {
            score += 800;
        } else if (total == 2 && opens == 2) {
            score += 400;
        } else if (total == 2 && opens == 1) {
            score += 100;
        } else if (total == 1 && opens == 2) {
            score += 50;
        }

        scan_dir(x, y, dirs[d][0], dirs[d][1], opponent, &c1, &o1);
        scan_dir(x, y, (int8_t)-dirs[d][0], (int8_t)-dirs[d][1], opponent, &c2, &o2);
        const uint8_t ot = (uint8_t)(c1 + c2), oo = (uint8_t)(o1 + o2);
        if (ot >= 4 && oo >= 1) {
            score += 6000;
        } else if (ot == 3 && oo == 2) {
            score += 2000;
        }
    }

    g_board[y][x] = 0;
    const int8_t dcx = (int8_t)(x - BOARD_SIZE / 2);
    const int8_t dcy = (int8_t)(y - BOARD_SIZE / 2);
    score += (int32_t)(10 - ((dcx < 0 ? -dcx : dcx) + (dcy < 0 ? -dcy : dcy))) * 3;
    return score;
}

static void render_stone_at(int8_t col, int8_t row, uint8_t stone);

static void ai_move(void) {
    int32_t best_score = -1;
    int8_t best_x = BOARD_SIZE / 2, best_y = BOARD_SIZE / 2;
    for (int8_t y = 0; y < BOARD_SIZE; y++) {
        for (int8_t x = 0; x < BOARD_SIZE; x++) {
            const int32_t s = score_position(x, y, 2);
            if (s > best_score) {
                best_score = s;
                best_x = x;
                best_y = y;
            }
        }
    }
    if (best_score >= 0) {
        g_board[best_y][best_x] = 2;
        render_stone_at(best_x, best_y, 2);
        if (check_win_at(best_x, best_y)) {
            g_winner = 2;
            g_state = gomoku_state_over;
            g_score = 0;
            Buzzer_Play_Music(g_hardware.buzzer, music_idx_defeat, 0);
        }
    }
}

/* ---- incremental rendering ---- */

static void render_stone_at(int8_t col, int8_t row, uint8_t stone) {
    const uint16_t color = stone == 1 ? COLOR_PLAYER : COLOR_AI;
    const int32_t cx = cell_x(col) + CELL_SIZE / 2;
    const int32_t cy = cell_y(row) + CELL_SIZE / 2;
    for (int32_t dy = -STONE_RADIUS; dy <= STONE_RADIUS; dy++) {
        for (int32_t dx = -STONE_RADIUS; dx <= STONE_RADIUS; dx++) {
            if (dx * dx + dy * dy > STONE_RADIUS * STONE_RADIUS) { continue; }
            const uint16_t pc =
                (dx * dx + dy * dy > (STONE_RADIUS - 2) * (STONE_RADIUS - 2)) ? COLOR_BOARD : color;
            Game_Graphics_Fill_Rect(g_hardware.lcd, cx + dx, cy + dy, 1, 1, pc);
        }
    }
}

static void draw_cursor_at(int8_t col, int8_t row) {
    const int32_t cx = cell_x(col);
    const int32_t cy = cell_y(row);
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 2, cy + 2, CELL_SIZE - 4, 1, COLOR_CURSOR);
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 2, cy + CELL_SIZE - 3, CELL_SIZE - 4, 1, COLOR_CURSOR);
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 2, cy + 2, 1, CELL_SIZE - 4, COLOR_CURSOR);
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx + CELL_SIZE - 3, cy + 2, 1, CELL_SIZE - 4, COLOR_CURSOR);
}

static void move_cursor(int8_t nx, int8_t ny) {
    if (nx == g_cursor_x && ny == g_cursor_y) { return; }
    /* Erase old cursor: restore the cell (grid + board background) */
    restore_cell(g_cursor_x, g_cursor_y);
    /* If there was a stone here, redraw it */
    if (g_board[g_cursor_y][g_cursor_x] != 0) {
        render_stone_at(g_cursor_x, g_cursor_y, g_board[g_cursor_y][g_cursor_x]);
    }
    g_cursor_x = nx;
    g_cursor_y = ny;
    draw_cursor_at(nx, ny);
}

static void render_board_bg(void) {
    Game_Graphics_Fill_Rect(
        g_hardware.lcd, 0, HUD_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - HUD_HEIGHT, COLOR_BOARD);
    for (int8_t i = 0; i < BOARD_SIZE; i++) {
        const int32_t px = BOARD_X + i * CELL_SIZE + CELL_SIZE / 2;
        const int32_t py = BOARD_Y + i * CELL_SIZE + CELL_SIZE / 2;
        Game_Graphics_Fill_Rect(
            g_hardware.lcd, px, BOARD_Y + CELL_SIZE / 2, 1, (BOARD_SIZE - 1) * CELL_SIZE, COLOR_GRID);
        Game_Graphics_Fill_Rect(
            g_hardware.lcd, BOARD_X + CELL_SIZE / 2, py, (BOARD_SIZE - 1) * CELL_SIZE, 1, COLOR_GRID);
    }
}

/* Top bar drawn once on init — never changes */
static void render_hud_top(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, HUD_HEIGHT, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 4, 6, "GOMOKU", 2, COLOR_CYAN);
    Game_Graphics_Draw_Text(g_hardware.lcd, 120, 8, "YOU", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_hardware.lcd, 155, 8, "AI", 1, COLOR_YELLOW);
    /* Black stone with white border (visible on black HUD) */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 165, 8, 6, 6, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_hardware.lcd, 166, 9, 4, 4, COLOR_PLAYER);
    /* White stone with dark border */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 180, 8, 6, 6, COLOR_GRID);
    Game_Graphics_Fill_Rect(g_hardware.lcd, 181, 9, 4, 4, COLOR_AI);
}

/* Bottom status bar — only call on state transitions */
static void render_status(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 284, SCREEN_WIDTH, 36, COLOR_BLACK);
    if (g_state == gomoku_state_over) {
        Game_Graphics_Draw_Text(g_hardware.lcd, g_winner == 1 ? 67 : 73, 300,
            g_winner == 1 ? "YOU WIN" : "AI WINS", 1, g_winner == 1 ? COLOR_WIN : COLOR_GAMEOVER);
        Game_Graphics_Draw_Text(g_hardware.lcd, 37, 284, "PRESS RESTART", 1, COLOR_WHITE);
    } else {
        Game_Graphics_Draw_Text(g_hardware.lcd, 58, 300, "HOLD FOR MENU", 1, COLOR_WHITE);
    }
}

static void restart_game(void) {
    for (int8_t y = 0; y < BOARD_SIZE; y++) {
        for (int8_t x = 0; x < BOARD_SIZE; x++) { g_board[y][x] = 0; }
    }
    g_state = gomoku_state_player;
    g_cursor_x = BOARD_SIZE / 2;
    g_cursor_y = BOARD_SIZE / 2;
    g_winner = 0;
    g_score = 0;
    g_move_count = 0;
    g_das_dir = 0;
    g_das_fired = 0;
    g_old_state = 99;
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    render_hud_top();
    render_board_bg();
    draw_cursor_at(g_cursor_x, g_cursor_y);
    render_status();
    Buzzer_Play_Music(g_hardware.buzzer, music_idx_snake_theme, 1);
}

void Gomoku_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

Game_result Gomoku_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state == gomoku_state_over) {
        if (g_old_state != (uint8_t)g_state) {
            g_old_state = (uint8_t)g_state;
            render_status();
        }
        if (input->confirm_pressed) {
            Buzzer_Stop(g_hardware.buzzer);
            restart_game();
        }
        return game_result_running;
    }

    if (g_state == gomoku_state_player) {
        const uint32_t now = Bsp_Get_Tick_Ms();
        int8_t nx = g_cursor_x, ny = g_cursor_y;

        /* DAS cursor movement — works while direction is held */
        if (input->direction == game_direction_left || input->direction == game_direction_right ||
            input->direction == game_direction_up || input->direction == game_direction_down) {
            const uint8_t cur_dir = (uint8_t)(input->direction + 1);
            if (input->direction_pressed || cur_dir != g_das_dir) {
                g_das_dir = cur_dir;
                g_das_fired = 0;
                g_das_time = now;
                if (input->direction == game_direction_left && g_cursor_x > 0) {
                    nx--;
                } else if (input->direction == game_direction_right && g_cursor_x < BOARD_SIZE - 1) {
                    nx++;
                } else if (input->direction == game_direction_up && g_cursor_y > 0) {
                    ny--;
                } else if (input->direction == game_direction_down && g_cursor_y < BOARD_SIZE - 1) {
                    ny++;
                }
            } else {
                const uint32_t thresh = g_das_fired ? DAS_REPEAT : DAS_INITIAL;
                while (now - g_das_time >= thresh) {
                    g_das_time += thresh;
                    g_das_fired = 1;
                    if (input->direction == game_direction_left && nx > 0) {
                        nx--;
                    } else if (input->direction == game_direction_right && nx < BOARD_SIZE - 1) {
                        nx++;
                    } else if (input->direction == game_direction_up && ny > 0) {
                        ny--;
                    } else if (input->direction == game_direction_down && ny < BOARD_SIZE - 1) {
                        ny++;
                    } else {
                        break;
                    }
                }
            }
        } else {
            g_das_dir = 0;
            g_das_fired = 0;
        }

        move_cursor(nx, ny);

        /* Place stone */
        if (input->confirm_pressed && g_board[g_cursor_y][g_cursor_x] == 0) {
            /* Erase cursor before placing stone */
            restore_cell(g_cursor_x, g_cursor_y);
            g_board[g_cursor_y][g_cursor_x] = 1;
            render_stone_at(g_cursor_x, g_cursor_y, 1);
            draw_cursor_at(g_cursor_x, g_cursor_y); /* redraw cursor on top for visual feedback */
            g_move_count++;                         /* count player stones */

            if (check_win_at(g_cursor_x, g_cursor_y)) {
                g_winner = 1;
                g_state = gomoku_state_over;
                /* Piecewise score: fewer moves = higher.
                   ≤20→1000, 20-50→200, 50-100→50, 100-200→1, >200→1 */
                {
                    const uint8_t m = g_move_count;
                    if (m <= 20) {
                        g_score = 1000;
                    } else if (m <= 50) {
                        g_score = 1000u - (uint32_t)(m - 20) * 800u / 30u;
                    } else if (m <= 100) {
                        g_score = 200u - (uint32_t)(m - 50) * 150u / 50u;
                    } else if (m <= 200) {
                        g_score = 50u - (uint32_t)(m - 100) * 49u / 100u;
                    } else {
                        g_score = 1;
                    }
                }
                Buzzer_Play_Music(g_hardware.buzzer, music_idx_victory, 0);
            } else {
                g_state = gomoku_state_ai;
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_menu_select);
            }
            g_old_state = (uint8_t)g_state;
            render_status();
        }
    } else if (g_state == gomoku_state_ai) {
        /* Erase cursor while AI "thinks" */
        restore_cell(g_cursor_x, g_cursor_y);
        if (g_board[g_cursor_y][g_cursor_x] != 0) {
            render_stone_at(g_cursor_x, g_cursor_y, g_board[g_cursor_y][g_cursor_x]);
        }
        ai_move();
        g_old_state = (uint8_t)g_state;
        render_status();
        /* ai_move() may set g_state to over; only switch back if still playing */
        if (g_state == gomoku_state_ai) {
            g_state = gomoku_state_player;
            draw_cursor_at(g_cursor_x, g_cursor_y);
        }
    }
    return game_result_running;
}

uint32_t Gomoku_Get_Score(void) { return g_score; }

uint8_t Gomoku_Is_Finished(void) { return g_state == gomoku_state_over; }
