#include "test_buzzer.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "buzzer.h"
#include "task.h"

static Buzzer* g_buzzer = NULL;
static const Music_idx demo_tracks[] = {
    music_idx_menu_theme,
    music_idx_pacman_theme,
    music_idx_snake_theme,
    music_idx_racing_theme,
    music_idx_tank_theme,
    music_idx_air_theme,
    music_idx_victory,
    music_idx_defeat,
};

static void buzzer_task(void* arg) {
    (void)arg;
    const Buzzer_config cfg = {.pwm_idx = PWM_BUZZER_IDX};
    g_buzzer = Buzzer_Create(&cfg);
    configASSERT(g_buzzer != NULL);

    while (1) {
        for (uint32_t i = 0; i < sizeof(demo_tracks) / sizeof(demo_tracks[0]); i++) {
            Buzzer_Play_Music(g_buzzer, demo_tracks[i], 0);
            vTaskDelay(pdMS_TO_TICKS(1500));
            Buzzer_Play_Sfx(g_buzzer, buzzer_sfx_menu_select);
            vTaskDelay(pdMS_TO_TICKS(4500));
        }
    }
}

void Test_Buzzer_Task_Def(void) { xTaskCreate(buzzer_task, "Buzzer", 128, NULL, 1, NULL); }
