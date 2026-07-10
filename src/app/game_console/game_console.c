#include "game_console.h"

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_config.h"
#include "board_config.h"
#include "bsp_time.h"
#include "button.h"
#include "buzzer.h"
#include "game_control_hint.h"
#include "game_graphics.h"
#include "game_info_screen.h"
#include "game_over_menu.h"
#include "game_registry.h"
#include "game_runtime.h"
#include "joystick.h"
#include "rtt_log.h"
#include "score_store.h"
#include "screensaver.h"
#include "st7789.h"
#include "task.h"

#define SCREEN_WIDTH           240
#define SCREEN_HEIGHT          320

#define INPUT_HW_BTN_A_IDX     GPIO_BNT_DOWN_IDX
#define INPUT_HW_BTN_B_IDX     GPIO_BNT_RIGHT_IDX
#define INPUT_HW_BTN_X_IDX     GPIO_BNT_UP_IDX
#define INPUT_HW_BTN_Y_IDX     GPIO_BNT_LEFT_IDX
#define INPUT_HW_BTN_START_IDX GPIO_SW_BTN_IDX

/* ── 3×2 grid menu layout ── */
#define MENU_COLS              3u
#define MENU_ROWS              2u
#define MENU_PER_PAGE          (MENU_COLS * MENU_ROWS)
#define CELL_W                 68
#define CELL_H                 88
#define CELL_GAP_X             8
#define CELL_GAP_Y             10
#define GRID_X0                10
#define GRID_Y0                56

#define COLOR_BLACK            0x0000u
#define COLOR_WHITE            0xffffu
#define COLOR_CYAN             0x07ffu
#define COLOR_BLUE             0x0010u
#define COLOR_YELLOW           0xffe0u
#define COLOR_GREEN            0x07e0u
#define COLOR_RED              0xf800u
#define COLOR_GRAY             0x8410u
#define COLOR_DARK             0xA514u

typedef enum {
    console_state_menu,
    console_state_game_info,
    console_state_game,
    console_state_paused,
    console_state_game_over,
} Console_state;

static St7789* g_lcd = NULL;
static Joystick* g_joystick = NULL;
static Button* g_a_button = NULL;
static Button* g_b_button = NULL;
static Button* g_x_button = NULL;
static Button* g_y_button = NULL;
static Button* g_start_button = NULL;
static Buzzer* g_buzzer = NULL;
static Vib_motor_gpio* g_vib_motor = NULL;

static Console_state g_console_state = console_state_menu;
static Game_direction g_last_direction = game_direction_none;
static Button_state g_last_a_state = button_state_up;
static Button_state g_last_b_state = button_state_up;
static Button_state g_last_x_state = button_state_up;
static Button_state g_last_y_state = button_state_up;
static Button_state g_last_start_state = button_state_up;
static uint8_t g_menu_selection = 0;
static uint8_t g_current_page = 0;
#if GAME_RUNTIME_MONITOR_ENABLE
static uint32_t g_last_monitor_time = 0;
#endif
static uint32_t g_last_input_tick = 0;

/* ── FPS tracking ── */
static uint32_t g_fps_frame_count = 0;
static uint32_t g_last_fps_time = 0;
static uint32_t g_current_fps = 0;
static uint32_t g_last_fps_draw_ms = 0;
static char g_running_bottom_hint[GAME_CONTROL_HINT_TEXT_MAX];

static void draw_running_bottom_bar(const Game_descriptor* game) {
    if (game == NULL) { return; }
    Game_Control_Hint_Format(game->control_hint, game->is_game, g_running_bottom_hint);
    Game_Graphics_Draw_Bottom_Bar(g_lcd, g_running_bottom_hint, g_current_fps);
}

static void render_game_info(void) {
    const Game_descriptor* game = Game_Registry_Get(g_menu_selection);
    if (game == NULL) { return; }
    Game_Info_Screen_Draw(g_lcd, game->name, game->info_text, g_current_fps);
}

