#pragma once

/* gpio */

#define GPIO_MODE_INPUT          0
#define GPIO_MODE_OUTPUT         1
#define GPIO_MODE_INPUT_PULLUP   2
#define GPIO_MODE_INPUT_PULLDOWN 3

#define GPIO_NUM                 1

#define GPIO_0_PORT              GPIOA
#define GPIO_0_PIN               DL_GPIO_PIN_15
#define GPIO_0_MODE              GPIO_MODE_OUTPUT

#define GPIO_PORTS               {GPIO_0_PORT}
#define GPIO_PINS                {GPIO_0_PIN}
#define GPIO_MODES               {GPIO_0_MODE}

#define GPIO_LED_IDX             0

/* pwm */

#define PWM_NUM                  1

#define PWM_0_TIMER              TIMG12
#define PWM_0_CHANNEL            DL_TIMER_CC_0_INDEX

#define PWM_PORTS                {PWM_0_TIMER}
#define PWM_CHANNELS             {PWM_0_CHANNEL}
