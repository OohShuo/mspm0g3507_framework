#include "ti_msp_dl_config.h"
#include "bsp/time.h"

/* This results in approximately 0.5s of delay assuming 32MHz CPU_CLK */
#define DELAY (16000000)

uint32_t tt = 0;
int main(void) {
    /* Power on GPIO, initialize pins as digital outputs */
    SYSCFG_DL_init();

    /* Default: LED1 and LED3 ON, LED2 OFF */
    DL_GPIO_clearPins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_PIN);

    while (1) {
        /*
         * Call togglePins API to flip the current value of LEDs 1-3. This
         * API causes the corresponding HW bits to be flipped by the GPIO HW
         * without need for additional R-M-W cycles by the processor.
         */
        delay_cycles(DELAY);
        DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_PIN);
        tt = get_time_ms();
    }
}
