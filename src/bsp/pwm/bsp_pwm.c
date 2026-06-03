#include "bsp_pwm.h"

#include "board_config.h"
#include "devices/msp/m0p/mspm0g350x.h"
#include "devices/msp/peripherals/hw_gptimer.h"
#include "dl_timera.h"
#include "dl_timerg.h"

#if PWM_NUM

struct Bsp_pwm_instances_t {
    GPTIMER_Regs* timer;
    DL_TIMER_CC_INDEX channel;
};

static struct Bsp_pwm_instances_t bsp_pwm_instances[PWM_NUM] = {0};

void Bsp_Pwm_Init(void) {
    for (uint32_t i = 0; i < PWM_NUM; i++) {
        bsp_pwm_instances[i].timer = ((GPTIMER_Regs*[])PWM_PORTS)[i];
        bsp_pwm_instances[i].channel = ((DL_TIMER_CC_INDEX[])PWM_CHANNELS)[i];
    }
}

void Bsp_Pwm_Set_Duty(uint32_t idx, float duty) {
    if (idx >= PWM_NUM) return;

    if (duty < 0) duty = 0;
    if (duty > 1) duty = 1;

    uint32_t ccr_value = (uint32_t)(DL_Timer_getLoadValue(bsp_pwm_instances[idx].timer) * duty);
    DL_Timer_setCaptureCompareValue(bsp_pwm_instances[idx].timer, ccr_value, bsp_pwm_instances[idx].channel);
}

void Bsp_Pwm_Start(uint32_t idx) {
    if (idx >= PWM_NUM) return;

    DL_Timer_startCounter(bsp_pwm_instances[idx].timer);
}

void Bsp_Pwm_Stop(uint32_t idx) {
    if (idx >= PWM_NUM) return;

    DL_Timer_stopCounter(bsp_pwm_instances[idx].timer);
}

#else

void Bsp_Pwm_Init(void) {}

void Bsp_Pwm_Set_Duty(uint32_t idx, float duty) {
    (void)idx;
    (void)duty;
}

void Bsp_Pwm_Start(uint32_t idx) { (void)idx; }

void Bsp_Pwm_Stop(uint32_t idx) { (void)idx; }

#endif