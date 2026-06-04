#pragma once

/* gpio */

#define GPIO_MODE_INPUT          0
#define GPIO_MODE_OUTPUT         1
#define GPIO_MODE_INPUT_PULLUP   2
#define GPIO_MODE_INPUT_PULLDOWN 3

#define GPIO_NUM                 2

#define GPIO_0_PORT              GPIOA
#define GPIO_0_PIN               DL_GPIO_PIN_15
#define GPIO_0_MODE              GPIO_MODE_OUTPUT

#define GPIO_1_PORT              GPIOA
#define GPIO_1_PIN               DL_GPIO_PIN_18
#define GPIO_1_MODE              GPIO_MODE_INPUT

#define GPIO_PORTS               {GPIO_0_PORT, GPIO_1_PORT}
#define GPIO_PINS                {GPIO_0_PIN, GPIO_1_PIN}
#define GPIO_MODES               {GPIO_0_MODE, GPIO_1_MODE}

#define GPIO_LED_IDX             0
#define GPIO_BUTTON_IDX          1

/* pwm */

#define PWM_NUM                  1

#define PWM_0_TIMER              TIMG12
#define PWM_0_CHANNEL            DL_TIMER_CC_0_INDEX

#define PWM_PORTS                {PWM_0_TIMER}
#define PWM_CHANNELS             {PWM_0_CHANNEL}

#define PWM_LED_IDX              0

/* adc */

#define ADC_NUM                  1

#define ADC_0_INSTANCE           ADC0
#define ADC_0_CHANNEL_NUM        2
#define ADC_0_INT_IRQN           ADC0_INT_IRQn
#define ADC_0_DMA_CHANNEL        0
#define ADC_0_DMA_TX_SIZE        2

#define ADC_PORTS                {ADC_0_INSTANCE}
#define ADC_CHANNEL_NUMS         {ADC_0_CHANNEL_NUM}
#define ADC_INT_IRQNS            {ADC_0_INT_IRQN}
#define ADC_DMA_CHANNELS         {ADC_0_DMA_CHANNEL}
#define ADC_DMA_TX_SIZES         {ADC_0_DMA_TX_SIZE}

#define ADC_JOYSTICK_IDX         0