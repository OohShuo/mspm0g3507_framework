#include "bsp_rz.h"

#include "FreeRTOS.h"
#include "board_config.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "devices/msp/peripherals/hw_gptimer.h"
#include "dl_dma.h"
#include "dl_timera.h"
#include "dl_timerg.h"
#include "task.h"

#define RZ_CACHE_BUF_SIZE        100
#define RZ_CODE_CONVERT_ONCE_NUM 6

#if RZ_NUM

struct Bsp_rz_instances_t {
    GPTIMER_Regs* timer;
    DL_TIMER_CC_INDEX pwm_channel;
    uint32_t clk_freq;
    uint32_t dma_channel;

    volatile uint8_t tx_in_progress;
    uint8_t code_data[RZ_CODE_BUF_SIZE];
    uint32_t data_len;
    uint32_t data_offset;
    uint16_t ccr_arr0[RZ_CACHE_BUF_SIZE];
    uint8_t ccr_arr0_num;
    uint16_t ccr_arr1[RZ_CACHE_BUF_SIZE];
    uint8_t ccr_arr1_num;
    uint8_t ccr_arr_inx;

    Bsp_rz_config config;
    uint32_t period_cnt;
    uint32_t one_code_ccr;
    uint32_t zero_code_ccr;
    uint32_t reset_period_num;
    TickType_t last_done_tick;
};

static struct Bsp_rz_instances_t bsp_rz_instances[RZ_NUM] = {0};

static uint32_t get_ccr_dest_addr(GPTIMER_Regs* timer, DL_TIMER_CC_INDEX channel) {
    switch (channel) {
        case DL_TIMER_CC_0_INDEX:
            return (uint32_t)&timer->COUNTERREGS.CC_01[0];
        case DL_TIMER_CC_1_INDEX:
            return (uint32_t)&timer->COUNTERREGS.CC_01[1];
        case DL_TIMER_CC_2_INDEX:
            return (uint32_t)&timer->COUNTERREGS.CC_23[0];
        case DL_TIMER_CC_3_INDEX:
            return (uint32_t)&timer->COUNTERREGS.CC_23[1];
        case DL_TIMER_CC_4_INDEX:
            return (uint32_t)&timer->COUNTERREGS.CC_45[0];
        case DL_TIMER_CC_5_INDEX:
            return (uint32_t)&timer->COUNTERREGS.CC_45[1];
        default:
            return 0;
    }
}

static uint8_t code_convert(struct Bsp_rz_instances_t* r, uint16_t* ccr_arr) {
    uint32_t remaining = r->data_len - r->data_offset;
    uint8_t byte_count =
        (remaining < RZ_CODE_CONVERT_ONCE_NUM) ? (uint8_t)remaining : RZ_CODE_CONVERT_ONCE_NUM;

    for (uint8_t b = 0; b < byte_count; b++) {
        uint8_t byte = r->code_data[r->data_offset++];
        for (int8_t bit = 7; bit >= 0; bit--) {
            *ccr_arr++ = (byte & (1 << bit)) ? (uint16_t)r->one_code_ccr : (uint16_t)r->zero_code_ccr;
        }
    }
    return byte_count * 8;
}

static void fill_ccr_buffer(struct Bsp_rz_instances_t* r, uint16_t* ccr_arr, uint8_t* ccr_arr_num) {
    *ccr_arr_num = 0;
    while (*ccr_arr_num + RZ_CODE_CONVERT_ONCE_NUM * 8 <= RZ_CACHE_BUF_SIZE) {
        if (r->data_offset >= r->data_len) break;
        uint8_t n = code_convert(r, &ccr_arr[*ccr_arr_num]);
        *ccr_arr_num += n;
    }
}

/*
 *  Configure DMA channel for a CCR buffer.
 *  The first CCR (index 0) is set manually — DMA skips it.
 */
static void dma_config_buffer(struct Bsp_rz_instances_t* r, uint16_t* ccr_arr, uint8_t ccr_arr_num) {
    uint32_t ch = r->dma_channel;
    DL_DMA_disableChannel(DMA, ch);
    DL_DMA_setSrcAddr(DMA, ch, (uint32_t)&ccr_arr[1]);
    DL_DMA_setDestAddr(DMA, ch, get_ccr_dest_addr(r->timer, r->pwm_channel));
    DL_DMA_setTransferSize(DMA, ch, ccr_arr_num - 1);
    DL_DMA_clearInterruptStatus(DMA, 1UL << ch);
}

