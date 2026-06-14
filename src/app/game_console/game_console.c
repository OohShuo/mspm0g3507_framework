#include "game_console.h"

#include <stdint.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_time.h"
#include "button.h"
#include "buzzer.h"
#include "game_graphics.h"
#include "game_runtime.h"
#include "joystick.h"
#include "pacman.h"
#include "racing.h"
#include "snake.h"
#include "st7789.h"
#include "task.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define BACK_HOLD_MS   1000u

#define COLOR_BLACK    0x0000u
#define COLOR_WHITE    0xffffu
#define COLOR_CYAN     0x07ffu
#define COLOR_BLUE     0x0010u
#define COLOR_YELLOW   0xffe0u
#define COLOR_GREEN    0x07e0u
#define COLOR_DARK     0x0841u

typedef enum {
    console_state_menu,
    console_state_pacman,
    console_state_snake,
    console_state_racing,
} Console_state;

static St7789* g_lcd = NULL;
static Joystick* g_joystick = NULL;
static Button* g_confirm_button = NULL;
static Buzzer* g_buzzer = NULL;

static Console_state g_console_state = console_state_menu;
static Game_direction g_last_direction = game_direction_none;
static Button_state g_last_button_state = button_state_up;
static uint32_t g_button_down_time = 0;
static uint8_t g_back_sent = 0;
static uint8_t g_menu_selection = 0;

static Game_direction read_direction(void) {
    if (g_joystick == NULL) { return game_direction_none; }

    const float x = g_joystick->x_value;
    const float y = g_joystick->y_value;
    const float abs_x = x < 0.0f ? -x : x;
    const float abs_y = y < 0.0f ? -y : y;

    if (abs_x < JOYSTICK_DIRECTION_THRESHOLD && abs_y < JOYSTICK_DIRECTION_THRESHOLD) {
        return game_direction_none;
    }
    if (abs_x > abs_y) { return x < 0.0f ? game_direction_left : game_direction_right; }
    return y < 0.0f ? game_direction_down : game_direction_up;
}

static Game_input poll_input(void) {
    Game_input input = {0};
    const uint32_t now = Bsp_Get_Tick_Ms();
    const Button_state button_state = Button_Get_State(g_confirm_button);

    input.direction = read_direction();
    input.direction_pressed =
        input.direction != game_direction_none && input.direction != g_last_direction;

    if (button_state == button_state_down && g_last_button_state == button_state_up) {
        g_button_down_time = now;
        g_back_sent = 0;
    } else if (button_state == button_state_down && !g_back_sent &&
               now - g_button_down_time >= BACK_HOLD_MS) {
        input.back_requested = 1;
        g_back_sent = 1;
    } else if (button_state == button_state_up && g_last_button_state == button_state_down) {
        if (!g_back_sent) { input.confirm_pressed = 1; }
    }

    g_last_direction = input.direction;
    g_last_button_state = button_state;
    return input;
}

static void draw_pacman_icon(int32_t x, int32_t y) {
    for (int32_t row = -18; row <= 18; row++) {
        int32_t half_width = 0;
        while ((half_width + 1) * (half_width + 1) + row * row <= 18 * 18) {
            half_width++;
        }

        const int32_t abs_row = row < 0 ? -row : row;
        int32_t right = half_width;
        if (abs_row <= half_width) { right = abs_row - 1; }
        if (right >= -half_width) {
            Game_Graphics_Fill_Rect(
                g_lcd, x - half_width, y + row, right + half_width + 1, 1, COLOR_YELLOW);
        }
    }
}

