#include "bsp_audio.h"

#include "board_config.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "devices/msp/peripherals/hw_gptimer.h"
#include "dl_timera.h"
#include "dl_timerg.h"
#include "ti_msp_dl_config.h"

/* ── Hardware instances ─────────────────────────────────────────────── */

#define AUDIO_CARRIER_TIMA   TIMA0   /* same timer as buzzer PWM */
#define AUDIO_CARRIER_CLK    16000000 /* BUSCLK(32MHz) / DIVIDE_2 = 16 MHz */
#define AUDIO_CARRIER_FREQ   20000    /* 20 kHz — 实验: 可尝试 8000-15000 之间,
                                       * 越低振幅分辨率越高(2000 steps at 8kHz),
                                       * 但可能引入可闻载波噪声 */
#define AUDIO_CARRIER_PERIOD (AUDIO_CARRIER_CLK / AUDIO_CARRIER_FREQ) /* = 800 */

#define AUDIO_SAMPLE_TIMG    TIMG6
#define AUDIO_SAMPLE_CLK     16000000 /* BUSCLK(32MHz) / DIVIDE_2 = 16 MHz */
#define AUDIO_SAMPLE_IRQn    TIMG6_INT_IRQn

/* ── State ──────────────────────────────────────────────────────────── */

static volatile uint8_t  audio_playing  = 0;
static volatile uint32_t sample_idx     = 0;
static volatile uint32_t sample_len     = 0;
static const uint8_t*    sample_data    = NULL;   /* ROM buffer mode */
static volatile uint8_t  sample_loop    = 0;

/* Double-buffer / callback mode */
static Bsp_Audio_Callback audio_callback = NULL;
static uint8_t*           cb_buffer      = NULL;
static uint32_t           cb_buf_len     = 0;
static volatile uint32_t  cb_write_idx  = 0;   /* callback fills here */
static volatile uint32_t  cb_read_idx   = 0;   /* ISR reads from here */
static volatile uint32_t  cb_fill_count = 0;   /* samples available */

/* ── Carrier (TIMA0) reconfiguration ────────────────────────────────── */

static void Audio_Carrier_Start(void) {
    /* Reconfigure TIMA0 clock: BUSCLK(32MHz) / 2 = 16 MHz timer clock */
    DL_TimerA_ClockConfig clk = {
        .clockSel     = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio  = DL_TIMER_CLOCK_DIVIDE_2,
        .prescale     = 0U,
    };
    DL_TimerA_setClockConfig(AUDIO_CARRIER_TIMA, &clk);

    /* Period = 800 → 20 kHz carrier (16 MHz / 800 = 20 kHz) */
    DL_Timer_setLoadValue(AUDIO_CARRIER_TIMA, AUDIO_CARRIER_PERIOD - 1);

    /* Start with 50 % duty = silence (buzzer centered) */
    DL_Timer_setCaptureCompareValue(
        AUDIO_CARRIER_TIMA, AUDIO_CARRIER_PERIOD / 2, DL_TIMER_CC_3_INDEX);

    /* No TIMA0 interrupt needed — duty is updated by TIMG6 sample clock ISR */
    DL_Timer_startCounter(AUDIO_CARRIER_TIMA);
}

static void Audio_Carrier_Stop(void) {
    DL_Timer_stopCounter(AUDIO_CARRIER_TIMA);

    /* Restore to original music-mode config: SYSCLK / 8 = 4 MHz, prescale 0 */
    DL_TimerA_ClockConfig clk = {
        .clockSel     = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio  = DL_TIMER_CLOCK_DIVIDE_8,
        .prescale     = 0U,
    };
    DL_TimerA_setClockConfig(AUDIO_CARRIER_TIMA, &clk);

    /* Default period for ~1 kHz (will be overridden by Buzzer_Play) */
    DL_Timer_setLoadValue(AUDIO_CARRIER_TIMA, 4000 - 1);
    DL_Timer_setCaptureCompareValue(
        AUDIO_CARRIER_TIMA, 2000, DL_TIMER_CC_3_INDEX);
}

/* ── Sample clock (TIMG6) ───────────────────────────────────────────── */