static Game_direction direction_from_axes(float x, float y, Game_direction prev) {
    const float abs_x = x < 0.0f ? -x : x;
    const float abs_y = y < 0.0f ? -y : y;

    if (abs_x < JOYSTICK_DIRECTION_THRESHOLD && abs_y < JOYSTICK_DIRECTION_THRESHOLD) {
        return game_direction_none;
    }

    const float hysteresis = (prev != game_direction_none) ? JOYSTICK_DIRECTION_HYSTERESIS : 0.0f;

    if (abs_x > abs_y + hysteresis) { return x < 0.0f ? game_direction_left : game_direction_right; }
    if (abs_y > abs_x + hysteresis) { return y < 0.0f ? game_direction_up : game_direction_down; }

    return prev;
}

static void poll_button(
    Button* button, Button_state* last_state, uint8_t* down, uint8_t* pressed, uint8_t* released) {
    const Button_state state = Button_Get_State(button);
    *down = state == button_state_down;
    *pressed = state == button_state_down && *last_state == button_state_up;
    *released = state == button_state_up && *last_state == button_state_down;
    *last_state = state;
}

static Game_input poll_input(void) {
    Game_input input = {0};
    if (g_joystick != NULL) {
        input.axis_x = g_joystick->x_value;
        input.axis_y = -g_joystick->y_value;
        if (input.axis_x != 0.0f && input.axis_y != 0.0f) {
            input.axis_x *= 0.7071f;
            input.axis_y *= 0.7071f;
        }
        input.stick_active = input.axis_x != 0.0f || input.axis_y != 0.0f;
    }

    input.direction = direction_from_axes(input.axis_x, input.axis_y, g_last_direction);
    input.direction_pressed = input.direction != game_direction_none && input.direction != g_last_direction;

    poll_button(g_a_button, &g_last_a_state, &input.a_down, &input.a_pressed, &input.a_released);
    poll_button(g_b_button, &g_last_b_state, &input.b_down, &input.b_pressed, &input.b_released);
    poll_button(g_x_button, &g_last_x_state, &input.x_down, &input.x_pressed, &input.x_released);
    poll_button(g_y_button, &g_last_y_state, &input.y_down, &input.y_pressed, &input.y_released);
    poll_button(
        g_start_button, &g_last_start_state, &input.start_down, &input.start_pressed, &input.start_released);

    input.confirm_pressed = input.a_pressed;
    input.back_requested = input.b_pressed;

    g_last_direction = input.direction;
    return input;
}

static int32_t cell_x(uint8_t col) { return GRID_X0 + (int32_t)col * (CELL_W + CELL_GAP_X); }
static int32_t cell_y(uint8_t row) { return GRID_Y0 + (int32_t)row * (CELL_H + CELL_GAP_Y); }

static void draw_dot(int32_t x, int32_t y, uint8_t filled, uint16_t color) {
    if (filled) {
        Game_Graphics_Fill_Rect(g_lcd, x - 3, y - 3, 7, 7, color);
    } else {
        Game_Graphics_Fill_Rect(g_lcd, x - 3, y - 3, 7, 7, COLOR_DARK);
        Game_Graphics_Fill_Rect(g_lcd, x - 1, y - 1, 3, 3, color);
    }
}

static void draw_grid_cell(uint8_t row, uint8_t col, uint8_t selected, uint8_t game_index) {
    const Game_descriptor* game = Game_Registry_Get(game_index);
    if (game == NULL) { return; }

    const int32_t cx = cell_x(col);
    const int32_t cy = cell_y(row);
    const uint16_t border = selected ? COLOR_CYAN : COLOR_DARK;
    const uint16_t background = selected ? COLOR_BLUE : COLOR_BLACK;

    Game_Graphics_Fill_Rect(g_lcd, cx - 2, cy - 2, CELL_W + 4, CELL_H + 4, border);
    Game_Graphics_Fill_Rect(g_lcd, cx, cy, CELL_W, CELL_H, background);

    /* Icon centered in upper portion of cell */
    const int32_t icon_x = cx + (CELL_W - 48) / 2;
    const int32_t icon_y = cy + 4;
    if (game->draw_icon != NULL) { game->draw_icon(g_lcd, icon_x, icon_y); }

    /* Game name below icon */
    const int32_t name_len = (int32_t)strlen(game->name);
    const int32_t name_x = cx + (CELL_W - name_len * 6) / 2;
    Game_Graphics_Draw_Text(g_lcd, name_x, cy + 54, game->name, 1, selected ? COLOR_WHITE : game->name_color);

    /* High score */
    Game_Graphics_Draw_Text(g_lcd, cx + 8, cy + 70, "HI", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, cx + 26, cy + 70, Score_Store_Get(game->id), 5, 1, COLOR_WHITE);
}

