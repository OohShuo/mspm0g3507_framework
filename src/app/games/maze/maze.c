#include "maze.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "buzzer.h"
#include "game_graphics.h"

/* ── 屏幕常量 ── */
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define BAR_H         12
#define BAR_BOT       298
#define PLAY_TOP      BAR_H
#define PLAY_BOT      BAR_BOT

/* ── 迷宫参数 ── */
#define MAZE_COLS     30
#define MAZE_ROWS     35
#define CELL_SIZE     8
#define MAZE_X0       0
#define MAZE_Y0       PLAY_TOP
#define MAZE_W        (MAZE_COLS * CELL_SIZE)   /* 240 */
#define MAZE_H        (MAZE_ROWS * CELL_SIZE)   /* 280 */

/* ── 墙壁位标记 ── */
#define WALL_N        0x01u
#define WALL_S        0x02u
#define WALL_W        0x04u
#define WALL_E        0x08u
#define WALL_ALL      0x0Fu
#define CELL_VISITED  0x80u   /* 生成时用 */
#define CELL_HAS_GEM  0x10u   /* 宝石标记 */

/* ── 颜色 RGB565 ── */
#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_CYAN    0x07ffu
#define COLOR_GREEN   0x07e0u
#define COLOR_RED     0xf800u
#define COLOR_YELLOW  0xffe0u
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0x4208u

/* ── 玩家/宝石/出口绘制尺寸 ── */
#define PLAYER_SIZE   6
#define GEM_SIZE      4
#define EXIT_SIZE     6

/* ── 类型定义 ── */
typedef enum { maze_state_ready, maze_state_playing, maze_state_over } Maze_state;

typedef struct {
    uint8_t col;
    uint8_t row;
} MazePos;

/* ── 静态全局变量 ── */
static Game_hardware g_hardware;
static Maze_state g_state;
static uint8_t g_maze[MAZE_ROWS][MAZE_COLS];   /* 墙壁 + 宝石 + 访问标记 */
static MazePos g_player;
static MazePos g_old_player;
static MazePos g_exit;
static uint32_t g_score;
static uint32_t g_gems_collected;
static uint32_t g_total_gems;
static uint32_t g_rand_state;
static uint32_t g_seed;

/* DFS 栈（生成迷宫用） */
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

static void play_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < PLAY_TOP) {
        h -= PLAY_TOP - y;
        y = PLAY_TOP;
    }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (y + h > PLAY_BOT) { h = PLAY_BOT - y; }
    if (w <= 0 || h <= 0) { return; }
    Game_Graphics_Fill_Rect(g_hardware.lcd, x, y, w, h, c);
}

/* ── 坐标转换 ── */

static int32_t cell_x(uint8_t col) { return MAZE_X0 + (int32_t)col * CELL_SIZE; }
static int32_t cell_y(uint8_t row) { return MAZE_Y0 + (int32_t)row * CELL_SIZE; }

/* ── 一次性的迷宫墙壁绘制 ── */

static void draw_maze_walls(void) {
    /* 先用墙壁色填满迷宫区域 */
    Game_Graphics_Fill_Rect(g_hardware.lcd, MAZE_X0, MAZE_Y0, MAZE_W, MAZE_H, COLOR_DARK);

    for (uint8_t r = 0; r < MAZE_ROWS; r++) {
        for (uint8_t c = 0; c < MAZE_COLS; c++) {
            const int32_t cx = cell_x(c);
            const int32_t cy = cell_y(r);
            const uint8_t walls = g_maze[r][c];

            /* 填充格内通道（6x6 黑色） */
            Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 1, cy + 1, CELL_SIZE - 2, CELL_SIZE - 2,
                COLOR_BLACK);

            /* 打通无墙方向的墙壁 */
            if (!(walls & WALL_N)) {
                Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 1, cy, CELL_SIZE - 2, 1, COLOR_BLACK);
            }
            if (!(walls & WALL_S)) {
                Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 1, cy + CELL_SIZE - 1, CELL_SIZE - 2, 1,
                    COLOR_BLACK);
            }
            if (!(walls & WALL_W)) {
                Game_Graphics_Fill_Rect(g_hardware.lcd, cx, cy + 1, 1, CELL_SIZE - 2, COLOR_BLACK);
            }
            if (!(walls & WALL_E)) {
                Game_Graphics_Fill_Rect(g_hardware.lcd, cx + CELL_SIZE - 1, cy + 1, 1, CELL_SIZE - 2,
                    COLOR_BLACK);
            }
        }
    }
}

