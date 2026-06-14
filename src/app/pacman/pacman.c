#include "pacman.h"

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_time.h"
#include "button.h"
#include "buzzer.h"
#include "joystick.h"
#include "st7789.h"
#include "task.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define MAP_WIDTH  15
#define MAP_HEIGHT 21
#define TILE_SIZE  12
#define MAP_X       ((SCREEN_WIDTH - MAP_WIDTH * TILE_SIZE) / 2)
#define MAP_Y       42

#define PLAYER_MOVE_MS 125u
#define GHOST_MOVE_MS  185u
#define POWER_TIME_MS  7000u
#define INPUT_THRESHOLD 0.42f

#define COLOR_BLACK       0x0000u
#define COLOR_WALL        0x001fu
#define COLOR_WALL_INNER  0x0008u
#define COLOR_PELLET      0xffdfu
#define COLOR_PACMAN      0xffe0u
#define COLOR_WHITE       0xffffu
#define COLOR_EYE         0x001fu
#define COLOR_FRIGHTENED  0x041fu
#define COLOR_HUD         0x07ffu
#define COLOR_GAME_OVER   0xf800u
#define COLOR_LEVEL_CLEAR 0x07e0u

typedef enum {
    direction_none,
    direction_up,
    direction_left,
    direction_down,
    direction_right,
} Direction;

typedef enum {
    game_state_playing,
    game_state_over,
    game_state_level_clear,
} Game_state;

typedef struct {
    int8_t x;
    int8_t y;
} Position;

typedef struct {
    Position pos;
    Position home;
    Direction direction;
    uint16_t color;
} Ghost;

static const char* const g_map_template[MAP_HEIGHT] = {
    "###############",
    "#......#......#",
    "#.###..#..###.#",
    "#o#.........#o#",
    "#.#.##.#.##.#.#",
    "#.............#",
    "###.#.###.#.###",
    "#...#.....#...#",
    "#.#.###.###.#.#",
    "#.#.........#.#",
    "#.###.GGG.###.#",
    "#.....G.G.....#",
    "#.###.GGG.###.#",
    "#.#.........#.#",
    "#.#.###.###.#.#",
    "#.............#",
    "###.#.###.#.###",
    "#o..#.....#..o#",
    "#.###..P..###.#",
    "#......#......#",
    "###############",
};

static const int8_t g_dir_x[] = {0, 0, -1, 0, 1};
static const int8_t g_dir_y[] = {0, -1, 0, 1, 0};

static St7789* g_lcd = NULL;
static Joystick* g_joystick = NULL;
static Buzzer* g_buzzer = NULL;
static Button* g_button_up = NULL;
static Button* g_button_left = NULL;
static Button* g_button_down = NULL;
static Button* g_button_right = NULL;
static Button* g_button_start = NULL;

static uint16_t g_tile_buffer[TILE_SIZE * TILE_SIZE];
static uint16_t g_line_buffer[SCREEN_WIDTH];
static uint8_t g_pellets[MAP_HEIGHT][MAP_WIDTH];

static Position g_player;
static Position g_player_home;
static Direction g_player_direction = direction_left;
static Direction g_wanted_direction = direction_left;
static Ghost g_ghosts[4];

static Game_state g_game_state = game_state_playing;
static uint32_t g_score = 0;
static uint16_t g_pellets_left = 0;
static uint8_t g_lives = 3;
static uint8_t g_level = 1;
static uint32_t g_last_player_move = 0;
static uint32_t g_last_ghost_move = 0;
static uint32_t g_power_until = 0;
static uint32_t g_random_state = 0x13579bdu;

static void render_cell(int8_t map_x, int8_t map_y);
static void render_full_map(void);
static void render_hud(void);

static uint8_t map_is_wall(int8_t x, int8_t y) {
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) { return 1; }
    return g_map_template[y][x] == '#';
}

static Direction direction_opposite(Direction direction) {
    switch (direction) {
        case direction_up:
            return direction_down;
        case direction_left:
            return direction_right;
        case direction_down:
            return direction_up;
        case direction_right:
            return direction_left;
        default:
            return direction_none;
    }
}

static uint8_t can_move(Position pos, Direction direction) {
    if (direction == direction_none) { return 0; }
    return !map_is_wall(pos.x + g_dir_x[direction], pos.y + g_dir_y[direction]);
}

static uint8_t power_active(void) {
    return (int32_t)(g_power_until - Bsp_Get_Tick_Ms()) > 0;
}

static uint32_t random_next(void) {
    g_random_state = g_random_state * 1664525u + 1013904223u;
    return g_random_state;
}