static void format_page_text(char* text, uint8_t page, uint8_t total_pages) {
    char* out = text;
    if (page >= 10u) { *out++ = (char)('0' + page / 10u); }
    *out++ = (char)('0' + page % 10u);
    *out++ = '/';
    if (total_pages >= 10u) { *out++ = (char)('0' + total_pages / 10u); }
    *out++ = (char)('0' + total_pages % 10u);
    *out = '\0';
}

static void draw_page_indicator(void) {
    const uint8_t game_count = Game_Registry_Count();
    const uint8_t total_pages = (uint8_t)((game_count + MENU_PER_PAGE - 1) / MENU_PER_PAGE);
    const int32_t bar_y = 250;
    const int32_t dot_y = bar_y + 12;

    /* Separator line */
    Game_Graphics_Fill_Rect(g_lcd, 10, bar_y, SCREEN_WIDTH - 20, 1, COLOR_DARK);

    /* Left arrow */
    const uint16_t left_color = g_current_page > 0 ? COLOR_CYAN : COLOR_DARK;
    Game_Graphics_Draw_Text(g_lcd, 20, bar_y + 6, "<", 2, left_color);

    /* Page dots */
    const int32_t dots_x = (int32_t)(SCREEN_WIDTH / 2 - total_pages * 8);
    for (uint8_t p = 0; p < total_pages; p++) {
        draw_dot(dots_x + (int32_t)p * 16, dot_y, p == g_current_page, COLOR_CYAN);
    }

    /* Right arrow */
    const uint16_t right_color = g_current_page < total_pages - 1 ? COLOR_CYAN : COLOR_DARK;
    Game_Graphics_Draw_Text(g_lcd, SCREEN_WIDTH - 36, bar_y + 6, ">", 2, right_color);

    /* Page number */
    char page_text[8];
    format_page_text(page_text, (uint8_t)(g_current_page + 1u), total_pages);
    Game_Graphics_Draw_Text(g_lcd, SCREEN_WIDTH - 60, bar_y + 6, page_text, 1, COLOR_WHITE);

    /* Separator line */
    Game_Graphics_Fill_Rect(g_lcd, 10, bar_y + 22, SCREEN_WIDTH - 20, 1, COLOR_DARK);
}

static void fps_tick(void) {
    g_fps_frame_count++;
    const uint32_t now = Bsp_Get_Tick_Ms();
    const uint32_t elapsed = now - g_last_fps_time;
    if (elapsed >= 300u) {
        g_current_fps = elapsed > 0u ? g_fps_frame_count * 1000u / elapsed : 0u;
        g_fps_frame_count = 0;
        g_last_fps_time = now;
    }
}

static void draw_fps(void) {
    /* Refresh every 300ms */
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (now - g_last_fps_draw_ms < 300u) { return; }
    g_last_fps_draw_ms = now;

    /* Menu / game: lightweight FPS update inside unified bottom bar */
    if (g_console_state == console_state_game || g_console_state == console_state_paused ||
        g_console_state == console_state_menu || g_console_state == console_state_game_info) {
        Game_Graphics_Update_Bottom_Fps(g_lcd, g_current_fps);
        return;
    }

    /* For game-over / screensaver: FPS at bottom-right */
    Game_Graphics_Fill_Rect(g_lcd, SCREEN_WIDTH - 62, SCREEN_HEIGHT - 20, 62, 12, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_lcd, SCREEN_WIDTH - 62, SCREEN_HEIGHT - 20, "FPS:", 1, 0x8410u);
    Game_Graphics_Draw_U32(g_lcd, SCREEN_WIDTH - 28, SCREEN_HEIGHT - 20, g_current_fps, 3, 1, 0xffffu);
}

