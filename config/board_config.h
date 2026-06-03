#pragma once

/* gpio */

#define GPIO_MODE_INPUT            0
#define GPIO_MODE_OUTPUT           1
#define GPIO_MODE_INPUT_PULLUP     2
#define GPIO_MODE_INPUT_PULLDOWN   3

#define GPIO_NUM     1

#define GPIO_0_PORT  GPIOA
#define GPIO_0_PIN   DL_GPIO_PIN_14
#define GPIO_0_MODE  GPIO_MODE_OUTPUT

#define GPIO_PORTS   {GPIO_0_PORT}
#define GPIO_PINS    {GPIO_0_PIN}
#define GPIO_MODES   {GPIO_0_MODE}

#define GPIO_LED_IDX 0
