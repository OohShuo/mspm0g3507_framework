#include "audio_player.h"

#include <stdint.h>
#include <string.h>

#include "app_config.h"

#if AUDIO_PLAYER_ENABLE

#include "FreeRTOS.h"
#include "board_config.h"
#include "bsp_pwm.h"
#include "dl_gpio.h"
#include "dl_timerg.h"
#include "storage.h"
#include "task.h"
#include "ti_msp_dl_config.h"

#if FRAMEWORK_USE_LFS
#include "lfs.h"
#endif

#define AUDIO_HEADER_SIZE        16u
#define AUDIO_CODEC_PCM8         1u
#define AUDIO_CODEC_PDM1         2u
#define AUDIO_CODEC_ADPCM4       3u
#define AUDIO_BUFFER_SIZE        256u
#define AUDIO_BUFFER_COUNT       2u
#define AUDIO_PCM_PWM_CARRIER_HZ 31250u
#define AUDIO_TIMER_CLOCK_HZ     40000000u

typedef struct {
    uint8_t data[AUDIO_BUFFER_SIZE];
    volatile uint16_t length;
    volatile uint8_t ready;
    volatile uint8_t reset_decoder;
} Audio_buffer;

static Audio_buffer g_buffers[AUDIO_BUFFER_COUNT];
static volatile uint8_t g_active_buffer = 0;
static volatile uint16_t g_active_byte = 0;
static volatile uint8_t g_active_bit = 0;
static volatile uint8_t g_active_nibble = 0;
static volatile uint8_t g_running = 0;
static volatile uint8_t g_finished = 0;
static uint8_t g_codec = 0;
static uint16_t g_sample_rate = 0;
static uint32_t g_data_size = 0;
static uint32_t g_data_read = 0;
static int16_t g_adpcm_predictor = 0;
static uint8_t g_adpcm_step_index = 0;

static const int8_t g_ima_index_table[8] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
};

static const uint16_t g_ima_step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
};

#if FRAMEWORK_USE_LFS
static lfs_file_t g_file;
static uint8_t g_file_open = 0;
#endif

