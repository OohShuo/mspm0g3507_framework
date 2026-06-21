#include "maze.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "buzzer.h"
#include "game_graphics.h"

/* ── 屏幕常量 ── */
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define PLAY_TOP      GAME_TOP_BAR_H
#define PLAY_BOT      GAME_AREA_BOTTOM

/* ── 迷宫参数（15×16 格，16px 每格 = 240×256） ── */
#define MAZE_COLS     15
#define MAZE_ROWS     16
#define CELL_SIZE     16
#define MAZE_X0       0
#define MAZE_Y0       PLAY_TOP
#define MAZE_W        (MAZE_COLS * CELL_SIZE) /* 240 */
#define MAZE_H        (MAZE_ROWS * CELL_SIZE) /* 272 */
#define GAP_Y         (MAZE_Y0 + MAZE_H)      /* 284，迷宫下方间隙起点 */

/* ── 墙壁位标记 ── */
#define WALL_N        0x01u
#define WALL_S        0x02u
#define WALL_W        0x04u
#define WALL_E        0x08u
#define WALL_ALL      0x0Fu
#define CELL_VISITED  0x80u /* 生成时用 */
#define CELL_HAS_GEM  0x10u /* 宝石标记 */

/* ── 颜色 RGB565 ── */
#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_GREEN   0x07e0u
#define COLOR_RED     0xf800u
#define COLOR_YELLOW  0xffe0u
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0xA514u

/* ── 玩家/宝石/出口绘制尺寸（在 16px 格内居中） ── */
#define PLAYER_SIZE   12
#define GEM_SIZE      8
#define EXIT_SIZE     12
#define PLAYER_OFF    ((CELL_SIZE - PLAYER_SIZE) / 2) /* 2 */
#define GEM_OFF       ((CELL_SIZE - GEM_SIZE) / 2)    /* 4 */
#define EXIT_OFF      ((CELL_SIZE - EXIT_SIZE) / 2)   /* 2 */

/* ── 类型定义 ── */
typedef enum { maze_state_ready, maze_state_playing, maze_state_over } Maze_state;

typedef struct {
    uint8_t col;
    uint8_t row;
} MazePos;

/* ── 静态全局变量 ── */
static Game_hardware g_hardware;
static Maze_state g_state;
static uint8_t g_maze[MAZE_ROWS][MAZE_COLS]; /* 墙壁 + 宝石 + 访问标记 */
static MazePos g_player;
static MazePos g_old_player;
static MazePos g_exit;
static uint32_t g_score;
static uint32_t g_gems_collected;
static uint32_t g_total_gems;
static uint32_t g_rand_state;
static uint32_t g_seed;

/* DFS 栈（生成迷宫用，同时被 BFS 复用为队列） */
static MazePos g_stack[MAZE_ROWS * MAZE_COLS];
static uint16_t g_stack_top;

/* ── 填充裁剪辅助 ── */

static void bar_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (y + h > SCREEN_HEIGHT) { h = SCREEN_HEIGHT - y; }
    if (w <= 0 || h <= 0) { return; }
    Game_Graphics_Fill_Rect(g_hardware.lcd, x, y, w, h, c);
}

/* ── 坐标转换 ── */

static int32_t cell_x(uint8_t col) { return MAZE_X0 + (int32_t)col * CELL_SIZE; }
static int32_t cell_y(uint8_t row) { return MAZE_Y0 + (int32_t)row * CELL_SIZE; }

/* ── 单格重绘（用于增量修复） ── */

static void redraw_cell(uint8_t col, uint8_t row) {
    const int32_t cx = cell_x(col);
    const int32_t cy = cell_y(row);
    const uint8_t walls = g_maze[row][col];

    /* 填充格内通道 */
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 1, cy + 1, CELL_SIZE - 2, CELL_SIZE - 2, COLOR_BLACK);

    /* 打通无墙方向的墙壁 */
    if (!(walls & WALL_N)) {
        Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 1, cy, CELL_SIZE - 2, 1, COLOR_BLACK);
    }
    if (!(walls & WALL_S)) {
        Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 1, cy + CELL_SIZE - 1, CELL_SIZE - 2, 1, COLOR_BLACK);
    }
    if (!(walls & WALL_W)) {
        Game_Graphics_Fill_Rect(g_hardware.lcd, cx, cy + 1, 1, CELL_SIZE - 2, COLOR_BLACK);
    }
    if (!(walls & WALL_E)) {
        Game_Graphics_Fill_Rect(g_hardware.lcd, cx + CELL_SIZE - 1, cy + 1, 1, CELL_SIZE - 2, COLOR_BLACK);
    }
}