/* ── 顶栏和底栏 ── */

static void draw_bars(void) {
    St7789* l = g_hardware.lcd;
    bar_fill(0, 0, SCREEN_WIDTH, BAR_H, COLOR_BLACK);
    Game_Graphics_Draw_Text(l, 2, 2, "MAZE", 1, COLOR_WHITE);
    bar_fill(0, BAR_H - 1, SCREEN_WIDTH, 1, COLOR_DARK);
    bar_fill(0, BAR_BOT, SCREEN_WIDTH, SCREEN_HEIGHT - BAR_BOT, COLOR_BLACK);
    bar_fill(0, BAR_BOT, SCREEN_WIDTH, 1, COLOR_DARK);
    Game_Graphics_Draw_Text(l, 56, BAR_BOT + 3, "HOLD TO BACK", 1, COLOR_GRAY);
}

static void draw_seed_display(void) {
    /* 在顶栏右侧显示种子号 */
    bar_fill(80, 2, SCREEN_WIDTH - 80, 8, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 80, 2, "SEED:", 1, COLOR_GRAY);
    Game_Graphics_Draw_U32(g_hardware.lcd, 120, 2, g_seed, 6, 1, COLOR_CYAN);
}

static void update_score_display(void) {
    /* 在顶栏显示宝石数 */
    bar_fill(2, 2, 74, 8, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 40, 2, "GEM:", 1, COLOR_YELLOW);
    Game_Graphics_Draw_U32(g_hardware.lcd, 66, 2, g_gems_collected, 2, 1, COLOR_YELLOW);
}

/* ── 随机数生成器（内联 LCG，沿用项目模式） ── */

static uint32_t fast_rand(void) {
    g_rand_state = g_rand_state * 1664525u + 1013904223u;
    return g_rand_state;
}

/* ── 迷宫生成（递归回溯 DFS，显式栈） ── */

static void generate_maze(void) {
    uint8_t r, c;

    /* 初始化：全部墙壁 + 清除访问/宝石标记 */
    for (r = 0; r < MAZE_ROWS; r++) {
        for (c = 0; c < MAZE_COLS; c++) {
            g_maze[r][c] = WALL_ALL;
        }
    }

    /* 从 (0,0) 开始 */
    g_maze[0][0] |= CELL_VISITED;
    g_stack[0].col = 0;
    g_stack[0].row = 0;
    g_stack_top = 1;

    while (g_stack_top > 0) {
        const MazePos current = g_stack[g_stack_top - 1];
        const uint8_t cr = current.row;
        const uint8_t cc = current.col;

        /* 收集未访问邻居方向 */
        uint8_t dirs[4];
        uint8_t dir_count = 0;

        if (cr > 0 && !(g_maze[cr - 1][cc] & CELL_VISITED)) { dirs[dir_count++] = 0; } /* N */
        if (cr + 1 < MAZE_ROWS && !(g_maze[cr + 1][cc] & CELL_VISITED)) {
            dirs[dir_count++] = 1;
        } /* S */
        if (cc > 0 && !(g_maze[cr][cc - 1] & CELL_VISITED)) { dirs[dir_count++] = 2; } /* W */
        if (cc + 1 < MAZE_COLS && !(g_maze[cr][cc + 1] & CELL_VISITED)) {
            dirs[dir_count++] = 3;
        } /* E */

        if (dir_count == 0) {
            /* 死胡同 — 弹栈 */
            g_stack_top--;
            continue;
        }

        /* 随机选方向 */
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

        /* 打通墙壁 */
        g_maze[cr][cc] &= (uint8_t)(~wall_bit);
        g_maze[nr][nc] &= (uint8_t)(~opp_wall);
        g_maze[nr][nc] |= CELL_VISITED;

        /* 压栈 */
        g_stack[g_stack_top].col = nc;
        g_stack[g_stack_top].row = nr;
        g_stack_top++;
    }

    /* 清除所有格子的 CELL_VISITED 标记 */
    for (r = 0; r < MAZE_ROWS; r++) {
        for (c = 0; c < MAZE_COLS; c++) {
            g_maze[r][c] &= (uint8_t)(~CELL_VISITED);
        }
    }
}

