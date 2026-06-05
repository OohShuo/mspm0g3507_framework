#include "buzzer.h"

#include <stdlib.h>
#include <string.h>

#include "bsp_time.h"
#include "vector.h"

// 滑音标记：最高位置 1 表示该音符需要滑音到下一个音
#define NOTE_FLAG_GLISSANDO 0x8000

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

void Buzzer_Play(Buzzer* obj, const Music* music, uint8_t is_loop) {
    if (obj == NULL || music == NULL) return;

    obj->music_score = music->score;
    obj->score_length = music->length;
    obj->speed_npm = music->speed_npm;
    obj->is_looping = is_loop;

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

            // 解析当前音符：低 15 位为周期，最高位为滑音标记
            uint16_t raw = obj->music_score[obj->note_index];
            uint8_t has_glissando = (raw & NOTE_FLAG_GLISSANDO) ? 1 : 0;
            uint16_t period = raw & 0x7FFF;

            // 确定目标频率
            uint16_t target_freq = (period == 0) ? 0 : (1000000 / period);

            // 获取下一音符频率（滑音目标）
            uint16_t next_target_freq = target_freq;
            if (has_glissando) {
                uint16_t next_idx = obj->note_index + 1;
                if (next_idx < obj->score_length) {
                    uint16_t next_raw = obj->music_score[next_idx];
                    uint16_t next_period = next_raw & 0x7FFF;
                    next_target_freq = (next_period == 0) ? 0 : (1000000 / next_period);
                }
            }

            // 设置当前音符
            if (target_freq == 0) {
                Bsp_Pwm_Set_Duty(obj->config.pwm_idx, 0);
            } else {
                Bsp_Pwm_Set_Freq(obj->config.pwm_idx, target_freq);
                Bsp_Pwm_Set_Duty(obj->config.pwm_idx, 0.5);
            }

            if (obj->note_last != raw) {
                Bsp_Pwm_Stop(obj->config.pwm_idx);
                Bsp_Pwm_Start(obj->config.pwm_idx);
            }

            // 初始化滑音参数
            if (has_glissando && target_freq != next_target_freq) {
                obj->glissando_src_freq = target_freq;
                obj->glissando_dst_freq = next_target_freq;
                obj->glissando_start_time = now;
                obj->glissando_duration = (note_duration * 9) / 10;
            } else {
                obj->glissando_src_freq = 0;
            }

            obj->note_last = raw;
            obj->note_index++;
        }

        // 滑音插值：从 src_freq 平滑滑向 dst_freq
        if (obj->glissando_src_freq != 0) {
            uint32_t elapsed = now - obj->glissando_start_time;
            if (elapsed < obj->glissando_duration && obj->glissando_duration > 0) {
                uint16_t freq =
                    obj->glissando_src_freq + (int32_t)(obj->glissando_dst_freq - obj->glissando_src_freq) *
                                                  elapsed / obj->glissando_duration;
                Bsp_Pwm_Set_Freq(obj->config.pwm_idx, freq);
            } else {
                obj->glissando_src_freq = 0;
            }
        }
    }
}