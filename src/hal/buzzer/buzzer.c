#include "buzzer.h"

#include <stdlib.h>
#include <string.h>

#include "bsp_time.h"
#include "vector.h"

static Vector* buzzer_instances = NULL;

void Buzzer_Init(void) {
    if (buzzer_instances == NULL) { buzzer_instances = Vector_Init(sizeof(Buzzer*), 4); }
}

Buzzer* Buzzer_Create(const Buzzer_config* config) {
    if (config == NULL || buzzer_instances == NULL) return NULL;

    Buzzer* obj = malloc(sizeof(Buzzer));
    if (obj == NULL) return NULL;

    memset(obj, 0, sizeof(Buzzer));
    obj->config = *config;

    Vector_Push_Back(buzzer_instances, (void*)&obj);
    return obj;
}

void Buzzer_Play(Buzzer* obj, const uint16_t* score, uint16_t length, uint16_t speed_npm, uint8_t loop) {
    if (obj == NULL || score == NULL) return;

    obj->music_score = score;
    obj->score_length = length;
    obj->speed_npm = speed_npm;
    obj->is_looping = loop;

    obj->is_playing = 1;
    obj->note_index = 0;
    obj->last_note_time = 0;

    Bsp_Pwm_Start(obj->config.pwm_idx);
}

void Buzzer_Stop(Buzzer* obj) {
    if (obj == NULL) return;
    obj->is_playing = 0;
    Bsp_Pwm_Stop(obj->config.pwm_idx);
}

void Buzzer_Update_All(void) {
    if (buzzer_instances == NULL) return;

    for (uint32_t i = 0; i < Vector_Get_Size(buzzer_instances); i++) {
        Buzzer* obj = *(Buzzer**)Vector_Get_At(buzzer_instances, i);
        if (obj == NULL || !obj->is_playing) continue;

        uint32_t now = Bsp_Get_Tick_Ms();
        uint32_t note_duration = 60000 / obj->speed_npm;

        if (now - obj->last_note_time >= note_duration) {
            obj->last_note_time = now;

            if (obj->note_index >= obj->score_length) {
                if (obj->is_looping) {
                    obj->note_index = 0;
                } else {
                    Buzzer_Stop(obj);
                    continue;
                }
            }

            obj->note_now = obj->music_score[obj->note_index];

            if (obj->note_now == 0) {
                Bsp_Pwm_Set_Duty(obj->config.pwm_idx, 0);
            } else {
                Bsp_Pwm_Set_Freq(obj->config.pwm_idx, 1000000 / obj->note_now);
                Bsp_Pwm_Set_Duty(obj->config.pwm_idx, 0.5);
            }

            if (obj->note_last != obj->note_now) {
                // 改变频率时重启，否则有概率寄掉
                Bsp_Pwm_Stop(obj->config.pwm_idx);
                Bsp_Pwm_Start(obj->config.pwm_idx);
            }

            obj->note_last = obj->note_now;
            obj->note_index++;
        }
    }
}