static void fill_rect(int32_t x, int32_t y, int32_t width, int32_t height, uint16_t color) {
    if (g_lcd == NULL || width <= 0 || height <= 0 || width > SCREEN_WIDTH) { return; }

    for (int32_t row = 0; row < height; row++) {
        for (int32_t col = 0; col < width; col++) { g_line_buffer[col] = color; }
        St7789_Flush(g_lcd, x, y + row, x + width - 1, y + row, (uint8_t*)g_line_buffer,
            (uint32_t)width * sizeof(uint16_t));
    }
}

static void tile_pixel(int32_t x, int32_t y, uint16_t color) {
    if (x < 0 || x >= TILE_SIZE || y < 0 || y >= TILE_SIZE) { return; }
    g_tile_buffer[y * TILE_SIZE + x] = color;
}

static void draw_pacman_sprite(Direction direction) {
    const int32_t center = TILE_SIZE / 2 - 1;
    const int32_t radius = TILE_SIZE / 2 - 1;

    for (int32_t y = 0; y < TILE_SIZE; y++) {
        for (int32_t x = 0; x < TILE_SIZE; x++) {
            const int32_t dx = x - center;
            const int32_t dy = y - center;
            if (dx * dx + dy * dy > radius * radius) { continue; }

            uint8_t mouth = 0;
            if (direction == direction_right) { mouth = dx >= 1 && (dy < 0 ? -dy : dy) <= dx; }
            if (direction == direction_left) { mouth = dx <= -1 && (dy < 0 ? -dy : dy) <= -dx; }
            if (direction == direction_up) { mouth = dy <= -1 && (dx < 0 ? -dx : dx) <= -dy; }
            if (direction == direction_down) { mouth = dy >= 1 && (dx < 0 ? -dx : dx) <= dy; }
            if (!mouth) { tile_pixel(x, y, COLOR_PACMAN); }
        }
    }
}

static void draw_ghost_sprite(uint16_t color) {
    for (int32_t y = 2; y < TILE_SIZE - 1; y++) {
        for (int32_t x = 2; x < TILE_SIZE - 2; x++) {
            const int32_t dx = x - (TILE_SIZE / 2 - 1);
            const int32_t dy = y - 5;
            const uint8_t head = y <= 6 && dx * dx + dy * dy <= 16;
            const uint8_t body = y >= 5;
            if (head || body) { tile_pixel(x, y, color); }
        }
    }

    tile_pixel(4, 5, COLOR_WHITE);
    tile_pixel(7, 5, COLOR_WHITE);
    tile_pixel(4, 6, COLOR_EYE);
    tile_pixel(7, 6, COLOR_EYE);
    tile_pixel(3, TILE_SIZE - 2, COLOR_BLACK);
    tile_pixel(7, TILE_SIZE - 2, COLOR_BLACK);
}

static void render_cell(int8_t map_x, int8_t map_y) {
    if (map_x < 0 || map_x >= MAP_WIDTH || map_y < 0 || map_y >= MAP_HEIGHT) { return; }

    const uint8_t wall = map_is_wall(map_x, map_y);
    for (uint32_t i = 0; i < TILE_SIZE * TILE_SIZE; i++) {
        g_tile_buffer[i] = wall ? COLOR_WALL : COLOR_BLACK;
    }

    if (wall) {
        for (int32_t y = 2; y < TILE_SIZE - 2; y++) {
            for (int32_t x = 2; x < TILE_SIZE - 2; x++) {
                tile_pixel(x, y, COLOR_WALL_INNER);
            }
        }
    } else if (g_pellets[map_y][map_x] != 0) {
        const int32_t center = TILE_SIZE / 2;
        const int32_t radius = g_pellets[map_y][map_x] == 2 ? 2 : 1;
        for (int32_t y = center - radius; y <= center + radius; y++) {
            for (int32_t x = center - radius; x <= center + radius; x++) {
                tile_pixel(x, y, COLOR_PELLET);
            }
        }
    }

    for (uint32_t i = 0; i < 4; i++) {
        if (g_ghosts[i].pos.x == map_x && g_ghosts[i].pos.y == map_y) {
            draw_ghost_sprite(power_active() ? COLOR_FRIGHTENED : g_ghosts[i].color);
        }
    }

    if (g_player.x == map_x && g_player.y == map_y) { draw_pacman_sprite(g_player_direction); }

    const int32_t screen_x = MAP_X + map_x * TILE_SIZE;
    const int32_t screen_y = MAP_Y + map_y * TILE_SIZE;
    St7789_Flush(g_lcd, screen_x, screen_y, screen_x + TILE_SIZE - 1, screen_y + TILE_SIZE - 1,
        (uint8_t*)g_tile_buffer, sizeof(g_tile_buffer));
}

