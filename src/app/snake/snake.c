#include "snake.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define GRID_WIDTH   20
#define GRID_HEIGHT  24
#define CELL_SIZE    10
#define FIELD_X      ((SCREEN_WIDTH - GRID_WIDTH * CELL_SIZE) / 2)
#define FIELD_Y      54
#define MAX_SNAKE    160

#define START_MOVE_MS 220u
#define MIN_MOVE_MS    80u

#define COLOR_BLACK      0x0000u
#define COLOR_WHITE      0xffffu
#define COLOR_BORDER     0x07ffu
#define COLOR_SNAKE      0x07e0u
#define COLOR_HEAD       0xffe0u
#define COLOR_FOOD       0xf800u
#define COLOR_GAME_OVER  0xf81fu
#define COLOR_WIN        0x07ffu

typedef struct {
    int8_t x;
    int8_t y;
} Snake_position;

typedef enum {
    snake_state_playing,
    snake_state_over,
    snake_state_win,
} Snake_state;

static Game_hardware g_hardware;
static Snake_position g_body[MAX_SNAKE];
static Snake_position g_food;
static Game_direction g_direction = game_direction_right;
static Game_direction g_wanted_direction = game_direction_right;
static Snake_state g_state = snake_state_playing;
static uint16_t g_length = 0;
static uint32_t g_score = 0;
static uint32_t g_last_move = 0;
static uint32_t g_random_state = 0x51a9e37du;
static uint16_t g_cell_buffer[CELL_SIZE * CELL_SIZE];

static uint32_t random_next(void) {
    g_random_state = g_random_state * 1664525u + 1013904223u;
    return g_random_state;
}

static uint8_t positions_equal(Snake_position a, Snake_position b) {
    return a.x == b.x && a.y == b.y;
}

static uint8_t body_contains(Snake_position position, uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        if (positions_equal(g_body[i], position)) { return 1; }
    }
    return 0;
}

static uint8_t directions_are_opposite(Game_direction a, Game_direction b) {
    return (a == game_direction_up && b == game_direction_down) ||
           (a == game_direction_down && b == game_direction_up) ||
           (a == game_direction_left && b == game_direction_right) ||
           (a == game_direction_right && b == game_direction_left);
}

static Snake_position next_position(Snake_position position, Game_direction direction) {
    if (direction == game_direction_up) { position.y--; }
    if (direction == game_direction_left) { position.x--; }
    if (direction == game_direction_down) { position.y++; }
    if (direction == game_direction_right) { position.x++; }
    return position;
}

static void render_cell(Snake_position position) {
    if (position.x < 0 || position.x >= GRID_WIDTH || position.y < 0 ||
        position.y >= GRID_HEIGHT) {
        return;
    }

    uint16_t color = COLOR_BLACK;
    if (positions_equal(position, g_food)) { color = COLOR_FOOD; }

    for (uint16_t i = g_length; i > 0; i--) {
        if (!positions_equal(position, g_body[i - 1])) { continue; }
        color = i == 1 ? COLOR_HEAD : COLOR_SNAKE;
        break;
    }

    for (uint32_t i = 0; i < CELL_SIZE * CELL_SIZE; i++) { g_cell_buffer[i] = COLOR_BLACK; }
    for (int32_t y = 1; y < CELL_SIZE - 1; y++) {
        for (int32_t x = 1; x < CELL_SIZE - 1; x++) {
            g_cell_buffer[y * CELL_SIZE + x] = color;
        }
    }

    const int32_t screen_x = FIELD_X + position.x * CELL_SIZE;
    const int32_t screen_y = FIELD_Y + position.y * CELL_SIZE;
    St7789_Flush(g_hardware.lcd, screen_x, screen_y, screen_x + CELL_SIZE - 1,
        screen_y + CELL_SIZE - 1, (uint8_t*)g_cell_buffer, sizeof(g_cell_buffer));
}

static void render_hud(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, 42, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 12, 10, "SCORE", 2, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 92, 10, g_score, 5, 2, COLOR_BORDER);

    if (g_state == snake_state_over) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 55, 300, "PRESS RESTART", 1, COLOR_GAME_OVER);
    } else if (g_state == snake_state_win) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 79, 300, "YOU WIN", 1, COLOR_WIN);
    } else {
        Game_Graphics_Draw_Text(g_hardware.lcd, 58, 300, "HOLD FOR MENU", 1, COLOR_WHITE);
    }
}