/* ── 一次性的迷宫墙壁绘制 ── */

static void draw_maze_walls(void) {
    /* 先用墙壁色填满迷宫区域 */
    Game_Graphics_Fill_Rect(g_hardware.lcd, MAZE_X0, MAZE_Y0, MAZE_W, MAZE_H, COLOR_DARK);

    for (uint8_t r = 0; r < MAZE_ROWS; r++) {
        for (uint8_t c = 0; c < MAZE_COLS; c++) { redraw_cell(c, r); }
    }
}

static void draw_seed_display(void) {
    /* "SEED:123456"=66px */
    bar_fill(162, 2, 76, 8, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_Text(g_hardware.lcd, 167, 2, "SEED:", 1, COLOR_GRAY);
    Game_Graphics_Draw_U32(g_hardware.lcd, 203, 2, g_seed, 6, 1, COLOR_CYAN);
}

static void update_score_display(void) {
    /* "GEM:12"=36px */
    bar_fill(192, 12, 46, 8, GAME_BAR_COLOR_BG);
    Game_Graphics_Draw_Text(g_hardware.lcd, 197, 12, "GEM:", 1, COLOR_YELLOW);
    Game_Graphics_Draw_U32(g_hardware.lcd, 223, 12, g_gems_collected, 2, 1, COLOR_YELLOW);
}

/* ── 随机数生成器 ── */

static uint32_t fast_rand(void) {
    g_rand_state = g_rand_state * 1664525u + 1013904223u;
    return g_rand_state;
}

/* ── 迷宫生成（递归回溯 DFS，显式栈） ── */

static void generate_maze(void) {
    uint8_t r, c;

    for (r = 0; r < MAZE_ROWS; r++) {
        for (c = 0; c < MAZE_COLS; c++) { g_maze[r][c] = WALL_ALL; }
    }

    g_maze[0][0] |= CELL_VISITED;
    g_stack[0].col = 0;
    g_stack[0].row = 0;
    g_stack_top = 1;

    while (g_stack_top > 0) {
        const MazePos current = g_stack[g_stack_top - 1];
        const uint8_t cr = current.row;
        const uint8_t cc = current.col;

        uint8_t dirs[4];
        uint8_t dir_count = 0;

        if (cr > 0 && !(g_maze[cr - 1][cc] & CELL_VISITED)) { dirs[dir_count++] = 0; }
        if (cr + 1 < MAZE_ROWS && !(g_maze[cr + 1][cc] & CELL_VISITED)) { dirs[dir_count++] = 1; }
        if (cc > 0 && !(g_maze[cr][cc - 1] & CELL_VISITED)) { dirs[dir_count++] = 2; }
        if (cc + 1 < MAZE_COLS && !(g_maze[cr][cc + 1] & CELL_VISITED)) { dirs[dir_count++] = 3; }

        if (dir_count == 0) {
            g_stack_top--;
            continue;
        }

        const uint8_t pick = fast_rand() % dir_count;
        const uint8_t dir = dirs[pick];

        uint8_t nr = cr, nc = cc;
        uint8_t wall_bit = 0, opp_wall = 0;

        if (dir == 0) {
            nr = cr - 1;
            wall_bit = WALL_N;
            opp_wall = WALL_S;
        } else if (dir == 1) {
            nr = cr + 1;
            wall_bit = WALL_S;
            opp_wall = WALL_N;
        } else if (dir == 2) {
            nc = cc - 1;
            wall_bit = WALL_W;
            opp_wall = WALL_E;
        } else {
            nc = cc + 1;
            wall_bit = WALL_E;
            opp_wall = WALL_W;
        }

        g_maze[cr][cc] &= (uint8_t)(~wall_bit);
        g_maze[nr][nc] &= (uint8_t)(~opp_wall);
        g_maze[nr][nc] |= CELL_VISITED;

        g_stack[g_stack_top].col = nc;
        g_stack[g_stack_top].row = nr;
        g_stack_top++;
    }

    /* 强制终点 2×2 区域联通，防止全收集必经之路被堵 */
    {
        const uint8_t r1 = MAZE_ROWS - 2, r2 = MAZE_ROWS - 1;
        const uint8_t c1 = MAZE_COLS - 2, c2 = MAZE_COLS - 1;

        /* 上排水平 */
        g_maze[r1][c1] &= (uint8_t)(~WALL_E);
        g_maze[r1][c2] &= (uint8_t)(~WALL_W);
        /* 下排水平 */
        g_maze[r2][c1] &= (uint8_t)(~WALL_E);
        g_maze[r2][c2] &= (uint8_t)(~WALL_W);
        /* 左列垂直 */
        g_maze[r1][c1] &= (uint8_t)(~WALL_S);
        g_maze[r2][c1] &= (uint8_t)(~WALL_N);
        /* 右列垂直 */
        g_maze[r1][c2] &= (uint8_t)(~WALL_S);
        g_maze[r2][c2] &= (uint8_t)(~WALL_N);
    }

    for (r = 0; r < MAZE_ROWS; r++) {
        for (c = 0; c < MAZE_COLS; c++) { g_maze[r][c] &= (uint8_t)(~CELL_VISITED); }
    }
}

/* ── BFS 最短路径标记 ── */

static void mark_solution_path(void) {
    static const int8_t dr[] = {-1, 1, 0, 0};
    static const int8_t dc[] = {0, 0, -1, 1};
    static const uint8_t w_bit[] = {WALL_N, WALL_S, WALL_W, WALL_E};
    static uint16_t dist[MAZE_ROWS][MAZE_COLS];
    uint8_t r, c;

    for (r = 0; r < MAZE_ROWS; r++) {
        for (c = 0; c < MAZE_COLS; c++) { dist[r][c] = 0xFFFFu; }
    }

    MazePos* q = g_stack;
    uint16_t qh = 0, qt = 0;
    q[qt].col = 0;
    q[qt].row = 0;
    qt++;
    dist[0][0] = 0;

    while (qh < qt) {
        const MazePos cur = q[qh++];
        const uint8_t cr = cur.row;
        const uint8_t cc = cur.col;
        const uint8_t walls = g_maze[cr][cc];
        const uint16_t nd = (uint16_t)(dist[cr][cc] + 1);

        for (uint8_t d = 0; d < 4; d++) {
            if (walls & w_bit[d]) { continue; }
            const int16_t nr16 = (int16_t)cr + dr[d];
            const int16_t nc16 = (int16_t)cc + dc[d];
            if (nr16 < 0 || nr16 >= MAZE_ROWS || nc16 < 0 || nc16 >= MAZE_COLS) { continue; }
            const uint8_t nr = (uint8_t)nr16;
            const uint8_t nc = (uint8_t)nc16;
            if (nd < dist[nr][nc]) {
                dist[nr][nc] = nd;
                q[qt].row = nr;
                q[qt].col = nc;
                qt++;
            }
        }
    }

    if (dist[MAZE_ROWS - 1][MAZE_COLS - 1] == 0xFFFFu) { return; }

    r = MAZE_ROWS - 1;
    c = MAZE_COLS - 1;
    g_maze[r][c] |= 0x40u;

    while (r != 0 || c != 0) {
        const uint16_t cur_dist = dist[r][c];
        uint8_t found = 0;
        for (uint8_t d = 0; d < 4; d++) {
            const int16_t nr16 = (int16_t)r + dr[d];
            const int16_t nc16 = (int16_t)c + dc[d];
            if (nr16 < 0 || nr16 >= MAZE_ROWS || nc16 < 0 || nc16 >= MAZE_COLS) { continue; }
            const uint8_t nr = (uint8_t)nr16;
            const uint8_t nc = (uint8_t)nc16;
            const uint8_t opp_wall = (d == 0) ? WALL_S : (d == 1) ? WALL_N : (d == 2) ? WALL_E : WALL_W;
            if (!(g_maze[nr][nc] & opp_wall) && dist[nr][nc] == cur_dist - 1) {
                g_maze[nr][nc] |= 0x40u;
                r = nr;
                c = nc;
                found = 1;
                break;
            }
        }
        if (!found) { break; }
    }
}

/* ── 放置宝石 ── */

static void place_gems(void) {
    g_total_gems = 0;

    for (uint8_t r = 0; r < MAZE_ROWS; r++) {
        for (uint8_t c = 0; c < MAZE_COLS; c++) {
            const uint8_t walls = g_maze[r][c] & WALL_ALL;
            const uint8_t on_path = g_maze[r][c] & 0x40u;

            uint8_t openings = 0;
            if (!(walls & WALL_N)) { openings++; }
            if (!(walls & WALL_S)) { openings++; }
            if (!(walls & WALL_W)) { openings++; }
            if (!(walls & WALL_E)) { openings++; }

            uint8_t place = 0;
            if (!on_path) {
                if (openings == 1) {
                    place = (fast_rand() % 100) < 70;
                } else if (openings >= 3 && (fast_rand() % 100) < 20) {
                    place = 1;
                }
            }

            if ((r == 0 && c == 0) || (r == MAZE_ROWS - 1 && c == MAZE_COLS - 1)) { place = 0; }

            g_maze[r][c] = walls;
            if (place) {
                g_maze[r][c] |= CELL_HAS_GEM;
                g_total_gems++;
            }
        }
    }
}

static void scatter_gems(void) {
    mark_solution_path();
    place_gems();
}

/* ── 绘制玩家/宝石/出口 ── */

static void draw_player(const MazePos* pos, uint16_t color) {
    const int32_t cx = cell_x(pos->col) + PLAYER_OFF;
    const int32_t cy = cell_y(pos->row) + PLAYER_OFF;
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx, cy, PLAYER_SIZE, PLAYER_SIZE, color);
}

