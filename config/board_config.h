#pragma once

/* gpio */

#define GPIO_MODE_INPUT  0
#define GPIO_MODE_OUTPUT 1

#define GPIO_NUM         10

#define GPIO_0_PORT      GPIOA
#define GPIO_0_PIN       DL_GPIO_PIN_25
#define GPIO_0_MODE      GPIO_MODE_INPUT

#define GPIO_1_PORT      GPIOA
#define GPIO_1_PIN       DL_GPIO_PIN_16
#define GPIO_1_MODE      GPIO_MODE_OUTPUT

#define GPIO_2_PORT      GPIOA
#define GPIO_2_PIN       DL_GPIO_PIN_15
#define GPIO_2_MODE      GPIO_MODE_OUTPUT

#define GPIO_3_PORT      GPIOA
#define GPIO_3_PIN       DL_GPIO_PIN_14
#define GPIO_3_MODE      GPIO_MODE_OUTPUT

#define GPIO_4_PORT      GPIOB
#define GPIO_4_PIN       DL_GPIO_PIN_2
#define GPIO_4_MODE      GPIO_MODE_OUTPUT

#define GPIO_5_PORT      GPIOB
#define GPIO_5_PIN       DL_GPIO_PIN_18
#define GPIO_5_MODE      GPIO_MODE_INPUT

#define GPIO_6_PORT      GPIOB
#define GPIO_6_PIN       DL_GPIO_PIN_19
#define GPIO_6_MODE      GPIO_MODE_INPUT

#define GPIO_7_PORT      GPIOB
#define GPIO_7_PIN       DL_GPIO_PIN_20
#define GPIO_7_MODE      GPIO_MODE_INPUT

#define GPIO_8_PORT      GPIOB
#define GPIO_8_PIN       DL_GPIO_PIN_24
#define GPIO_8_MODE      GPIO_MODE_INPUT

#define GPIO_9_PORT      GPIOB
#define GPIO_9_PIN       DL_GPIO_PIN_17
#define GPIO_9_MODE      GPIO_MODE_OUTPUT

#define GPIO_PORTS                                                                                           \
    {GPIO_0_PORT, GPIO_1_PORT, GPIO_2_PORT, GPIO_3_PORT, GPIO_4_PORT, GPIO_5_PORT, GPIO_6_PORT, GPIO_7_PORT, \
        GPIO_8_PORT, GPIO_9_PORT}
#define GPIO_PINS                                                                                    \
    {GPIO_0_PIN, GPIO_1_PIN, GPIO_2_PIN, GPIO_3_PIN, GPIO_4_PIN, GPIO_5_PIN, GPIO_6_PIN, GPIO_7_PIN, \
        GPIO_8_PIN, GPIO_9_PIN}
#define GPIO_MODES                                                                                           \
    {GPIO_0_MODE, GPIO_1_MODE, GPIO_2_MODE, GPIO_3_MODE, GPIO_4_MODE, GPIO_5_MODE, GPIO_6_MODE, GPIO_7_MODE, \
        GPIO_8_MODE, GPIO_9_MODE}

#define GPIO_SW_BTN_IDX        0
#define GPIO_TFT_RST_IDX       1
#define GPIO_TFT_DC_IDX        2
#define GPIO_TFT_BLK_IDX       3
#define GPIO_PWR_LED_IDX       4
#define GPIO_BNT_UP_IDX        5
#define GPIO_BNT_LEFT_IDX      6
#define GPIO_BNT_DOWN_IDX      7
#define GPIO_BNT_RIGHT_IDX     8
#define GPIO_SPI_CS_IDX        9

/* pwm */

#define PWM_NUM                2

#define PWM_0_TIMER            TIMA0
#define PWM_0_CHANNEL          DL_TIMER_CC_3_INDEX
#define PWM_0_CLK_FREQ         4000000

#define PWM_1_TIMER            TIMG0
#define PWM_1_CHANNEL          DL_TIMER_CC_1_INDEX
#define PWM_1_CLK_FREQ         4000000

#define PWM_PORTS              {PWM_0_TIMER, PWM_1_TIMER}
#define PWM_CHANNELS           {PWM_0_CHANNEL, PWM_1_CHANNEL}
#define PWM_CLK_FREQS          {PWM_0_CLK_FREQ, PWM_1_CLK_FREQ}

#define PWM_BUZZER_IDX         0
#define PWM_VIB_MOTOR_IDX      1

/* adc */

#define ADC_NUM                1

#define ADC_0_INSTANCE         ADC0
#define ADC_0_CHANNEL_NUM      2
#define ADC_0_INT_IRQN         ADC0_INT_IRQn
#define ADC_0_DMA_CHANNEL      0
#define ADC_0_DMA_TX_SIZE      10

#define ADC_PORTS              {ADC_0_INSTANCE}
#define ADC_CHANNEL_NUMS       {ADC_0_CHANNEL_NUM}
#define ADC_INT_IRQNS          {ADC_0_INT_IRQN}
#define ADC_DMA_CHANNELS       {ADC_0_DMA_CHANNEL}
#define ADC_DMA_TX_SIZES       {ADC_0_DMA_TX_SIZE}

#define ADC_JOYSTICK_IDX       0
#define ADC_JOYSTICK_X_CHANNEL 0
#define ADC_JOYSTICK_Y_CHANNEL 1

/* spi */

#define SPI_NUM                0

#define SPI_0_INSTANCE         SPI1
#define SPI_0_DMA_TX_CHANNEL   1
#define SPI_0_DMA_RX_CHANNEL   2
#define SPI_0_INT_IRQN         SPI1_INT_IRQn

#define SPI_PORTS              {SPI_0_INSTANCE}
#define SPI_DMA_TX_CHANNELS    {SPI_0_DMA_TX_CHANNEL}
#define SPI_DMA_RX_CHANNELS    {SPI_0_DMA_RX_CHANNEL}
#define SPI_INT_IRQNS          {SPI_0_INT_IRQN}

#define SPI_LCD_IDX            0
/* lcd - 1.3" TFT (ST7789) */
/* CS tied to GND on hardware */

#define LCD_RES_PORT           GPIOA
#define LCD_RES_PIN            DL_GPIO_PIN_16
#define LCD_DC_PORT            GPIOA
#define LCD_DC_PIN             DL_GPIO_PIN_15
#define LCD_BLK_PORT           GPIOA
#define LCD_BLK_PIN            DL_GPIO_PIN_14

/* soft spi - bit-bang backend for the LCD panel (and any other
 * MSB-first SPI slave that only needs MOSI). */

#define SOFT_SPI_NUM                1

#define SOFT_SPI_0_SCLK_PORT        GPIOA
#define SOFT_SPI_0_SCLK_PIN         DL_GPIO_PIN_17
#define SOFT_SPI_0_MOSI_PORT        GPIOB
#define SOFT_SPI_0_MOSI_PIN         DL_GPIO_PIN_8

#define SOFT_SPI_SCLK_PORTS         {SOFT_SPI_0_SCLK_PORT}
#define SOFT_SPI_MOSI_PORTS         {SOFT_SPI_0_MOSI_PORT}
#define SOFT_SPI_SCLK_PINS          {SOFT_SPI_0_SCLK_PIN}
#define SOFT_SPI_MOSI_PINS          {SOFT_SPI_0_MOSI_PIN}

#define SOFT_SPI_LCD_IDX            0