static void render_full(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    Game_Graphics_Fill_Rect(
        g_hardware.lcd, FIELD_X - 2, FIELD_Y - 2, GRID_WIDTH * CELL_SIZE + 4, 2, COLOR_BORDER);
    Game_Graphics_Fill_Rect(g_hardware.lcd, FIELD_X - 2, FIELD_Y + GRID_HEIGHT * CELL_SIZE,
        GRID_WIDTH * CELL_SIZE + 4, 2, COLOR_BORDER);
    Game_Graphics_Fill_Rect(
        g_hardware.lcd, FIELD_X - 2, FIELD_Y - 2, 2, GRID_HEIGHT * CELL_SIZE + 4, COLOR_BORDER);
    Game_Graphics_Fill_Rect(g_hardware.lcd, FIELD_X + GRID_WIDTH * CELL_SIZE, FIELD_Y - 2, 2,
        GRID_HEIGHT * CELL_SIZE + 4, COLOR_BORDER);

    render_cell(g_food);
    for (uint16_t i = 0; i < g_length; i++) { render_cell(g_body[i]); }
    render_hud();
}

static void place_food(void) {
    const uint16_t available = GRID_WIDTH * GRID_HEIGHT - g_length;
    if (available == 0) {
        g_state = snake_state_win;
        return;
    }

    uint16_t target = (uint16_t)(random_next() % available);
    for (int8_t y = 0; y < GRID_HEIGHT; y++) {
        for (int8_t x = 0; x < GRID_WIDTH; x++) {
            const Snake_position candidate = {x, y};
            if (body_contains(candidate, g_length)) { continue; }
            if (target == 0) {
                g_food = candidate;
                return;
            }
            target--;
        }
    }
}

static void restart_game(void) {
    g_length = 4;
    g_score = 0;
    g_direction = game_direction_right;
    g_wanted_direction = game_direction_right;
    g_state = snake_state_playing;

    const int8_t start_x = GRID_WIDTH / 2;
    const int8_t start_y = GRID_HEIGHT / 2;
    for (uint16_t i = 0; i < g_length; i++) {
        g_body[i] = (Snake_position){(int8_t)(start_x - i), start_y};
    }

    place_food();
    g_last_move = Bsp_Get_Tick_Ms();
    render_full();
}

static uint32_t move_interval(void) {
    const uint32_t reduction = (g_score / 50u) * 12u;
    return reduction >= START_MOVE_MS - MIN_MOVE_MS ? MIN_MOVE_MS : START_MOVE_MS - reduction;
}

static void end_game(Snake_state state) {
    g_state = state;
    if (g_hardware.buzzer != NULL) {
        const Music_idx music = state == snake_state_win ? music_idx_victory : music_idx_death;
        Buzzer_Play(g_hardware.buzzer, &music_library[music], 0);
    }
    render_hud();
}

static void move_snake(void) {
    g_direction = g_wanted_direction;
    const Snake_position old_tail = g_body[g_length - 1];
    const Snake_position new_head = next_position(g_body[0], g_direction);
    const uint8_t ate_food = positions_equal(new_head, g_food);
    const uint16_t collision_count = ate_food ? g_length : (uint16_t)(g_length - 1);

    if (new_head.x < 0 || new_head.x >= GRID_WIDTH || new_head.y < 0 ||
        new_head.y >= GRID_HEIGHT || body_contains(new_head, collision_count)) {
        end_game(snake_state_over);
        return;
    }

    if (ate_food && g_length < MAX_SNAKE) { g_length++; }
    for (uint16_t i = g_length - 1; i > 0; i--) { g_body[i] = g_body[i - 1]; }
    g_body[0] = new_head;

    if (ate_food) {
        g_score += 10u;
        if (g_length >= MAX_SNAKE) {
            end_game(snake_state_win);
            return;
        }
        place_food();
        render_hud();
        render_cell(g_food);
    } else {
        render_cell(old_tail);
    }

    if (g_length > 1) { render_cell(g_body[1]); }
    render_cell(g_body[0]);
}

void Snake_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

Game_result Snake_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state != snake_state_playing) {
        if (input->confirm_pressed) {
            Buzzer_Stop(g_hardware.buzzer);
            restart_game();
        }
        return game_result_running;
    }

    if (input->direction_pressed && input->direction != game_direction_none &&
        !directions_are_opposite(input->direction, g_direction)) {
        g_wanted_direction = input->direction;
    }

    const uint32_t now = Bsp_Get_Tick_Ms();
    if (now - g_last_move >= move_interval()) {
        g_last_move = now;
        move_snake();
    }
    return game_result_running;
}