static void draw_gem_at(uint8_t col, uint8_t row) {
    const int32_t cx = cell_x(col) + GEM_OFF;
    const int32_t cy = cell_y(row) + GEM_OFF;
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx, cy, GEM_SIZE, GEM_SIZE, COLOR_YELLOW);
}

static void draw_exit_marker(void) {
    const int32_t cx = cell_x(g_exit.col) + EXIT_OFF;
    const int32_t cy = cell_y(g_exit.row) + EXIT_OFF;
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx, cy, EXIT_SIZE, EXIT_SIZE, COLOR_RED);
}

static void draw_all_gems(void) {
    for (uint8_t r = 0; r < MAZE_ROWS; r++) {
        for (uint8_t c = 0; c < MAZE_COLS; c++) {
            if (g_maze[r][c] & CELL_HAS_GEM) { draw_gem_at(c, r); }
        }
    }
}

/* ── 底部间隙中的提示文字 ── */

static void draw_prompt(const char* text) {
    /* 擦除间隙区域 */
    bar_fill(0, GAP_Y, SCREEN_WIDTH, PLAY_BOT - GAP_Y, COLOR_BLACK);
    if (text != NULL) { Game_Graphics_Draw_Text(g_hardware.lcd, 52, GAP_Y + 3, text, 1, COLOR_WHITE); }
}