static const uint8_t g_digit_font[10][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7},
    {0x2, 0x6, 0x2, 0x2, 0x7},
    {0x7, 0x1, 0x7, 0x4, 0x7},
    {0x7, 0x1, 0x7, 0x1, 0x7},
    {0x5, 0x5, 0x7, 0x1, 0x1},
    {0x7, 0x4, 0x7, 0x1, 0x7},
    {0x7, 0x4, 0x7, 0x5, 0x7},
    {0x7, 0x1, 0x1, 0x1, 0x1},
    {0x7, 0x5, 0x7, 0x5, 0x7},
    {0x7, 0x5, 0x7, 0x1, 0x7},
};

static void draw_digit(int32_t x, int32_t y, uint8_t digit, uint16_t color) {
    if (digit > 9) { return; }
    for (int32_t row = 0; row < 5; row++) {
        for (int32_t col = 0; col < 3; col++) {
            if (g_digit_font[digit][row] & (1u << (2 - col))) {
                fill_rect(x + col * 2, y + row * 2, 2, 2, color);
            }
        }
    }
}

static void render_hud(void) {
    fill_rect(0, 0, SCREEN_WIDTH, 34, COLOR_BLACK);

    uint32_t score = g_score;
    for (int32_t i = 5; i >= 0; i--) {
        draw_digit(8 + i * 9, 9, (uint8_t)(score % 10u), COLOR_HUD);
        score /= 10u;
    }

    draw_digit(102, 9, g_level % 10u, COLOR_WHITE);

    for (uint8_t i = 0; i < g_lives; i++) {
        fill_rect(174 + i * 18, 10, 10, 10, COLOR_PACMAN);
    }

    if (g_game_state == game_state_over) {
        fill_rect(70, 27, 100, 5, COLOR_GAME_OVER);
    } else if (g_game_state == game_state_level_clear) {
        fill_rect(70, 27, 100, 5, COLOR_LEVEL_CLEAR);
    }
}

static void render_full_map(void) {
    fill_rect(0, 34, SCREEN_WIDTH, SCREEN_HEIGHT - 34, COLOR_BLACK);
    for (int8_t y = 0; y < MAP_HEIGHT; y++) {
        for (int8_t x = 0; x < MAP_WIDTH; x++) { render_cell(x, y); }
    }
    render_hud();
}

static void reset_actor_positions(void) {
    g_player = g_player_home;
    g_player_direction = direction_left;
    g_wanted_direction = direction_left;
    for (uint32_t i = 0; i < 4; i++) {
        g_ghosts[i].pos = g_ghosts[i].home;
        g_ghosts[i].direction = (i & 1u) ? direction_right : direction_left;
    }
}

static void load_level(void) {
    static const uint16_t ghost_colors[4] = {0xf800u, 0xf81fu, 0x07ffu, 0xfd20u};
    uint8_t ghost_index = 0;

    memset(g_pellets, 0, sizeof(g_pellets));
    g_pellets_left = 0;

    for (int8_t y = 0; y < MAP_HEIGHT; y++) {
        for (int8_t x = 0; x < MAP_WIDTH; x++) {
            const char cell = g_map_template[y][x];
            if (cell == '.' || cell == 'o') {
                g_pellets[y][x] = cell == 'o' ? 2 : 1;
                g_pellets_left++;
            } else if (cell == 'P') {
                g_player_home = (Position){x, y};
            } else if (cell == 'G' && ghost_index < 4) {
                g_ghosts[ghost_index].home = (Position){x, y};
                g_ghosts[ghost_index].color = ghost_colors[ghost_index];
                ghost_index++;
            }
        }
    }

    reset_actor_positions();
    g_power_until = 0;
    g_game_state = game_state_playing;
    g_last_player_move = Bsp_Get_Tick_Ms();
    g_last_ghost_move = g_last_player_move;
}

static void restart_game(void) {
    g_score = 0;
    g_lives = 3;
    g_level = 1;
    load_level();
    render_full_map();
    if (g_buzzer != NULL) { Buzzer_Stop(g_buzzer); }
}