static void Audio_SampleClock_Start(uint32_t sample_rate) {
    /* Enable TIMG6 power (may not be on yet) */
    DL_TimerG_enablePower(AUDIO_SAMPLE_TIMG);
    delay_cycles(POWER_STARTUP_DELAY);

    /* Clock: BUSCLK(32MHz) / 2 = 16 MHz */
    DL_TimerG_ClockConfig clk = {
        .clockSel     = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio  = DL_TIMER_CLOCK_DIVIDE_2,
        .prescale     = 0U,
    };
    DL_TimerG_setClockConfig(AUDIO_SAMPLE_TIMG, &clk);

    /* Period: 8 MHz / sample_rate */
    uint32_t period = AUDIO_SAMPLE_CLK / sample_rate;
    DL_Timer_setLoadValue(AUDIO_SAMPLE_TIMG, period - 1);

    /* Periodic mode, don't start yet */
    DL_Timer_TimerConfig tcfg = {
        .timerMode    = DL_TIMER_TIMER_MODE_PERIODIC,
        .period       = period - 1,
        .startTimer   = DL_TIMER_STOP,
        .genIntermInt = DL_TIMER_INTERM_INT_DISABLED,
        .counterVal   = 0,
    };
    DL_Timer_initTimerMode(AUDIO_SAMPLE_TIMG, &tcfg);

    /* Enable zero-event interrupt */
    DL_Timer_clearInterruptStatus(AUDIO_SAMPLE_TIMG, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_Timer_enableInterrupt(AUDIO_SAMPLE_TIMG, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(AUDIO_SAMPLE_IRQn);

    DL_Timer_startCounter(AUDIO_SAMPLE_TIMG);
}

static void Audio_SampleClock_Stop(void) {
    DL_Timer_stopCounter(AUDIO_SAMPLE_TIMG);
    DL_Timer_disableInterrupt(AUDIO_SAMPLE_TIMG, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(AUDIO_SAMPLE_IRQn);
}

/* ── Inline helper: set carrier duty from 8-bit sample ──────────────── */

static inline void Audio_Set_Sample(uint8_t sample) {
    /*
     * Map 8-bit sample to full PWM range (0 to PERIOD-1).
     * 0 → silence, 255 → maximum amplitude.
     * The buzzer membrane responds to duty cycle CHANGE around 50%,
     * so we want maximum swing for best volume.
     */
    uint32_t duty = ((uint32_t)sample * AUDIO_CARRIER_PERIOD) >> 8;
    DL_Timer_setCaptureCompareValue(AUDIO_CARRIER_TIMA, duty, DL_TIMER_CC_3_INDEX);
}

/* ── Public API ──────────────────────────────────────────────────────── */

void Bsp_Audio_Init(void) {
    /* Nothing to do until playback starts */
}

void Bsp_Audio_Start(Bsp_Audio_Callback callback,
                     uint8_t* buffer, uint32_t buf_len,
                     uint32_t sample_rate) {
    if (audio_playing) Bsp_Audio_Stop();

    audio_callback = callback;
    cb_buffer      = buffer;
    cb_buf_len     = buf_len;
    cb_write_idx   = 0;
    cb_read_idx    = 0;
    cb_fill_count  = 0;
    sample_data    = NULL;
    sample_len     = 0;
    sample_idx     = 0;
    sample_loop    = 0;

    /* Pre-fill: let the callback fill the first half */
    if (audio_callback && cb_buffer && cb_buf_len >= 2) {
        audio_callback(cb_buffer, cb_buf_len / 2);
        cb_fill_count = cb_buf_len / 2;
    }

    audio_playing = 1;

    Audio_Carrier_Start();
    Audio_SampleClock_Start(sample_rate);
}

void Bsp_Audio_Start_Buffer(const uint8_t* data, uint32_t length,
                            uint32_t sample_rate) {
    if (audio_playing) Bsp_Audio_Stop();

    audio_callback = NULL;
    cb_buffer      = NULL;
    cb_buf_len     = 0;
    sample_data    = data;
    sample_len     = length;
    sample_idx     = 0;
    sample_loop    = 0;

    audio_playing = 1;

    Audio_Carrier_Start();
    Audio_SampleClock_Start(sample_rate);
}

void Bsp_Audio_Stop(void) {
    audio_playing = 0;

    Audio_SampleClock_Stop();
    Audio_Carrier_Stop();
}

uint8_t Bsp_Audio_Is_Playing(void) {
    return audio_playing;
}

/* ── TIMG6 ISR: sample clock ─────────────────────────────────────────── */

void TIMG6_IRQHandler(void) {
    DL_Timer_clearInterruptStatus(AUDIO_SAMPLE_TIMG, DL_TIMER_INTERRUPT_ZERO_EVENT);

    if (!audio_playing) return;

    uint8_t sample = 128; /* default silence */

    if (audio_callback && cb_buffer && cb_buf_len) {
        /* ── Callback / double-buffer mode ── */
        if (cb_fill_count > 0) {
            sample = cb_buffer[cb_read_idx];
            cb_read_idx = (cb_read_idx + 1) % cb_buf_len;
            cb_fill_count--;

            /* When buffer is half-empty, ask callback to refill */
            if (cb_fill_count <= cb_buf_len / 4) {
                uint32_t fill_start = cb_write_idx;
                uint32_t fill_count = cb_buf_len / 2;
                audio_callback(&cb_buffer[fill_start], fill_count);
                cb_write_idx = (cb_write_idx + fill_count) % cb_buf_len;
                cb_fill_count += fill_count;
            }
        }
    } else if (sample_data && sample_len) {
        /* ── ROM buffer mode ── */
        if (sample_idx < sample_len) {
            sample = sample_data[sample_idx];
            sample_idx++;
        } else if (sample_loop) {
            sample_idx = 0;
            sample = sample_data[0];
        } else {
            /* Finished: stop everything from ISR to silence immediately */
            audio_playing = 0;
            Audio_SampleClock_Stop();
            Audio_Carrier_Stop();
            return;
        }
    }

    Audio_Set_Sample(sample);
}