/* ── 重启游戏 ── */

static void restart_game(void) {
    g_state = maze_state_ready;

    g_seed = Game_Runtime_Get_Tick_Ms();
    g_rand_state = g_seed;

    generate_maze();
    scatter_gems();

    g_player.col = 0;
    g_player.row = 0;
    g_old_player = g_player;

    g_exit.col = MAZE_COLS - 1;
    g_exit.row = MAZE_ROWS - 1;

    g_score = 0;
    g_gems_collected = 0;

    Game_Graphics_Clear_Game_Area(g_hardware.lcd);
    draw_seed_display();
    draw_maze_walls();
    draw_all_gems();
    draw_exit_marker();
    draw_player(&g_player, COLOR_CYAN);
    update_score_display();
    draw_prompt("PUSH TO START");
}

/* ── Init ── */

void Maze_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

/* ── 移动辅助 ── */

static uint8_t can_move(uint8_t dir_enum) {
    const uint8_t walls = g_maze[g_player.row][g_player.col];
    if (dir_enum == 0) { return !(walls & WALL_N); }
    if (dir_enum == 1) { return !(walls & WALL_W); }
    if (dir_enum == 2) { return !(walls & WALL_S); }
    return !(walls & WALL_E);
}

static void move_player(uint8_t dir_enum) {
    if (!can_move(dir_enum)) { return; }
    g_old_player = g_player;
    if (dir_enum == 0) {
        g_player.row--;
    } else if (dir_enum == 1) {
        g_player.col--;
    } else if (dir_enum == 2) {
        g_player.row++;
    } else {
        g_player.col++;
    }
}