static Direction read_direction(void) {
    if (Button_Get_State(g_button_up) == button_state_down) { return direction_up; }
    if (Button_Get_State(g_button_left) == button_state_down) { return direction_left; }
    if (Button_Get_State(g_button_down) == button_state_down) { return direction_down; }
    if (Button_Get_State(g_button_right) == button_state_down) { return direction_right; }

    if (g_joystick == NULL) { return direction_none; }
    const float x = g_joystick->x_value;
    const float y = g_joystick->y_value;
    const float abs_x = x < 0.0f ? -x : x;
    const float abs_y = y < 0.0f ? -y : y;

    if (abs_x < INPUT_THRESHOLD && abs_y < INPUT_THRESHOLD) { return direction_none; }
    if (abs_x > abs_y) { return x < 0.0f ? direction_left : direction_right; }
    return y < 0.0f ? direction_down : direction_up;
}

static void collect_pellet(void) {
    const uint8_t pellet = g_pellets[g_player.y][g_player.x];
    if (pellet == 0) { return; }

    g_pellets[g_player.y][g_player.x] = 0;
    g_pellets_left--;
    g_score += pellet == 2 ? 50u : 10u;
    if (pellet == 2) { g_power_until = Bsp_Get_Tick_Ms() + POWER_TIME_MS; }
    render_hud();

    if (g_pellets_left == 0) {
        g_game_state = game_state_level_clear;
        if (g_buzzer != NULL) { Buzzer_Play(g_buzzer, &music_library[music_idx_victory], 0); }
        render_hud();
    }
}

static void lose_life(void) {
    if (g_lives > 0) { g_lives--; }
    if (g_buzzer != NULL) { Buzzer_Play(g_buzzer, &music_library[music_idx_death], 0); }

    if (g_lives == 0) {
        g_game_state = game_state_over;
        render_hud();
        return;
    }

    reset_actor_positions();
    g_last_player_move = Bsp_Get_Tick_Ms();
    g_last_ghost_move = g_last_player_move;
    render_full_map();
}

static uint8_t check_collisions(void) {
    for (uint32_t i = 0; i < 4; i++) {
        if (g_ghosts[i].pos.x != g_player.x || g_ghosts[i].pos.y != g_player.y) { continue; }

        if (power_active()) {
            const Position old = g_ghosts[i].pos;
            g_ghosts[i].pos = g_ghosts[i].home;
            g_score += 200u;
            render_cell(old.x, old.y);
            render_cell(g_ghosts[i].pos.x, g_ghosts[i].pos.y);
            render_hud();
        } else {
            lose_life();
        }
        return 1;
    }
    return 0;
}

static void move_player(void) {
    if (can_move(g_player, g_wanted_direction)) { g_player_direction = g_wanted_direction; }
    if (!can_move(g_player, g_player_direction)) { return; }

    const Position old = g_player;
    g_player.x += g_dir_x[g_player_direction];
    g_player.y += g_dir_y[g_player_direction];

    collect_pellet();
    render_cell(old.x, old.y);
    render_cell(g_player.x, g_player.y);
    if (g_game_state != game_state_playing) { return; }
    (void)check_collisions();
}

static int32_t ghost_direction_score(uint32_t ghost_index, Direction direction) {
    const int32_t next_x = g_ghosts[ghost_index].pos.x + g_dir_x[direction];
    const int32_t next_y = g_ghosts[ghost_index].pos.y + g_dir_y[direction];
    int32_t target_x = g_player.x;
    int32_t target_y = g_player.y;

    if (ghost_index == 1) {
        target_x += g_dir_x[g_player_direction] * 3;
        target_y += g_dir_y[g_player_direction] * 3;
    } else if (ghost_index == 2) {
        target_x = (int32_t)(random_next() % MAP_WIDTH);
        target_y = (int32_t)(random_next() % MAP_HEIGHT);
    } else if (ghost_index == 3) {
        const int32_t current_distance =
            (g_player.x - next_x < 0 ? next_x - g_player.x : g_player.x - next_x) +
            (g_player.y - next_y < 0 ? next_y - g_player.y : g_player.y - next_y);
        if (current_distance < 5) {
            target_x = MAP_WIDTH - 2;
            target_y = MAP_HEIGHT - 2;
        }
    }

    const int32_t dx = target_x - next_x;
    const int32_t dy = target_y - next_y;
    const int32_t distance = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    return power_active() ? -distance : distance;
}

static Direction choose_ghost_direction(uint32_t ghost_index) {
    const Direction reverse = direction_opposite(g_ghosts[ghost_index].direction);
    Direction best = direction_none;
    int32_t best_score = 0x7fffffff;
    uint8_t choices = 0;

    for (Direction direction = direction_up; direction <= direction_right; direction++) {
        if (!can_move(g_ghosts[ghost_index].pos, direction)) { continue; }
        choices++;
        if (direction == reverse) { continue; }

        const int32_t score = ghost_direction_score(ghost_index, direction);
        if (best == direction_none || score < best_score ||
            (score == best_score && (random_next() & 1u) != 0)) {
            best = direction;
            best_score = score;
        }
    }

    if (best == direction_none && choices > 0 && can_move(g_ghosts[ghost_index].pos, reverse)) {
        best = reverse;
    }
    return best;
}