static void render_menu(void) {
    const uint8_t game_count = Game_Registry_Count();
    const uint8_t page_start = (uint8_t)(g_current_page * MENU_PER_PAGE);
    const uint8_t page_end = page_start + MENU_PER_PAGE;
    const uint8_t page_game_count =
        page_end <= game_count ? MENU_PER_PAGE : (uint8_t)(game_count - page_start);

    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);

    /* Title bar */
    Game_Graphics_Draw_Text(g_lcd, 62, 5, "SELECT GAME", 2, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, 10, 26, SCREEN_WIDTH - 20, 1, COLOR_DARK);

    /* Draw grid cells */
    for (uint8_t slot = 0; slot < page_game_count; slot++) {
        const uint8_t row = slot / MENU_COLS;
        const uint8_t col = slot % MENU_COLS;
        const uint8_t game_index = (uint8_t)(page_start + slot);
        draw_grid_cell(row, col, g_menu_selection == game_index, game_index);
    }

    /* Page indicator with arrows and dots */
    draw_page_indicator();

    /* Navigation hints above bottom bar */
    Game_Graphics_Draw_Text(g_lcd, 14, 276, "JOY: MOVE", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 112, 276, "X: INFO", 1, COLOR_CYAN);
    Game_Graphics_Draw_Text(g_lcd, 174, 276, "A: OK", 1, COLOR_WHITE);

    /* Unified bottom bar */
    Game_Graphics_Draw_Bottom_Bar(g_lcd, "A OK  X INFO  B BACK", g_current_fps);
}

static void enter_menu(void) {
    g_console_state = console_state_menu;
    render_menu();
}

static void redraw_cell(uint8_t game_index, uint8_t selected) {
    const uint8_t page_start = (uint8_t)(g_current_page * MENU_PER_PAGE);
    const uint8_t slot = (uint8_t)(game_index - page_start);
    draw_grid_cell(slot / MENU_COLS, slot % MENU_COLS, selected, game_index);
}

