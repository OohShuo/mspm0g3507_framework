#include "app.h"

#include <stdint.h>
#include <stdlib.h>

#include "board_config.h"
#include "bsp_audio.h"
#include "bsp_time.h"
#include "button.h"
#include "buzzer.h"
#include "joystick.h"
#include "led_breath.h"
#include "led_simple.h"
#include "synth.h"

Led_simple* led_indicator = NULL;
Led_breath* led_breath = NULL;
Button* button1 = NULL;
// Button* button2 = NULL;
Buzzer* buzzer = NULL;
Joystick* joystick = NULL;

/* ─── Audio state machine ────────────────────────────────────────────── */
typedef enum {
    AUDIO_MODE_NONE,     /* silence */
    AUDIO_MODE_SYNTH,    /* polyphonic synthesizer */
    AUDIO_MODE_SCORE,    /* legacy square-wave score (Buzzer_Play) */
    AUDIO_MODE_PCM,      /* raw PCM WAV playback */
} AudioMode;

static AudioMode current_mode = AUDIO_MODE_NONE;

void App_Init(void) {
    Button_config btn_cfg = {.gpio_idx = GPIO_SW_BTN_IDX, .gpio_state_when_pressed = bsp_gpio_state_reset};
    button1 = Button_Create(&btn_cfg);

    Buzzer_config buzzer_cfg = {
        .pwm_idx = PWM_BUZZER_IDX,
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

    /* ─── Init synth engine ──────────────────────────────────────────── */
    Synth_Init();

    /* ─── 上电自动播放合成器音乐（支持和弦、多音色） ─── */
    Synth_PlaySong(synth_demo_song);
    current_mode = AUDIO_MODE_SYNTH;
}

void App_Loop(void) {
    /* ─── Step the synth sequencer (drives song playback) ───────────── */
    if (Synth_IsSongPlaying()) {
        Synth_Update();
    }

    /* ─── Button: cycle through audio modes ─────────────────────────── */
    static Button_state last_button_state = button_state_up;

    if (Button_Get_State(button1) != last_button_state) {
        last_button_state = Button_Get_State(button1);
        if (last_button_state == button_state_down) {
            /* Stop everything first */
            Synth_StopSong();
            Bsp_Audio_Stop();

            /* Cycle: SYNTH → SCORE → PCM → NONE → SYNTH → ... */
            switch (current_mode) {
            case AUDIO_MODE_SYNTH:
                /* Switch to legacy score player (monophonic) */
                Buzzer_Play(buzzer, &music_library[music_idx_main_theme], 1);
                current_mode = AUDIO_MODE_SCORE;
                break;

            case AUDIO_MODE_SCORE:
                /* Switch back to synth (chords + melody) */
                Synth_PlaySong(synth_demo_song);
                current_mode = AUDIO_MODE_SYNTH;
                break;

            case AUDIO_MODE_NONE:
            case AUDIO_MODE_PCM:
            default:
                /* Start synth */
                Synth_PlaySong(synth_demo_song);
                current_mode = AUDIO_MODE_SYNTH;
                break;
            }
        }
    }
}