static void move_ghosts(void) {
    for (uint32_t i = 0; i < 4; i++) {
        const Position old = g_ghosts[i].pos;
        const Direction next_direction = choose_ghost_direction(i);
        if (next_direction != direction_none) {
            g_ghosts[i].direction = next_direction;
            g_ghosts[i].pos.x += g_dir_x[next_direction];
            g_ghosts[i].pos.y += g_dir_y[next_direction];
        }

        render_cell(old.x, old.y);
        render_cell(g_ghosts[i].pos.x, g_ghosts[i].pos.y);
        if (check_collisions() || g_game_state != game_state_playing) { return; }
    }
}

static Button* create_button(uint32_t gpio_idx) {
    const Button_config config = {
        .gpio_idx = gpio_idx,
        .gpio_state_when_pressed = bsp_gpio_state_reset,
    };
    return Button_Create(&config);
}

static void pacman_init(void) {
    const Joystick_config joystick_config = {
        .adc_idx = ADC_JOYSTICK_IDX,
        .adc_channel_x = ADC_JOYSTICK_X_CHANNEL,
        .adc_channel_y = ADC_JOYSTICK_Y_CHANNEL,
        .x_min_voltage = JOYSTICK_X_MIN_VOLTAGE,
        .x_max_voltage = JOYSTICK_X_MAX_VOLTAGE,
        .y_min_voltage = JOYSTICK_Y_MIN_VOLTAGE,
        .y_max_voltage = JOYSTICK_Y_MAX_VOLTAGE,
        .x_reverse = JOYSTICK_X_REVERSE,
        .y_reverse = JOYSTICK_Y_REVERSE,
    };
    g_joystick = Joystick_Create(&joystick_config);
    Joystick_Calibrate_Center(g_joystick, 32, 5);

    g_button_up = create_button(GPIO_BNT_UP_IDX);
    g_button_left = create_button(GPIO_BNT_LEFT_IDX);
    g_button_down = create_button(GPIO_BNT_DOWN_IDX);
    g_button_right = create_button(GPIO_BNT_RIGHT_IDX);
    g_button_start = create_button(GPIO_SW_BTN_IDX);

    const Buzzer_config buzzer_config = {.pwm_idx = PWM_BUZZER_IDX};
    g_buzzer = Buzzer_Create(&buzzer_config);

    const St7789_config lcd_config = {
        .spi_idx = SOFT_SPI_LCD_IDX,
        .cs_gpio_idx = (uint32_t)-1,
        .dc_gpio_idx = GPIO_TFT_DC_IDX,
        .rst_gpio_idx = GPIO_TFT_RST_IDX,
        .bkl_gpio_idx = GPIO_TFT_BLK_IDX,
        .hor_res = SCREEN_WIDTH,
        .ver_res = SCREEN_HEIGHT,
        .flags = {.mirror_y = 1, .color_use_bgr = 1},
    };
    g_lcd = St7789_Create(&lcd_config);
    configASSERT(g_lcd != NULL);

    St7789_Init(g_lcd);
    St7789_Set_Backlight(g_lcd, 1);
    restart_game();
}

static void pacman_loop(void) {
    const Direction input = read_direction();
    if (input != direction_none) { g_wanted_direction = input; }

    if (g_game_state != game_state_playing) {
        if (Button_Get_State(g_button_start) == button_state_down) {
            if (g_game_state == game_state_level_clear) {
                g_level++;
                load_level();
                render_full_map();
            } else {
                restart_game();
            }
        }
        return;
    }

    const uint32_t now = Bsp_Get_Tick_Ms();
    if (now - g_last_player_move >= PLAYER_MOVE_MS) {
        g_last_player_move = now;
        move_player();
    }
    if (g_game_state == game_state_playing && now - g_last_ghost_move >= GHOST_MOVE_MS) {
        g_last_ghost_move = now;
        move_ghosts();
    }
}

static void pacman_task(void* arg) {
    (void)arg;
    pacman_init();

    uint32_t tick = xTaskGetTickCount();
    while (1) {
        pacman_loop();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(20));
    }
}

void Pacman_Task_Def(void) {
    const BaseType_t result = xTaskCreate(pacman_task, "Pac", 512, NULL, 1, NULL);
    configASSERT(result == pdPASS);
}
