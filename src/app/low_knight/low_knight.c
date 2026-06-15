#include "low_knight.h"

#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "app_config.h"

#if LOW_KNIGHT_STANDALONE_ENABLE

#include "board_config.h"
#include "bsp_time.h"
#include "button.h"
#include "buzzer.h"
#include "game_graphics.h"
#include "joystick.h"
#include "low_knight_resources.h"
#include "low_knight_runtime.h"
#include "rtt_log.h"
#include "st7789.h"
#include "storage.h"
#include "task.h"

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xffffu
#define COLOR_RED     0xf800u
#define COLOR_GREEN   0x07e0u
#define COLOR_YELLOW  0xffe0u
#define COLOR_CYAN    0x07ffu
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0x1082u

#define LOW_KNIGHT_FRAME_MS 33u
#define JOYSTICK_MOVE_THRESHOLD 0.45f

static St7789* g_lcd = NULL;
static Joystick* g_joystick = NULL;
static Button* g_button = NULL;
static Buzzer* g_buzzer = NULL;
static Low_Knight_Resources g_resources;

static uint8_t read_jump_down(void) {
    return Button_Get_State(g_button) == button_state_down ||
           Bsp_Gpio_Read(GPIO_SW_BTN_IDX) == bsp_gpio_state_reset;
}

static void draw_centered(const char* text, int32_t y, uint8_t scale, uint16_t color) {
    int32_t length = 0;
    while (text[length] != '\0') { length++; }
    const int32_t width = length * 6 * scale;
    Game_Graphics_Draw_Text(g_lcd, (SCREEN_WIDTH - width) / 2, y, text, scale, color);
}

static void draw_boot_screen(uint8_t ready) {
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    draw_centered("LOW KNIGHT", 18, 2, COLOR_CYAN);

    if (!ready) {
        draw_centered("RESOURCE MISS", 82, 2, COLOR_RED);
        Game_Graphics_Draw_Text(g_lcd, 20, 130, "UPLOAD", 1, COLOR_WHITE);
        Game_Graphics_Draw_Text(g_lcd, 20, 148, "LOW-KNIGHT-P8R", 1, COLOR_YELLOW);
        Game_Graphics_Draw_Text(g_lcd, 20, 174, "TO LITTLEFS", 1, COLOR_WHITE);
        Game_Graphics_Draw_Text(g_lcd, 20, 192, "PATH", 1, COLOR_WHITE);
        Game_Graphics_Draw_Text(g_lcd, 72, 192, "LOW-KNIGHT", 1, COLOR_CYAN);
        return;
    }

    draw_centered("RESOURCE OK", 64, 2, COLOR_GREEN);
    Game_Graphics_Draw_Text(g_lcd, 26, 104, "GFX", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, 88, 104, g_resources.gfx_size, 5, 1, COLOR_YELLOW);
    Game_Graphics_Draw_Text(g_lcd, 26, 124, "GFF", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, 88, 124, g_resources.gff_size, 5, 1, COLOR_YELLOW);
    Game_Graphics_Draw_Text(g_lcd, 26, 144, "MAP", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, 88, 144, g_resources.map_size, 5, 1, COLOR_YELLOW);
    Game_Graphics_Draw_Text(g_lcd, 26, 164, "CRC", 1, COLOR_WHITE);
    Game_Graphics_Draw_U32(g_lcd, 88, 164, g_resources.crc16, 5, 1, COLOR_YELLOW);
}

static void low_knight_init_hardware(void) {
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
        .gpio_idx = GPIO_SW_BTN_IDX,
        .gpio_state_when_pressed = bsp_gpio_state_reset,
    };
    g_button = Button_Create(&button_config);
    configASSERT(g_button != NULL);

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
        .flags =
            {
                .mirror_x = LCD_MIRROR_X,
                .mirror_y = LCD_MIRROR_Y,
                .color_use_bgr = LCD_COLOR_USE_BGR,
            },
    };
    g_lcd = St7789_Create(&lcd_config);
    configASSERT(g_lcd != NULL);
    St7789_Init(g_lcd);
    St7789_Set_Backlight(g_lcd, 1);
}

static void low_knight_task(void* arg) {
    (void)arg;
    low_knight_init_hardware();

    uint8_t ready = Low_Knight_Resources_Open(&g_resources, LOW_KNIGHT_RESOURCE_PATH);
    draw_boot_screen(ready);
    if (ready && Low_Knight_Runtime_Init(&g_resources)) {
        Low_Knight_Runtime_Draw(g_lcd);
    } else {
        ready = 0;
    }
    uint32_t tick = xTaskGetTickCount();
    uint8_t last_jump_down = 0;

    while (1) {
        if (ready) {
            const uint8_t jump_down = read_jump_down();
            Low_Knight_Input input = {
                .move_x = 0,
                .jump_down = jump_down,
                .jump_pressed = jump_down && !last_jump_down,
                .jump_released = !jump_down && last_jump_down,
            };
            if (g_joystick->x_value < -JOYSTICK_MOVE_THRESHOLD) { input.move_x = -1; }
            if (g_joystick->x_value > JOYSTICK_MOVE_THRESHOLD) { input.move_x = 1; }

            const Low_Knight_Step_Result step_result = Low_Knight_Runtime_Step(&input);
            if (step_result == low_knight_step_full) {
                Low_Knight_Runtime_Draw(g_lcd);
            } else if (step_result == low_knight_step_dirty) {
                Low_Knight_Runtime_Draw_Dirty(g_lcd);
            }
            last_jump_down = jump_down;
        } else if (Storage_Is_Available()) {
            ready = Low_Knight_Resources_Open(&g_resources, LOW_KNIGHT_RESOURCE_PATH) &&
                    Low_Knight_Runtime_Init(&g_resources);
            if (ready) {
                Low_Knight_Runtime_Draw(g_lcd);
                last_jump_down = 0;
            } else {
                draw_boot_screen(0);
            }
        }

        (void)g_joystick;
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(LOW_KNIGHT_FRAME_MS));
    }
}

void Low_Knight_Task_Def(void) {
    const BaseType_t result = xTaskCreate(low_knight_task, "LowKnight", 768, NULL, 1, NULL);
    configASSERT(result == pdPASS);
}

#else

void Low_Knight_Task_Def(void) { }

#endif