static void update_menu(const Game_input* input, const Game_hardware* hardware) {
    const uint8_t game_count = Game_Registry_Count();
    if (game_count == 0) { return; }

    const uint8_t page_start = (uint8_t)(g_current_page * MENU_PER_PAGE);
    const uint8_t page_game_count =
        page_start + MENU_PER_PAGE <= game_count ? MENU_PER_PAGE : (uint8_t)(game_count - page_start);

    if (input->direction_pressed) {
        const uint8_t old_selection = g_menu_selection;
        const uint8_t old_page = g_current_page;
        uint8_t changed = 0;

        /* Compute cursor slot within current page */
        const uint8_t slot = (uint8_t)(g_menu_selection - page_start);
        const uint8_t row = slot / MENU_COLS;
        const uint8_t col = slot % MENU_COLS;

        if (input->direction == game_direction_up) {
            if (row == 0) {
                /* Wrap to last row */
                const uint8_t last_row = (uint8_t)((page_game_count - 1) / MENU_COLS);
                const uint8_t new_slot = last_row * MENU_COLS + col;
                g_menu_selection = new_slot < page_game_count ? (uint8_t)(page_start + new_slot)
                                                              : (uint8_t)(page_start + page_game_count - 1);
            } else {
                g_menu_selection = (uint8_t)(page_start + (row - 1) * MENU_COLS + col);
            }
            changed = 1;
        } else if (input->direction == game_direction_down) {
            const uint8_t new_row = row + 1;
            const uint8_t new_slot = new_row * MENU_COLS + col;
            if (new_slot < page_game_count) {
                g_menu_selection = (uint8_t)(page_start + new_slot);
            } else {
                /* Wrap to top row, same column */
                g_menu_selection = col < page_game_count ? (uint8_t)(page_start + col) : page_start;
            }
            changed = 1;
        } else if (input->direction == game_direction_left) {
            if (col > 0) {
                g_menu_selection = (uint8_t)(g_menu_selection - 1);
            } else {
                /* Flip page left */
                const uint8_t total_pages = (uint8_t)((game_count + MENU_PER_PAGE - 1) / MENU_PER_PAGE);
                if (g_current_page > 0) {
                    g_current_page--;
                } else {
                    g_current_page = (uint8_t)(total_pages - 1);
                }
                /* Position cursor on rightmost column of the same row */
                const uint8_t new_start = (uint8_t)(g_current_page * MENU_PER_PAGE);
                const uint8_t new_count = new_start + MENU_PER_PAGE <= game_count
                                              ? MENU_PER_PAGE
                                              : (uint8_t)(game_count - new_start);
                const uint8_t new_rows = (uint8_t)((new_count + MENU_COLS - 1) / MENU_COLS);
                const uint8_t target_row = row < new_rows ? row : 0;
                const uint8_t target_slot = target_row * MENU_COLS + (MENU_COLS - 1);
                g_menu_selection = target_slot < new_count ? (uint8_t)(new_start + target_slot)
                                                           : (uint8_t)(new_start + new_count - 1);
            }
            changed = 1;
        } else if (input->direction == game_direction_right) {
            const uint8_t next_slot = slot + 1;
            if (col < MENU_COLS - 1 && next_slot < page_game_count) {
                g_menu_selection = (uint8_t)(g_menu_selection + 1);
            } else {
                /* Flip page right */
                const uint8_t total_pages = (uint8_t)((game_count + MENU_PER_PAGE - 1) / MENU_PER_PAGE);
                if (g_current_page < total_pages - 1) {
                    g_current_page++;
                } else {
                    g_current_page = 0;
                }
                /* Position cursor on leftmost column of the same row */
                const uint8_t new_start = (uint8_t)(g_current_page * MENU_PER_PAGE);
                const uint8_t new_count = new_start + MENU_PER_PAGE <= game_count
                                              ? MENU_PER_PAGE
                                              : (uint8_t)(game_count - new_start);
                const uint8_t new_rows = (uint8_t)((new_count + MENU_COLS - 1) / MENU_COLS);
                const uint8_t target_row = row < new_rows ? row : 0;
                g_menu_selection = (uint8_t)(new_start + target_row * MENU_COLS);
            }
            changed = 1;
        }

        if (changed) {
            Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_move);
            Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_menu_tick);
            if (g_current_page != old_page) {
                /* Page flip — full screen redraw */
                render_menu();
            } else if (g_menu_selection != old_selection) {
                /* Same page — only redraw the two changed cells */
                redraw_cell(old_selection, 0);
                redraw_cell(g_menu_selection, 1);
            }
        }
    }

    if (input->x_pressed) {
        Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_select);
        Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_menu_select);
        g_console_state = console_state_game_info;
        render_game_info();
        return;
    }

    if (!input->confirm_pressed) { return; }

    const Game_descriptor* game = Game_Registry_Get(g_menu_selection);
    if (game != NULL) {
        Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_select);
        Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_menu_select);
        g_console_state = console_state_game;
        Game_Graphics_Draw_Top_Bar(g_lcd, game->name);
        Game_Graphics_Clear_Game_Area(g_lcd);
        game->init(hardware);
        draw_running_bottom_bar(game);
    }
}

static void update_game_info(const Game_input* input) {
    if (!input->b_pressed) { return; }
    Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_move);
    Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_back);
    enter_menu();
}

static void monitor_resources(void) {
#if GAME_RUNTIME_MONITOR_ENABLE
    const uint32_t now = Bsp_Get_Tick_Ms();
    if (now - g_last_monitor_time < GAME_RUNTIME_MONITOR_INTERVAL_MS) { return; }
    g_last_monitor_time = now;

    printf("[MEM] heap=%u min_heap=%u game_stack=%u words save=%u\n", (unsigned)xPortGetFreeHeapSize(),
        (unsigned)xPortGetMinimumEverFreeHeapSize(), (unsigned)uxTaskGetStackHighWaterMark(NULL),
        (unsigned)Score_Store_Is_Available());
#endif
}

