# BSP — 板级支持包

Board Support Package，封装 MSPM0G3507 板级外设的底层操作，向上层 HAL 提供统一接口。所有外设通过 `config/board_config.h` 中的宏定义映射引脚索引。

## GPIO

`src/bsp/gpio/bsp_gpio.h`

| API | 说明 |
| --- | --- |
| `Bsp_Gpio_Init()` | 初始化 GPIO 子系统 |
| `Bsp_Gpio_Write(idx, state)` | 写 GPIO 输出电平 |
| `Bsp_Gpio_Toggle(idx)` | 翻转 GPIO 输出 |
| `Bsp_Gpio_Read(idx)` | 读 GPIO 输入电平 |

状态枚举：`bsp_gpio_state_reset`（低）、`bsp_gpio_state_set`（高）、`bsp_gpio_state_err`（异常）。

## PWM

`src/bsp/pwm/bsp_pwm.h`

| API | 说明 |
| --- | --- |
| `Bsp_Pwm_Init()` | 初始化 PWM 子系统 |
| `Bsp_Pwm_Set_Duty(idx, duty)` | 设置占空比（0-100） |
| `Bsp_Pwm_Set_Freq(idx, freq_hz)` | 设置频率 |

## ADC

`src/bsp/adc/bsp_adc.h`

支持 DMA 自动采集。通过 `Bsp_Adc_Get_Value()` 读取指定通道的原始 ADC 值。

## SPI

`src/bsp/spi/bsp_spi.h`

| 子模块 | 说明 |
| --- | --- |
| `hard/bsp_hard_spi.h` | 硬件 SPI — 基于 MSPM0G3507 片上 SPI 外设 |
| `soft/bsp_soft_spi.h` | 软件 SPI — GPIO 模拟 SPI 时序，用于无硬件 SPI 或引脚受限场景 |

## UART

`src/bsp/uart/bsp_uart.h`

支持 DMA 接收、空闲中断回调、连续接收模式：

| API | 说明 |
| --- | --- |
| `Bsp_Uart_Write(idx, data, len)` | DMA 发送 |
| `Bsp_Uart_Write_Blocking(idx, data, len)` | 阻塞发送 |
| `Bsp_Uart_Read(idx, data, len)` | DMA 接收 |
| `Bsp_Uart_Start_Continuous_Rx(idx, timeout_ms, buf, max_len)` | 启动连续接收（空闲中断驱动） |
| `Bsp_Uart_Stop_Continuous_Rx(idx)` | 停止连续接收 |
| `Bsp_Uart_Wait_For_Complete(idx)` | 等待 DMA 完成 |
| `Bsp_Uart_Register_Rx_Idle_Cb(idx, cb, arg)` | 注册空闲中断回调 |

## Time

`src/bsp/time/bsp_time.h`

提供毫秒级时基，供 HAL 层去皮抖、呼吸灯、蜂鸣器等模块使用。