/* ── Update ── */

Game_result Maze_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    St7789* lcd = g_hardware.lcd;

    /* ── Game Over ── */
    if (g_state == maze_state_over) {
        if (input->direction_pressed) { restart_game(); }
        return game_result_running;
    }

    /* ── Ready → Playing ── */
    if (g_state == maze_state_ready) {
        if (input->direction_pressed) {
            g_state = maze_state_playing;
            /* 清除间隙中的提示 */
            bar_fill(0, GAP_Y, SCREEN_WIDTH, PLAY_BOT - GAP_Y, COLOR_BLACK);
        }
        return game_result_running;
    }

    /* ── Playing ── */
    if (input->direction_pressed) {
        uint8_t dir_enum = 0;
        uint8_t moved = 0;

        if (input->direction == game_direction_up) {
            dir_enum = 0;
            moved = 1;
        } else if (input->direction == game_direction_left) {
            dir_enum = 1;
            moved = 1;
        } else if (input->direction == game_direction_down) {
            dir_enum = 2;
            moved = 1;
        } else if (input->direction == game_direction_right) {
            dir_enum = 3;
            moved = 1;
        }

        if (moved && can_move(dir_enum)) {
            move_player(dir_enum);
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_maze_move);

            /* 擦除旧玩家位置：重绘该格 */
            redraw_cell(g_old_player.col, g_old_player.row);
            /* 如果旧位置是出口，重画出口标记 */
            if (g_old_player.col == g_exit.col && g_old_player.row == g_exit.row) { draw_exit_marker(); }
            /* 如果旧位置有宝石，重画宝石 */
            if (g_maze[g_old_player.row][g_old_player.col] & CELL_HAS_GEM) {
                draw_gem_at(g_old_player.col, g_old_player.row);
            }

            /* 收集宝石 */
            if (g_maze[g_player.row][g_player.col] & CELL_HAS_GEM) {
                g_maze[g_player.row][g_player.col] &= (uint8_t)(~CELL_HAS_GEM);
                g_gems_collected++;
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_menu_select);
                update_score_display();
            }

            /* 画新玩家 */
            draw_player(&g_player, COLOR_CYAN);

            /* 到达出口 */
            if (g_player.col == g_exit.col && g_player.row == g_exit.row) {
                g_state = maze_state_over;
                g_score = g_gems_collected * 100u + 500u;
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_maze_goal);
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_defeat);
                Vib_Motor_Play_Effect(g_hardware.vib_motor, vib_effect_victory);

                /* 画 GAME OVER 在间隙中 */
                bar_fill(0, GAP_Y, SCREEN_WIDTH, PLAY_BOT - GAP_Y, COLOR_BLACK);
                Game_Graphics_Draw_Text(lcd, 40, GAP_Y + 3, "GAME OVER", 1, COLOR_RED);
                Game_Graphics_Draw_Text(lcd, 140, GAP_Y + 3, "PUSH RESTART", 1, COLOR_WHITE);
            } else {
                Vib_Motor_Play_Effect(g_hardware.vib_motor, vib_effect_menu_tick);
            }
        }
    }

    return game_result_running;
}

/* ── Score / Finished ── */

uint32_t Maze_Get_Score(void) { return g_score; }

uint8_t Maze_Is_Finished(void) { return g_state == maze_state_over; }