static Game_result update_active_game(const Game_input* input) {
    const Game_descriptor* game = Game_Registry_Get(g_menu_selection);
    if (game == NULL) { return game_result_exit; }

    if (game->is_game && (input->x_pressed || input->b_pressed)) {
        g_console_state = console_state_paused;
        Game_Runtime_Pause_Time();
        Game_Graphics_Draw_Pause_Bottom_Bar(g_lcd, g_current_fps);
        return game_result_running;
    }

    const Game_result result = game->update(input);
    if (result == game_result_exit) { return result; }
    if (result == game_result_won || result == game_result_lost) {
        g_console_state = console_state_game_over;
        Game_Over_Menu_Open(g_lcd, g_buzzer, g_vib_motor, result, game->id, game->name, game->get_score());
    }
    return game_result_running;
}

static void update_paused_game(const Game_input* input) {
    if (input->b_pressed) {
        Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_back);
        Game_Runtime_Resume_Time();
        enter_menu();
        return;
    }
    if (input->a_pressed || input->x_pressed) {
        const Game_descriptor* game = Game_Registry_Get(g_menu_selection);
        Game_Runtime_Resume_Time();
        g_console_state = console_state_game;
        draw_running_bottom_bar(game);
    }
}

static void update_game_over(const Game_input* input, const Game_hardware* hardware) {
    const Game_over_action action = Game_Over_Menu_Update(input);
    if (action == game_over_action_menu) {
        enter_menu();
    } else if (action == game_over_action_replay) {
        const Game_descriptor* game = Game_Registry_Get(g_menu_selection);
        if (game == NULL) {
            enter_menu();
            return;
        }
        g_console_state = console_state_game;
        Game_Graphics_Draw_Top_Bar(g_lcd, game->name);
        Game_Graphics_Clear_Game_Area(g_lcd);
        game->init(hardware);
        draw_running_bottom_bar(game);
    }
}

static void console_init(void) {
    const Joystick_config joystick_config = {
        .adc_idx = ADC_JOYSTICK_IDX,
        .adc_channel_x = ADC_JOYSTICK_X_CHANNEL,
        .adc_channel_y = ADC_JOYSTICK_Y_CHANNEL,
        .x_min_voltage = JOYSTICK_X_MIN_VOLTAGE,
        .x_max_voltage = JOYSTICK_X_MAX_VOLTAGE,
        .y_min_voltage = JOYSTICK_Y_MIN_VOLTAGE,
        .y_max_voltage = JOYSTICK_Y_MAX_VOLTAGE,
        .x_offset = JOYSTICK_X_OFFSET,
        .y_offset = JOYSTICK_Y_OFFSET,
        .x_dead_zone = JOYSTICK_X_DEAD_ZONE,
        .y_dead_zone = JOYSTICK_Y_DEAD_ZONE,
        .x_reverse = JOYSTICK_X_REVERSE,
        .y_reverse = JOYSTICK_Y_REVERSE,
    };
    g_joystick = Joystick_Create(&joystick_config);
    configASSERT(g_joystick != NULL);
    Joystick_Calibrate_Center(g_joystick, JOYSTICK_CALIBRATION_SAMPLES, JOYSTICK_CALIBRATION_INTERVAL_MS);

    const Button_config button_config = {
        .gpio_idx = INPUT_HW_BTN_A_IDX,
        .gpio_state_when_pressed = bsp_gpio_state_reset,
    };
    Button_config config = button_config;
    g_a_button = Button_Create(&config);
    config.gpio_idx = INPUT_HW_BTN_B_IDX;
    g_b_button = Button_Create(&config);
    config.gpio_idx = INPUT_HW_BTN_X_IDX;
    g_x_button = Button_Create(&config);
    config.gpio_idx = INPUT_HW_BTN_Y_IDX;
    g_y_button = Button_Create(&config);
    config.gpio_idx = INPUT_HW_BTN_START_IDX;
    g_start_button = Button_Create(&config);
    configASSERT(g_a_button != NULL && g_b_button != NULL && g_x_button != NULL && g_y_button != NULL &&
                 g_start_button != NULL);

    const Buzzer_config buzzer_config = {.pwm_idx = PWM_BUZZER_IDX};
    g_buzzer = Buzzer_Create(&buzzer_config);
    configASSERT(g_buzzer != NULL);

    const Vib_motor_gpio_config vib_motor_config = {
        .gpio_idx = GPIO_VIB_MOTOR_IDX,
        .active_high = 1u,
        .enabled = 1u,
    };
    g_vib_motor = Vib_Motor_Gpio_Create(&vib_motor_config);
    configASSERT(g_vib_motor != NULL);

    const St7789_config lcd_config = {
        .spi_idx = SOFT_SPI_LCD_IDX,
        .cs_gpio_idx = (uint32_t)-1,
        .dc_gpio_idx = GPIO_TFT_DC_IDX,
        .rst_gpio_idx = GPIO_TFT_RST_IDX,
        .bkl_gpio_idx = GPIO_TFT_BLK_IDX,
        .hor_res = SCREEN_WIDTH,
        .ver_res = SCREEN_HEIGHT,
        .flags = {.mirror_x = LCD_MIRROR_X, .mirror_y = LCD_MIRROR_Y, .color_use_bgr = LCD_COLOR_USE_BGR},
    };
    g_lcd = St7789_Create(&lcd_config);
    configASSERT(g_lcd != NULL);
    St7789_Init(g_lcd);
    St7789_Set_Backlight(g_lcd, 1);

    Score_Store_Init(game_id_count);
    render_menu();

    Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_boot);
}