static void draw_snake_icon(int32_t x, int32_t y) {
    static const int8_t cells[][2] = {
        {0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {3, 2}, {4, 2},
    };
    for (uint32_t i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
        Game_Graphics_Fill_Rect(
            g_lcd, x + cells[i][0] * 9, y + cells[i][1] * 9, 8, 8, COLOR_GREEN);
    }
    Game_Graphics_Fill_Rect(g_lcd, x + 39, y + 19, 2, 2, COLOR_BLACK);
}

static void draw_racing_icon(int32_t x, int32_t y) {
    Game_Graphics_Fill_Rect(g_lcd, x, y, 48, 34, COLOR_DARK);
    Game_Graphics_Fill_Rect(g_lcd, x + 22, y, 4, 34, COLOR_WHITE);
    Game_Graphics_Fill_Rect(g_lcd, x + 8, y + 8, 18, 24, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, x + 12, y + 12, 10, 8, COLOR_BLUE);
}

static void draw_menu_card(int32_t y, uint8_t selected, uint8_t game_index) {
    const uint16_t border = selected ? COLOR_CYAN : COLOR_DARK;
    const uint16_t background = selected ? COLOR_BLUE : COLOR_BLACK;

    Game_Graphics_Fill_Rect(g_lcd, 18, y, 204, 56, border);
    Game_Graphics_Fill_Rect(g_lcd, 21, y + 3, 198, 50, background);

    if (game_index == 0) {
        draw_pacman_icon(62, y + 28);
        Game_Graphics_Draw_Text(g_lcd, 100, y + 20, "PAC-MAN", 2, COLOR_YELLOW);
    } else if (game_index == 1) {
        draw_snake_icon(38, y + 14);
        Game_Graphics_Draw_Text(g_lcd, 108, y + 20, "SNAKE", 2, COLOR_GREEN);
    } else {
        draw_racing_icon(38, y + 11);
        Game_Graphics_Draw_Text(g_lcd, 105, y + 20, "RACING", 2, COLOR_CYAN);
    }
}

static void render_menu(void) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_lcd, 52, 17, "GAME SELECT", 2, COLOR_CYAN);
    draw_menu_card(54, g_menu_selection == 0, 0);
    draw_menu_card(116, g_menu_selection == 1, 1);
    draw_menu_card(178, g_menu_selection == 2, 2);
    Game_Graphics_Draw_Text(g_lcd, 42, 252, "MOVE JOYSTICK", 2, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 48, 284, "PRESS TO START", 1, COLOR_WHITE);
}

static void enter_menu(void) {
    Buzzer_Stop(g_buzzer);
    g_console_state = console_state_menu;
    render_menu();
}

static void update_menu(const Game_input* input, const Game_hardware* hardware) {
    if (input->direction_pressed && input->direction == game_direction_up) {
        g_menu_selection = g_menu_selection == 0 ? 2 : (uint8_t)(g_menu_selection - 1);
        render_menu();
    } else if (input->direction_pressed && input->direction == game_direction_down) {
        g_menu_selection = (uint8_t)((g_menu_selection + 1) % 3);
        render_menu();
    }

    if (!input->confirm_pressed) { return; }

    if (g_menu_selection == 0) {
        g_console_state = console_state_pacman;
        Pacman_Init(hardware);
    } else if (g_menu_selection == 1) {
        g_console_state = console_state_snake;
        Snake_Init(hardware);
    } else {
        g_console_state = console_state_racing;
        Racing_Init(hardware);
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
    Joystick_Calibrate_Center(
        g_joystick, JOYSTICK_CALIBRATION_SAMPLES, JOYSTICK_CALIBRATION_INTERVAL_MS);

    const Button_config button_config = {
        .gpio_idx = GPIO_SW_BTN_IDX,
        .gpio_state_when_pressed = bsp_gpio_state_reset,
    };
    g_confirm_button = Button_Create(&button_config);
    configASSERT(g_confirm_button != NULL);

    const Buzzer_config buzzer_config = {.pwm_idx = PWM_BUZZER_IDX};
    g_buzzer = Buzzer_Create(&buzzer_config);
    configASSERT(g_buzzer != NULL);

    const St7789_config lcd_config = {
        .spi_idx = SOFT_SPI_LCD_IDX,
        .cs_gpio_idx = (uint32_t)-1,
        .dc_gpio_idx = GPIO_TFT_DC_IDX,
        .rst_gpio_idx = GPIO_TFT_RST_IDX,
        .bkl_gpio_idx = GPIO_TFT_BLK_IDX,
        .hor_res = SCREEN_WIDTH,
        .ver_res = SCREEN_HEIGHT,
        .flags = {
            .mirror_x = LCD_MIRROR_X,
            .mirror_y = LCD_MIRROR_Y,
            .color_use_bgr = LCD_COLOR_USE_BGR,
        },
    };
    g_lcd = St7789_Create(&lcd_config);
    configASSERT(g_lcd != NULL);
    St7789_Init(g_lcd);
    St7789_Set_Backlight(g_lcd, 1);

    render_menu();
}

static void console_task(void* arg) {
    (void)arg;
    console_init();

    const Game_hardware hardware = {
        .lcd = g_lcd,
        .buzzer = g_buzzer,
    };
    uint32_t tick = xTaskGetTickCount();

    while (1) {
        const Game_input input = poll_input();
        Game_result result = game_result_running;

        if (g_console_state == console_state_menu) {
            update_menu(&input, &hardware);
        } else if (g_console_state == console_state_pacman) {
            result = Pacman_Update(&input);
        } else if (g_console_state == console_state_snake) {
            result = Snake_Update(&input);
        } else {
            result = Racing_Update(&input);
        }

        if (result == game_result_exit) { enter_menu(); }
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(20));
    }
}

void Game_Console_Task_Def(void) {
    const BaseType_t result = xTaskCreate(console_task, "Game", 768, NULL, 1, NULL);
    configASSERT(result == pdPASS);
}
