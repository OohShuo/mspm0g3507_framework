#include "racing.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_time.h"
#include "game_graphics.h"

#define SCREEN_WIDTH     240
#define SCREEN_HEIGHT    320

#define ROAD_X           35
#define ROAD_Y           48
#define ROAD_WIDTH       170
#define ROAD_HEIGHT      244
#define LANE_COUNT       3
#define LANE_WIDTH       (ROAD_WIDTH / LANE_COUNT)

#define CAR_WIDTH        28
#define CAR_HEIGHT       38
#define CAR_STRIP_HEIGHT 4
#define PLAYER_Y         (ROAD_Y + ROAD_HEIGHT - CAR_HEIGHT - 8)

#define OBSTACLE_COUNT   4
#define MIN_FRAME_MS     35u
#define MAX_FRAME_MS     90u

#define COLOR_BLACK      0x0000u
#define COLOR_WHITE      0xffffu
#define COLOR_GRASS      0x03e0u
#define COLOR_ROAD       0x4208u
#define COLOR_LINE       0xffffu
#define COLOR_PLAYER     0x07ffu
#define COLOR_WINDOW     0x001fu
#define COLOR_TIRE       0x0000u
#define COLOR_GAME_OVER  0xf800u

typedef struct {
    int16_t y;
    int8_t lane;
    uint16_t color;
    uint8_t active;
} Racing_obstacle;

typedef enum {
    racing_state_playing,
    racing_state_over,
} Racing_state;

static const uint16_t g_obstacle_colors[] = {0xf800u, 0xffe0u, 0xf81fu, 0x07e0u};

static Game_hardware g_hardware;
static Racing_obstacle g_obstacles[OBSTACLE_COUNT];
static Racing_state g_state = racing_state_playing;
static int8_t g_player_lane = 1;
static uint32_t g_score = 0;
static uint32_t g_last_frame = 0;
static uint32_t g_last_spawn = 0;
static uint32_t g_random_state = 0x7ac35e21u;
static uint16_t g_car_strip[CAR_WIDTH * CAR_STRIP_HEIGHT];

static uint32_t random_next(void) {
    g_random_state = g_random_state * 1664525u + 1013904223u;
    return g_random_state;
}

static int32_t lane_x(int8_t lane) { return ROAD_X + lane * LANE_WIDTH + (LANE_WIDTH - CAR_WIDTH) / 2; }

static uint32_t frame_interval(void) {
    const uint32_t reduction = (g_score / 10u) * 2u;
    return reduction >= MAX_FRAME_MS - MIN_FRAME_MS ? MIN_FRAME_MS : MAX_FRAME_MS - reduction;
}

static void draw_car(int8_t lane, int16_t y, uint16_t body_color) {
    if (y < ROAD_Y || y + CAR_HEIGHT > ROAD_Y + ROAD_HEIGHT) { return; }
    const int32_t screen_x = lane_x(lane);

    for (int32_t strip_y = 0; strip_y < CAR_HEIGHT; strip_y += CAR_STRIP_HEIGHT) {
        const int32_t strip_height =
            CAR_HEIGHT - strip_y < CAR_STRIP_HEIGHT ? CAR_HEIGHT - strip_y : CAR_STRIP_HEIGHT;

        for (int32_t row = 0; row < strip_height; row++) {
            const int32_t car_y = strip_y + row;
            for (int32_t x = 0; x < CAR_WIDTH; x++) {
                uint16_t color = COLOR_ROAD;
                const uint8_t body = x >= 4 && x < CAR_WIDTH - 4 && car_y >= 2 && car_y < CAR_HEIGHT - 2;
                const uint8_t cabin = x >= 7 && x < CAR_WIDTH - 7 && car_y >= 8 && car_y < 21;
                const uint8_t tire = (x < 4 || x >= CAR_WIDTH - 4) &&
                                     ((car_y >= 7 && car_y < 15) || (car_y >= 25 && car_y < 34));

                if (body) { color = body_color; }
                if (cabin) { color = COLOR_WINDOW; }
                if (tire) { color = COLOR_TIRE; }
                g_car_strip[row * CAR_WIDTH + x] = color;
            }
        }

        St7789_Flush(g_hardware.lcd, screen_x, y + strip_y, screen_x + CAR_WIDTH - 1,
            y + strip_y + strip_height - 1, (uint8_t*)g_car_strip,
            (uint32_t)(CAR_WIDTH * strip_height * sizeof(uint16_t)));
    }
}

static void clear_car(int8_t lane, int16_t y) {
    if (y < ROAD_Y || y + CAR_HEIGHT > ROAD_Y + ROAD_HEIGHT) { return; }
    Game_Graphics_Fill_Rect(g_hardware.lcd, lane_x(lane), y, CAR_WIDTH, CAR_HEIGHT, COLOR_ROAD);
}

static void draw_road(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_GRASS);
    Game_Graphics_Fill_Rect(g_hardware.lcd, ROAD_X, ROAD_Y, ROAD_WIDTH, ROAD_HEIGHT, COLOR_ROAD);
    Game_Graphics_Fill_Rect(g_hardware.lcd, ROAD_X - 3, ROAD_Y, 3, ROAD_HEIGHT, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_hardware.lcd, ROAD_X + ROAD_WIDTH, ROAD_Y, 3, ROAD_HEIGHT, COLOR_WHITE);

    for (int32_t lane = 1; lane < LANE_COUNT; lane++) {
        const int32_t x = ROAD_X + lane * LANE_WIDTH;
        for (int32_t y = ROAD_Y + 5; y < ROAD_Y + ROAD_HEIGHT; y += 24) {
            Game_Graphics_Fill_Rect(g_hardware.lcd, x - 1, y, 3, 12, COLOR_LINE);
        }
    }
}