static void console_task(void* arg) {
    (void)arg;
    console_init();
    g_last_input_tick = Bsp_Get_Tick_Ms();

    const Game_hardware hardware = {
        .lcd = g_lcd,
        .buzzer = g_buzzer,
        .vib_motor = g_vib_motor,
    };
    uint32_t tick = xTaskGetTickCount();

    while (1) {
        fps_tick();

        const Game_input input = poll_input();
        Game_result result = game_result_running;

        /* ── idle tracking ── */
        if (input.direction_pressed || input.a_pressed || input.b_pressed || input.x_pressed ||
            input.y_pressed) {
            g_last_input_tick = Bsp_Get_Tick_Ms();
        }

        /* ── screensaver activation ── */
        if (!Screensaver_Is_Active()) {
            if (g_console_state != console_state_game && g_console_state != console_state_paused &&
                Bsp_Get_Tick_Ms() - g_last_input_tick > 30000u) {
                Screensaver_Init(g_lcd);
            }
        }

        /* ── screensaver loop ── */
        if (Screensaver_Is_Active()) {
            if (input.direction_pressed || input.a_pressed || input.b_pressed || input.x_pressed ||
                input.y_pressed) {
                Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_action_light);
                Screensaver_Exit();
                g_last_input_tick = Bsp_Get_Tick_Ms();
                /* redraw the screen that was underneath */
                if (g_console_state == console_state_menu) {
                    render_menu();
                } else if (g_console_state == console_state_game_info) {
                    render_game_info();
                } else if (g_console_state == console_state_game_over) {
                    Game_Over_Menu_Redraw();
                } else if (g_console_state == console_state_game) {
                    const Game_descriptor* game = Game_Registry_Get(g_menu_selection);
                    if (game != NULL) {
                        Game_Graphics_Draw_Top_Bar(g_lcd, game->name);
                        draw_running_bottom_bar(game);
                    }
                }
            } else {
                Screensaver_Run_Frame();
            }
            draw_fps();
            vTaskDelayUntil(&tick, pdMS_TO_TICKS(20));
            continue;
        }

        /* ── normal dispatch ── */
        if (g_console_state == console_state_menu) {
            update_menu(&input, &hardware);
        } else if (g_console_state == console_state_game_info) {
            update_game_info(&input);
        } else if (g_console_state == console_state_game) {
            result = update_active_game(&input);
        } else if (g_console_state == console_state_paused) {
            update_paused_game(&input);
        } else {
            update_game_over(&input, &hardware);
        }

        if (result == game_result_exit) {
            if (input.back_requested) { Vib_Motor_Gpio_Play_Effect(g_vib_motor, vib_effect_back); }
            enter_menu();
        }
        monitor_resources();
        draw_fps();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(5));
    }
}

void Game_Console_Task_Def(void) {
    const BaseType_t result = xTaskCreate(console_task, "Game", 1024, NULL, 1, NULL);
    configASSERT(result == pdPASS);
}