/* ── BFS 找出最短路径上的格子 ── */

static void mark_solution_path(void) {
    /* 方向：N S W E */
    static const int8_t dr[] = {-1, 1, 0, 0};
    static const int8_t dc[] = {0, 0, -1, 1};
    static const uint8_t w_bit[] = {WALL_N, WALL_S, WALL_W, WALL_E};

    /* BFS 距离数组 */
    static uint16_t dist[MAZE_ROWS][MAZE_COLS];
    uint8_t r, c;

    for (r = 0; r < MAZE_ROWS; r++) {
        for (c = 0; c < MAZE_COLS; c++) {
            dist[r][c] = 0xFFFFu;
        }
    }

    /* BFS 队列（复用 g_stack 内存 — g_stack 此时已空闲） */
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
            if (walls & w_bit[d]) { continue; } /* 有墙，不通 */
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

    /* 从出口回溯标记最短路径（bit6 临时用作"在最短路径上"标记） */
    if (dist[MAZE_ROWS - 1][MAZE_COLS - 1] == 0xFFFFu) { return; } /* 无路径 */

    r = MAZE_ROWS - 1;
    c = MAZE_COLS - 1;
    g_maze[r][c] |= 0x40u; /* 标记出口 */
    const uint16_t target = dist[r][c];
    (void)target;

    while (r != 0 || c != 0) {
        const uint16_t cur_dist = dist[r][c];
        uint8_t found = 0;
        for (uint8_t d = 0; d < 4; d++) {
            const int16_t nr16 = (int16_t)r + dr[d];
            const int16_t nc16 = (int16_t)c + dc[d];
            if (nr16 < 0 || nr16 >= MAZE_ROWS || nc16 < 0 || nc16 >= MAZE_COLS) { continue; }
            const uint8_t nr = (uint8_t)nr16;
            const uint8_t nc = (uint8_t)nc16;
            /* 检查反向墙壁 */
            const uint8_t opp_wall =
                (d == 0) ? WALL_S : (d == 1) ? WALL_N : (d == 2) ? WALL_E : WALL_W;
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

/* ── 放置宝石：死胡同和岔路上，不在最短路径上 ── */

static void place_gems(void) {
    g_total_gems = 0;

    for (uint8_t r = 0; r < MAZE_ROWS; r++) {
        for (uint8_t c = 0; c < MAZE_COLS; c++) {
            /* 清除宝石位和路径标记位（保留墙壁位） */
            const uint8_t walls = g_maze[r][c] & WALL_ALL;
            const uint8_t on_path = g_maze[r][c] & 0x40u;

            /* 数开口数（死胡同 = 只有 1 个开口） */
            uint8_t openings = 0;
            if (!(walls & WALL_N)) { openings++; }
            if (!(walls & WALL_S)) { openings++; }
            if (!(walls & WALL_W)) { openings++; }
            if (!(walls & WALL_E)) { openings++; }

            /* 死胡同且不在最短路径上 → 70% 概率放宝石 */
            uint8_t place = 0;
            if (!on_path) {
                if (openings == 1) {
                    place = (fast_rand() % 100) < 70;
                } else if (openings >= 3 && (fast_rand() % 100) < 20) {
                    place = 1; /* 放在岔路交叉口 */
                }
            }

            /* 不在起点或出口放宝石 */
            if ((r == 0 && c == 0) || (r == MAZE_ROWS - 1 && c == MAZE_COLS - 1)) {
                place = 0;
            }

            g_maze[r][c] = walls; /* 先仅保留墙壁 */
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

/* ── 绘制玩家 ── */

static void draw_player(const MazePos* pos, uint16_t color) {
    const int32_t cx = cell_x(pos->col);
    const int32_t cy = cell_y(pos->row);
    /* 6x6 居中于 8x8 格内 */
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 1, cy + 1, PLAYER_SIZE, PLAYER_SIZE, color);
}

/* ── 绘制宝石 ── */

static void draw_gem(const MazePos* pos) {
    const int32_t cx = cell_x(pos->col);
    const int32_t cy = cell_y(pos->row);
    /* 4x4 居中于格内 */
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 2, cy + 2, GEM_SIZE, GEM_SIZE, COLOR_YELLOW);
}

/* ── 绘制出口标记 ── */

static void draw_exit_marker(void) {
    const int32_t cx = cell_x(g_exit.col);
    const int32_t cy = cell_y(g_exit.row);
    Game_Graphics_Fill_Rect(g_hardware.lcd, cx + 1, cy + 1, EXIT_SIZE, EXIT_SIZE, COLOR_RED);
}

/* ── 绘制所有宝石（初始化用） ── */

static void draw_all_gems(void) {
    MazePos pos;
    for (pos.row = 0; pos.row < MAZE_ROWS; pos.row++) {
        for (pos.col = 0; pos.col < MAZE_COLS; pos.col++) {
            if (g_maze[pos.row][pos.col] & CELL_HAS_GEM) {
                draw_gem(&pos);
            }
        }
    }
}

/* ── 重启游戏 ── */

static void restart_game(void) {
    g_state = maze_state_ready;

    /* 取种子 */
    g_seed = Bsp_Get_Tick_Ms();
    g_rand_state = g_seed;

    /* 生成迷宫 */
    generate_maze();

    /* 放置宝石 */
    scatter_gems();

    /* 初始化玩家位置 */
    g_player.col = 0;
    g_player.row = 0;
    g_old_player = g_player;

    /* 出口位置 */
    g_exit.col = MAZE_COLS - 1;
    g_exit.row = MAZE_ROWS - 1;

    /* 分数 */
    g_score = 0;
    g_gems_collected = 0;

    /* 清屏并绘制 */
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_bars();
    draw_seed_display();
    draw_maze_walls();
    draw_all_gems();
    draw_exit_marker();
    draw_player(&g_player, COLOR_CYAN);
    update_score_display();

    /* 提示 */
    Game_Graphics_Draw_Text(g_hardware.lcd, 40, 140, "PUSH UP TO START", 1, COLOR_WHITE);
}

/* ── Init ── */

void Maze_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

/* ── 检查能否向某方向移动 ── */

static uint8_t can_move(uint8_t dir_enum) {
    const uint8_t walls = g_maze[g_player.row][g_player.col];
    /* dir_enum: 0=up(N), 1=left(W), 2=down(S), 3=right(E) */
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

    /* ── Game Over 状态 ── */
    if (g_state == maze_state_over) {
        if (input->direction_pressed) {
            Buzzer_Stop(g_hardware.buzzer);
            restart_game();
        }
        return game_result_running;
    }

    /* ── Ready 状态 ── */
    if (g_state == maze_state_ready) {
        if (input->direction_pressed) {
            g_state = maze_state_playing;
            /* 清除提示文字 */
            play_fill(20, 130, SCREEN_WIDTH - 40, 20, COLOR_BLACK);
            Buzzer_Play_Music(g_hardware.buzzer, music_idx_racing_theme, 1);
        }
        return game_result_running;
    }

    /* ── Playing 状态 ── */
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

        if (moved) {
            move_player(dir_enum);

            /* 擦除旧玩家位置 */
            draw_player(&g_old_player, COLOR_BLACK);

            /* 如果旧位置是出口标记，重画出口 */
            if (g_old_player.col == g_exit.col && g_old_player.row == g_exit.row) {
                draw_exit_marker();
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

            /* 检查是否到达出口 */
            if (g_player.col == g_exit.col && g_player.row == g_exit.row) {
                g_state = maze_state_over;
                g_score = g_gems_collected * 100u + 500u;
                Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_life_lost);
                Buzzer_Play_Music(g_hardware.buzzer, music_idx_defeat, 0);

                /* 显示 GAME OVER */
                play_fill(50, 148, 140, 9, COLOR_BLACK);
                Game_Graphics_Draw_Text(lcd, 60, 150, "GAME OVER", 1, COLOR_RED);
                play_fill(25, 168, 190, 9, COLOR_BLACK);
                Game_Graphics_Draw_Text(lcd, 30, 170, "PUSH TO RESTART", 1, COLOR_WHITE);
            }
        }
    }

    return game_result_running;
}

/* ── Score / Finished ── */

uint32_t Maze_Get_Score(void) { return g_score; }

uint8_t Maze_Is_Finished(void) { return g_state == maze_state_over; }