static uint16_t read_u16_le(const uint8_t* data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_u32_le(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void output_silence(void) {
    if (g_codec == AUDIO_CODEC_PCM8 ||
        g_codec == AUDIO_CODEC_ADPCM4) {
        Bsp_Pwm_Set_Duty_U8(PWM_BUZZER_IDX, 128u);
    } else {
        DL_GPIO_clearPins(GPIO_PWM_0_C3_PORT, GPIO_PWM_0_C3_PIN);
    }
}

static void configure_output(void) {
    Bsp_Pwm_Stop(PWM_BUZZER_IDX);
    if (g_codec == AUDIO_CODEC_PCM8 ||
        g_codec == AUDIO_CODEC_ADPCM4) {
        DL_GPIO_initPeripheralOutputFunction(
            GPIO_PWM_0_C3_IOMUX, GPIO_PWM_0_C3_IOMUX_FUNC);
        Bsp_Pwm_Set_Freq(PWM_BUZZER_IDX, AUDIO_PCM_PWM_CARRIER_HZ);
        Bsp_Pwm_Set_Duty_U8(PWM_BUZZER_IDX, 128u);
        Bsp_Pwm_Start(PWM_BUZZER_IDX);
    } else {
        DL_GPIO_initDigitalOutput(GPIO_PWM_0_C3_IOMUX);
        DL_GPIO_clearPins(GPIO_PWM_0_C3_PORT, GPIO_PWM_0_C3_PIN);
        DL_GPIO_enableOutput(GPIO_PWM_0_C3_PORT, GPIO_PWM_0_C3_PIN);
    }
}

static void configure_sample_timer(void) {
    DL_TimerG_reset(TIMG6);
    DL_TimerG_enablePower(TIMG6);
    delay_cycles(POWER_STARTUP_DELAY);

    const DL_TimerG_ClockConfig clock_config = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = 0u,
    };
    const uint32_t period =
        (AUDIO_TIMER_CLOCK_HZ + g_sample_rate / 2u) / g_sample_rate - 1u;
    const DL_TimerG_TimerConfig timer_config = {
        .timerMode = DL_TIMER_TIMER_MODE_PERIODIC,
        .period = period,
        .startTimer = DL_TIMER_STOP,
        .genIntermInt = DL_TIMER_INTERM_INT_DISABLED,
        .counterVal = 0u,
    };

    DL_TimerG_setClockConfig(TIMG6, &clock_config);
    DL_TimerG_initTimerMode(TIMG6, &timer_config);
    DL_TimerG_enableInterrupt(TIMG6, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_enableClock(TIMG6);
    NVIC_ClearPendingIRQ(TIMG6_INT_IRQn);
    NVIC_EnableIRQ(TIMG6_INT_IRQn);
}

#if FRAMEWORK_USE_LFS
static uint16_t fill_buffer(Audio_buffer* buffer) {
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL || buffer == NULL || !g_file_open || buffer->ready) {
        return 0;
    }

    uint8_t reset_decoder = 0;
    if (g_data_read >= g_data_size) {
#if AUDIO_PLAYER_LOOP
        Storage_Lock();
        const lfs_soff_t seek_result =
            lfs_file_seek(lfs, &g_file, AUDIO_HEADER_SIZE, LFS_SEEK_SET);
        Storage_Unlock();
        if (seek_result != AUDIO_HEADER_SIZE) {
            g_finished = 1;
            return 0;
        }
        g_data_read = 0;
        reset_decoder = 1;
#else
        g_finished = 1;
        return 0;
#endif
    }

    uint32_t read_size = g_data_size - g_data_read;
    if (read_size > AUDIO_BUFFER_SIZE) { read_size = AUDIO_BUFFER_SIZE; }
    Storage_Lock();
    const lfs_ssize_t result =
        lfs_file_read(lfs, &g_file, buffer->data, read_size);
    Storage_Unlock();
    if (result <= 0) {
        g_finished = 1;
        return 0;
    }

    buffer->length = (uint16_t)result;
    buffer->reset_decoder = reset_decoder;
    buffer->ready = 1;
    g_data_read += (uint32_t)result;
    return (uint16_t)result;
}

static uint8_t open_audio(void) {
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs == NULL) { return 0; }

    uint8_t header[AUDIO_HEADER_SIZE];
    Storage_Lock();
    int result =
        lfs_file_open(lfs, &g_file, AUDIO_PLAYER_PATH, LFS_O_RDONLY);
    const uint8_t opened = result == 0;
    if (result == 0) {
        result = (int)lfs_file_read(
            lfs, &g_file, header, sizeof(header));
    }
    const lfs_soff_t file_size =
        result == (int)sizeof(header)
            ? lfs_file_size(lfs, &g_file)
            : -1;
    Storage_Unlock();

    if (result != (int)sizeof(header) ||
        memcmp(header, "AUD1", 4) != 0 || header[4] != 1u) {
        if (opened) {
            Storage_Lock();
            lfs_file_close(lfs, &g_file);
            Storage_Unlock();
        }
        return 0;
    }

    g_codec = header[5];
    g_sample_rate = read_u16_le(header + 6);
    const uint32_t sample_count = read_u32_le(header + 8);
    g_data_size = read_u32_le(header + 12);
    uint32_t expected_data_size = sample_count;
    if (g_codec == AUDIO_CODEC_PDM1 &&
        sample_count <= UINT32_MAX - 7u) {
        expected_data_size = (sample_count + 7u) / 8u;
    }
    if ((g_codec != AUDIO_CODEC_PCM8 &&
         g_codec != AUDIO_CODEC_PDM1 &&
         g_codec != AUDIO_CODEC_ADPCM4) ||
        (g_codec == AUDIO_CODEC_PCM8 && g_sample_rate != 8000u) ||
        (g_codec == AUDIO_CODEC_ADPCM4 && g_sample_rate != 8000u) ||
        (g_codec == AUDIO_CODEC_PDM1 && g_sample_rate != 32000u) ||
        sample_count == 0u ||
        (g_codec == AUDIO_CODEC_PDM1 && sample_count > UINT32_MAX - 7u) ||
        (g_codec == AUDIO_CODEC_ADPCM4
             ? (sample_count > UINT32_MAX - 1u ||
               g_data_size != (sample_count + 1u) / 2u)
             : g_data_size != expected_data_size) ||
        file_size < (lfs_soff_t)(AUDIO_HEADER_SIZE + g_data_size)) {
        Storage_Lock();
        lfs_file_close(lfs, &g_file);
        Storage_Unlock();
        return 0;
    }

    g_file_open = 1;
    g_data_read = 0;
    memset(g_buffers, 0, sizeof(g_buffers));
    (void)fill_buffer(&g_buffers[0]);
    g_buffers[0].reset_decoder = 1;
    (void)fill_buffer(&g_buffers[1]);
    return g_buffers[0].ready;
}

static void close_audio(void) {
    if (!g_file_open) { return; }
    lfs_t* lfs = Storage_Get_Lfs();
    if (lfs != NULL) {
        Storage_Lock();
        lfs_file_close(lfs, &g_file);
        Storage_Unlock();
    }
    g_file_open = 0;
}
#endif

