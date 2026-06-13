#include "bsp_rz.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "devices/msp/peripherals/hw_gptimer.h"
#include "dl_timera.h"
#include "dl_timerg.h"
#include "task.h"

#if RZ_NUM

struct Bsp_rz_instances_t {
    GPTIMER_Regs* timer;
    DL_TIMER_CC_INDEX pwm_channel;
    uint32_t clk_freq;

    volatile uint8_t tx_in_progress;
    uint8_t code_data[RZ_CODE_BUF_SIZE];
    uint32_t data_len;
    uint32_t bit_pos;

    Bsp_rz_config config;
    uint32_t period_cnt;
    uint32_t one_code_ccr;
    uint32_t zero_code_ccr;
    uint32_t reset_period_num;
    TickType_t last_done_tick;
};

static struct Bsp_rz_instances_t bsp_rz_instances[RZ_NUM] = {0};

static inline uint16_t get_ccr_for_bit(struct Bsp_rz_instances_t* r, uint32_t bit_pos) {
    uint32_t byte_idx = bit_pos >> 3;
    uint8_t bit_idx = (uint8_t)(7 - (bit_pos & 0x7));
    uint8_t byte = r->code_data[byte_idx];
    return (byte & (1 << bit_idx)) ? (uint16_t)r->one_code_ccr : (uint16_t)r->zero_code_ccr;
}

void Bsp_Rz_Init(void) {
    for (uint32_t i = 0; i < RZ_NUM; i++) {
        bsp_rz_instances[i].timer = ((GPTIMER_Regs*[])RZ_PWM_TIMERS)[i];
        bsp_rz_instances[i].pwm_channel = ((DL_TIMER_CC_INDEX[])RZ_PWM_CHANNELS)[i];
        bsp_rz_instances[i].clk_freq = ((uint32_t[])RZ_PWM_CLK_FREQS)[i];
    }
}

void Bsp_Rz_Set_Config(uint32_t idx, Bsp_rz_config* config) {
    if (idx >= RZ_NUM) return;

    struct Bsp_rz_instances_t* r = &bsp_rz_instances[idx];
    r->config = *config;

    r->period_cnt = (uint32_t)((uint64_t)r->config.period_ns * r->clk_freq / 1000000000ULL);
    r->one_code_ccr = (uint32_t)((uint64_t)r->config.one_code.high_ns * r->clk_freq / 1000000000ULL);
    r->zero_code_ccr = (uint32_t)((uint64_t)r->config.zero_code.high_ns * r->clk_freq / 1000000000ULL);
}

void Bsp_Rz_Start(uint32_t idx, uint8_t* data, uint32_t len) {
    if (idx >= RZ_NUM) return;
    if (len > RZ_CODE_BUF_SIZE) return;

    struct Bsp_rz_instances_t* r = &bsp_rz_instances[idx];
    if (r->tx_in_progress) return;

    /* Enforce reset time: line must stay low for reset_required_us
     * before the next transmission can start. */
    if (r->last_done_tick != 0) {
        uint32_t reset_ms = (r->config.reset_required_us + 999) / 1000;
        TickType_t elapsed = xTaskGetTickCount() - r->last_done_tick;
        if (elapsed < pdMS_TO_TICKS(reset_ms)) return;
        r->last_done_tick = 0;
    }

    for (uint32_t i = 0; i < len; i++) { r->code_data[i] = data[i]; }
    r->data_len = len;
    r->bit_pos = 0;

    DL_Timer_setLoadValue(r->timer, r->period_cnt);
    DL_Timer_setCaptureCompareValue(r->timer, get_ccr_for_bit(r, 0), r->pwm_channel);

    r->tx_in_progress = 1;
    DL_Timer_startCounter(r->timer);
}

uint8_t Bsp_Rz_Is_Busy(uint32_t idx) {
    if (idx >= RZ_NUM) return 0;
    return bsp_rz_instances[idx].tx_in_progress;
}

/*
 *  Called from PWM Zero interrupt. At each counter-zero event,
 *  advance to the next bit and update the CCR. When all bits are
 *  sent, stop the counter and record the completion time.
 */
void Bsp_Rz_Iqr_Handler(uint32_t idx) {
    if (idx >= RZ_NUM) return;

    struct Bsp_rz_instances_t* r = &bsp_rz_instances[idx];
    if (!r->tx_in_progress) return;

    r->bit_pos++;
    uint32_t total_bits = r->data_len << 3;

    if (r->bit_pos < total_bits) {
        DL_Timer_setCaptureCompareValue(r->timer, get_ccr_for_bit(r, r->bit_pos), r->pwm_channel);
    } else {
        DL_Timer_stopCounter(r->timer);
        r->last_done_tick = xTaskGetTickCountFromISR();
        r->tx_in_progress = 0;
    }
}

#else

void Bsp_Rz_Init(void) {}

void Bsp_Rz_Set_Config(uint32_t idx, Bsp_rz_config* config) {
    (void)idx;
    (void)config;
}

void Bsp_Rz_Start(uint32_t idx, uint8_t* data, uint32_t len) {
    (void)idx;
    (void)data;
    (void)len;
}

uint8_t Bsp_Rz_Is_Busy(uint32_t idx) {
    (void)idx;
    return 0;
}

void Bsp_Rz_Iqr_Handler(uint32_t idx) { (void)idx; }

#endif
