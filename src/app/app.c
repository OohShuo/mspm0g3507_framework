#include "app.h"

#include <stdint.h>
#include <stdlib.h>

#include "board_config.h"
#include "bsp_time.h"
#include "button.h"
#include "buzzer.h"
#include "joystick.h"
#include "led_breath.h"
#include "led_simple.h"

Led_simple* led_indicator = NULL;
Led_breath* led_breath = NULL;
Button* button1 = NULL;
Button* button2 = NULL;
Buzzer* buzzer = NULL;
Joystick* joystick = NULL;

// 音乐列表
static const uint16_t* music_list[] = {
    music1, music2, music3, NULL, music5, music6, music7,
    music8, music9, music10, music11, music12, music13, music14,
    music15, music16, music17,
};
static const uint8_t music_len_list[] = {63, 21, 15, 0, 61, 38, 59, 64, 47, 44, 48, 40, 59, 133, 27, 34, 26};
static const uint16_t music_speed_list[] = {
    60 * 3, 60 * 6, 60 * 4, 0, 60 * 5, 60 * 3, 60 * 4,
    60 * 4, 60 * 3, 60 * 6, 60 * 4, 60 * 3, 60 * 3, 60 * 3,
    60 * 2 + 30, 60 * 3 + 20, 60 * 3,
};
#define MUSIC_COUNT 17
#define MUSIC_STANDBY_IDX 3

static uint8_t current_music = 0;

static void Play_Current_Music(void) {
    if (current_music == MUSIC_STANDBY_IDX) {
        Buzzer_Stop(buzzer);
    } else {
        Buzzer_Play(buzzer, music_list[current_music], music_len_list[current_music],
                    music_speed_list[current_music], 1);
    }
}

void App_Init(void) {
    // Led_simple_config led_cfg = {
    //     .gpio_idx = 0, .use_as_indicator = 1, .blink_freq_hz = 2, .gpio_state_when_on =
    //     bsp_gpio_state_set};
    // led_indicator = Led_Simple_Create(&led_cfg);

    // Led_breath_config led_breath_cfg = {.pwm_idx = 0, .max_brightness = 100, .breath_freq_hz = 1.0f};
    // led_breath = Led_Breath_Create(&led_breath_cfg);

    Button_config btn_cfg = {.gpio_idx = 1, .gpio_state_when_pressed = bsp_gpio_state_set};
    button1 = Button_Create(&btn_cfg);
    btn_cfg.gpio_idx = 0;
    btn_cfg.gpio_state_when_pressed = bsp_gpio_state_reset;
    button2 = Button_Create(&btn_cfg);

    Buzzer_config buzzer_cfg = {
        .pwm_idx = 0,
    };
    buzzer = Buzzer_Create(&buzzer_cfg);

    Joystick_config joystick_cfg = {
        .adc_idx = ADC_JOYSTICK_IDX,
        .adc_channel_x = ADC_JOYSTICK_X_CHANNEL,
        .adc_channel_y = ADC_JOYSTICK_Y_CHANNEL,
        .x_offset = -0.0178018045,
        .y_offset = -0.0148155261,
    };
    joystick = Joystick_Create(&joystick_cfg);

    // 上电自动播放主题曲（循环）
    Play_Current_Music();
}

void App_Loop(void) {
    static Button_state last_button_state = button_state_up;

    // 按下 button1 切换到下一首循环播放
    if (Button_Get_State(button1) != last_button_state) {
        last_button_state = Button_Get_State(button1);
        if (last_button_state == button_state_down) {
            current_music = (current_music + 1) % MUSIC_COUNT;
            Play_Current_Music();
        }
    }
}