static void start_audio(void) {
    g_active_buffer = 0;
    g_active_byte = 0;
    g_active_bit = 0;
    g_active_nibble = 0;
    g_adpcm_predictor = 0;
    g_adpcm_step_index = 0;
    g_finished = 0;
    configure_output();
    configure_sample_timer();
    g_running = 1;
    DL_TimerG_startCounter(TIMG6);
}

static void stop_audio(void) {
    g_running = 0;
    DL_TimerG_stopCounter(TIMG6);
    NVIC_DisableIRQ(TIMG6_INT_IRQn);
    output_silence();
    Bsp_Pwm_Stop(PWM_BUZZER_IDX);
}

static uint8_t decode_adpcm4(uint8_t code) {
    const int32_t step = g_ima_step_table[g_adpcm_step_index];
    int32_t difference = step >> 3;
    if (code & 1u) { difference += step >> 2; }
    if (code & 2u) { difference += step >> 1; }
    if (code & 4u) { difference += step; }

    int32_t predictor = g_adpcm_predictor;
    predictor += (code & 8u) ? -difference : difference;
    if (predictor > 32767) { predictor = 32767; }
    if (predictor < -32768) { predictor = -32768; }
    g_adpcm_predictor = (int16_t)predictor;

    int32_t step_index =
        (int32_t)g_adpcm_step_index + g_ima_index_table[code & 7u];
    if (step_index > 88) { step_index = 88; }
    if (step_index < 0) { step_index = 0; }
    g_adpcm_step_index = (uint8_t)step_index;
    return (uint8_t)((predictor + 32768) >> 8);
}

void Audio_Player_Irq_Handler(void) {
    if (DL_TimerG_getPendingInterrupt(TIMG6) !=
        DL_TIMERG_INTERRUPT_ZERO_EVENT) {
        return;
    }
    if (!g_running) { return; }

    Audio_buffer* buffer = &g_buffers[g_active_buffer];
    if (!buffer->ready) {
        output_silence();
        return;
    }

    if (buffer->reset_decoder && g_active_byte == 0u &&
        g_active_nibble == 0u) {
        g_adpcm_predictor = 0;
        g_adpcm_step_index = 0;
        buffer->reset_decoder = 0;
    }

    if (g_codec == AUDIO_CODEC_PCM8) {
        Bsp_Pwm_Set_Duty_U8(
            PWM_BUZZER_IDX, buffer->data[g_active_byte++]);
    } else if (g_codec == AUDIO_CODEC_ADPCM4) {
        const uint8_t packed = buffer->data[g_active_byte];
        const uint8_t code =
            g_active_nibble == 0u ? packed & 0x0fu : packed >> 4;
        Bsp_Pwm_Set_Duty_U8(
            PWM_BUZZER_IDX, decode_adpcm4(code));
        g_active_nibble ^= 1u;
        if (g_active_nibble == 0u) { g_active_byte++; }
    } else {
        const uint8_t bit =
            (uint8_t)((buffer->data[g_active_byte] >>
                       (7u - g_active_bit)) & 1u);
        if (bit) {
            DL_GPIO_setPins(GPIO_PWM_0_C3_PORT, GPIO_PWM_0_C3_PIN);
        } else {
            DL_GPIO_clearPins(GPIO_PWM_0_C3_PORT, GPIO_PWM_0_C3_PIN);
        }
        g_active_bit++;
        if (g_active_bit == 8u) {
            g_active_bit = 0;
            g_active_byte++;
        }
    }

    if (g_active_byte >= buffer->length) {
        buffer->ready = 0;
        g_active_buffer ^= 1u;
        g_active_byte = 0;
        g_active_bit = 0;
        g_active_nibble = 0;
    }
}

static void audio_player_task(void* arg) {
    (void)arg;
#if FRAMEWORK_USE_LFS
    while (!Storage_Is_Available() || !open_audio()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    start_audio();
    while (1) {
        for (uint32_t i = 0; i < AUDIO_BUFFER_COUNT; i++) {
            if (!g_buffers[i].ready && !g_finished) {
                (void)fill_buffer(&g_buffers[i]);
            }
        }
        if (g_finished &&
            !g_buffers[0].ready && !g_buffers[1].ready) {
            stop_audio();
            close_audio();
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
#else
    vTaskDelete(NULL);
#endif
}

void Audio_Player_Task_Def(void) {
    const BaseType_t result =
        xTaskCreate(audio_player_task, "Audio", 256, NULL, 2, NULL);
    configASSERT(result == pdPASS);
}

#else

void Audio_Player_Task_Def(void) { }
void Audio_Player_Irq_Handler(void) { }

#endif