void Bsp_Rz_Init(void) {
    for (uint32_t i = 0; i < RZ_NUM; i++) {
        bsp_rz_instances[i].timer = ((GPTIMER_Regs*[])RZ_PWM_TIMERS)[i];
        bsp_rz_instances[i].pwm_channel = ((DL_TIMER_CC_INDEX[])RZ_PWM_CHANNELS)[i];
        bsp_rz_instances[i].clk_freq = ((uint32_t[])RZ_PWM_CLK_FREQS)[i];
        bsp_rz_instances[i].dma_channel = ((uint32_t[])RZ_DMA_CHANNELS)[i];
    }
}

void Bsp_Rz_Set_Config(uint32_t idx, Bsp_rz_config* config) {
    if (idx >= RZ_NUM) return;

    struct Bsp_rz_instances_t* r = &bsp_rz_instances[idx];
    r->config = *config;

    r->period_cnt = (uint32_t)((uint64_t)r->config.period_ns * r->clk_freq / 1000000000ULL);
    r->one_code_ccr = (uint32_t)((uint64_t)r->config.one_code.high_ns * r->clk_freq / 1000000000ULL);
    r->zero_code_ccr = (uint32_t)((uint64_t)r->config.zero_code.high_ns * r->clk_freq / 1000000000ULL);

    /*
     *  RZ encoding: output must start HIGH (coded pulse) then go LOW at CCR
     *  (return to zero). Syscfg defaults to INIT_VAL_LOW — override here.
     */
    // DL_Timer_setCaptureCompareOutCtl(r->timer, DL_TIMER_CC_OCTL_INIT_VAL_HIGH,
    //                                   DL_TIMER_CC_OCTL_INV_OUT_DISABLED,
    //                                   DL_TIMER_CC_OCTL_SRC_FUNCVAL,
    //                                   r->pwm_channel);
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
    r->data_offset = 0;

    fill_ccr_buffer(r, r->ccr_arr0, &r->ccr_arr0_num);
    fill_ccr_buffer(r, r->ccr_arr1, &r->ccr_arr1_num);

    if (r->ccr_arr0_num == 0) return;

    r->tx_in_progress = 1;
    r->ccr_arr_inx = 0;

    DL_Timer_setLoadValue(r->timer, r->period_cnt);
    DL_Timer_setCaptureCompareValue(r->timer, r->ccr_arr0[0], r->pwm_channel);

    dma_config_buffer(r, r->ccr_arr0, r->ccr_arr0_num);
    DL_DMA_enableChannel(DMA, r->dma_channel);

    DL_Timer_startCounter(r->timer);
}

uint8_t Bsp_Rz_Is_Busy(uint32_t idx) {
    if (idx >= RZ_NUM) return 0;
    return bsp_rz_instances[idx].tx_in_progress;
}

void Bsp_Rz_Dma_IRQHandler(uint32_t dma_channel) {
    for (uint32_t i = 0; i < RZ_NUM; i++) {
        struct Bsp_rz_instances_t* r = &bsp_rz_instances[i];
        if (r->dma_channel != dma_channel) continue;

        if (!r->tx_in_progress) continue;

        uint32_t mask = 1UL << r->dma_channel;
        if (!DL_DMA_getEnabledInterruptStatus(DMA, mask)) continue;

        DL_DMA_clearInterruptStatus(DMA, mask);
        DL_DMA_disableChannel(DMA, r->dma_channel);

        uint8_t next_inx = r->ccr_arr_inx ^ 1;
        uint16_t* next_arr = (next_inx == 0) ? r->ccr_arr0 : r->ccr_arr1;
        uint8_t next_num = (next_inx == 0) ? r->ccr_arr0_num : r->ccr_arr1_num;

        if (next_num > 0) {
            r->ccr_arr_inx = next_inx;

            DL_Timer_setLoadValue(r->timer, r->period_cnt);
            DL_Timer_setCaptureCompareValue(r->timer, next_arr[0], r->pwm_channel);

            dma_config_buffer(r, next_arr, next_num);
            DL_DMA_enableChannel(DMA, r->dma_channel);

            uint16_t* consumed_arr = (next_inx == 0) ? r->ccr_arr1 : r->ccr_arr0;
            uint8_t* consumed_num = (next_inx == 0) ? &r->ccr_arr1_num : &r->ccr_arr0_num;
            fill_ccr_buffer(r, consumed_arr, consumed_num);
        } else {
            DL_Timer_stopCounter(r->timer);
            r->last_done_tick = xTaskGetTickCountFromISR();
            r->tx_in_progress = 0;
        }
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

void Bsp_Rz_Dma_IRQHandler(uint32_t dma_channel) { (void)dma_channel; }

uint8_t Bsp_Rz_Is_Busy(uint32_t idx) {
    (void)idx;
    return 0;
}

#endif