static void render_hud(void) {
    Game_Graphics_Fill_Rect(g_hardware.lcd, 0, 0, SCREEN_WIDTH, 42, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_hardware.lcd, 8, 10, "SCORE", 2, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_hardware.lcd, 88, 10, g_score, 5, 2, COLOR_PLAYER);

    if (g_state == racing_state_over) {
        Game_Graphics_Draw_Text(g_hardware.lcd, 61, 300, "PRESS RESTART", 1, COLOR_GAME_OVER);
    } else {
        Game_Graphics_Draw_Text(g_hardware.lcd, 58, 300, "HOLD FOR MENU", 1, COLOR_WHITE);
    }
}

static void spawn_obstacle(uint32_t index) {
    Racing_obstacle* obstacle = &g_obstacles[index];
    obstacle->lane = (int8_t)(random_next() % LANE_COUNT);
    obstacle->y = ROAD_Y;
    obstacle->color = g_obstacle_colors[random_next() % OBSTACLE_COUNT];
    obstacle->active = 1;
    draw_car(obstacle->lane, obstacle->y, obstacle->color);
}

static void restart_game(void) {
    g_player_lane = 1;
    g_score = 0;
    g_state = racing_state_playing;
    for (uint32_t i = 0; i < OBSTACLE_COUNT; i++) { g_obstacles[i].active = 0; }

    draw_road();
    render_hud();
    draw_car(g_player_lane, PLAYER_Y, COLOR_PLAYER);

    g_last_frame = Bsp_Get_Tick_Ms();
    g_last_spawn = g_last_frame;
}

static uint8_t obstacle_hits_player(const Racing_obstacle* obstacle) {
    if (!obstacle->active || obstacle->lane != g_player_lane) { return 0; }
    return obstacle->y + CAR_HEIGHT > PLAYER_Y && obstacle->y < PLAYER_Y + CAR_HEIGHT;
}

static void end_game(void) {
    g_state = racing_state_over;
    Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_racing_crash);
    render_hud();
}

static void move_player(Game_direction direction) {
    int8_t new_lane = g_player_lane;
    if (direction == game_direction_left && new_lane > 0) { new_lane--; }
    if (direction == game_direction_right && new_lane < LANE_COUNT - 1) { new_lane++; }
    if (new_lane == g_player_lane) { return; }

    clear_car(g_player_lane, PLAYER_Y);
    g_player_lane = new_lane;
    Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_lane_change);
    for (uint32_t i = 0; i < OBSTACLE_COUNT; i++) {
        if (obstacle_hits_player(&g_obstacles[i])) {
            end_game();
            return;
        }
    }
    draw_car(g_player_lane, PLAYER_Y, COLOR_PLAYER);
}

static void update_obstacles(uint32_t step) {
    for (uint32_t i = 0; i < OBSTACLE_COUNT; i++) {
        Racing_obstacle* obstacle = &g_obstacles[i];
        if (!obstacle->active) { continue; }

        clear_car(obstacle->lane, obstacle->y);
        obstacle->y += (int16_t)step;
        if (obstacle->y + CAR_HEIGHT >= ROAD_Y + ROAD_HEIGHT) {
            obstacle->active = 0;
            g_score++;
            Buzzer_Play_Sfx(g_hardware.buzzer, buzzer_sfx_overtake);
            render_hud();
            continue;
        }

        if (obstacle_hits_player(obstacle)) {
            draw_car(obstacle->lane, obstacle->y, obstacle->color);
            end_game();
            return;
        }
        draw_car(obstacle->lane, obstacle->y, obstacle->color);
    }

    draw_car(g_player_lane, PLAYER_Y, COLOR_PLAYER);
}

static void try_spawn(uint32_t now) {
    const uint32_t spawn_interval = 900u - (g_score > 30u ? 300u : g_score * 10u);
    if (now - g_last_spawn < spawn_interval) { return; }

    for (uint32_t i = 0; i < OBSTACLE_COUNT; i++) {
        if (!g_obstacles[i].active) {
            spawn_obstacle(i);
            g_last_spawn = now;
            return;
        }
    }
}

void Racing_Init(const Game_hardware* hardware) {
    if (hardware == NULL) { return; }
    g_hardware = *hardware;
    restart_game();
}

Game_result Racing_Update(const Game_input* input) {
    if (input == NULL) { return game_result_running; }
    if (input->back_requested) { return game_result_exit; }

    if (g_state == racing_state_over) {
        if (input->confirm_pressed) {
            restart_game();
        }
        return game_result_running;
    }

    if (input->direction_pressed &&
        (input->direction == game_direction_left || input->direction == game_direction_right)) {
        move_player(input->direction);
        if (g_state != racing_state_playing) { return game_result_running; }
    }

    const uint32_t now = Bsp_Get_Tick_Ms();
    uint32_t interval = frame_interval();
    if (input->direction == game_direction_up && interval > MIN_FRAME_MS) { interval -= 15u; }
    if (input->direction == game_direction_down) { interval += 35u; }

    if (now - g_last_frame >= interval) {
        g_last_frame = now;
        update_obstacles(5u);
        if (g_state == racing_state_playing) { try_spawn(now); }
    }
    return game_result_running;
}

uint32_t Racing_Get_Score(void) { return g_score; }

uint8_t Racing_Is_Finished(void) { return g_state == racing_state_over; }
