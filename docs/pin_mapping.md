# MSPM0G3507 引脚连接表

> 基于 `config/board_config.h` 和 `config/syscfg/ti_msp_dl_config.h` 整理。

## GPIO

| 索引 | 功能 | 方向 | 端口 | 引脚 |
|------|------|------|------|------|
| 0 | 软件按钮 (SW_BTN) | 输入 | GPIOA | PA25 |
| 1 | TFT 复位 (TFT_RST) | 输出 | GPIOA | PA16 |
| 2 | TFT 数据/命令 (TFT_DC) | 输出 | GPIOA | PA15 |
| 3 | TFT 背光 (TFT_BLK) | 输出 | GPIOA | PA14 |
| 4 | 电源指示灯 (PWR_LED) | 输出 | GPIOB | PB2 |
| 5 | 按键-上 (BTN_UP) | 输入 | GPIOB | PB18 |
| 6 | 按键-左 (BTN_LEFT) | 输入 | GPIOB | PB19 |
| 7 | 按键-下 (BTN_DOWN) | 输入 | GPIOB | PB20 |
| 8 | 按键-右 (BTN_RIGHT) | 输入 | GPIOB | PB24 |
| 9 | SPI 片选 (SPI_CS) | 输出 | GPIOB | PB17 |

## SPI1（硬件 SPI — LCD）

| 信号 | 端口 | 引脚 | IOMUX |
|------|------|------|-------|
| SCLK | GPIOB | PB16 | PINCM33 |
| MOSI (PICO) | GPIOB | PB15 | PINCM32 |
| MISO (POCI) | GPIOB | PB14 | PINCM31 |
| CS | GPIOA | PA2 | PINCM7 |

当前固件在 BSP 初始化阶段把 SPI1 串行时钟分频值设为 1，对应约 20 MHz，
用于 W25Q32 原始资源缓存和 LittleFS。LCD 仍使用下方独立的软件 SPI 引脚。

## Soft SPI（软件 SPI — LCD）

| 信号 | 端口 | 引脚 |
|------|------|------|
| SCLK | GPIOA | PA17 |
| MOSI | GPIOB | PB8 |

> 仅写入，无 MISO。配合 GPIO 的 RST(PA16)、DC(PA15)、BLK(PA14) 使用。

## UART0（调试串口）

| 信号 | 端口 | 引脚 | IOMUX |
|------|------|------|-------|
| TX | GPIOA | PA10 | PINCM21 |
| RX | GPIOA | PA11 | PINCM22 |

- 空闲检测定时器：TIMA1

## ADC0（摇杆）

| 通道 | 功能 | 端口 | 引脚 | IOMUX |
|------|------|------|------|-------|
| CH0 | 摇杆 X 轴 | GPIOA | PA27 | PINCM60 |
| CH1 | 摇杆 Y 轴 | GPIOA | PA26 | PINCM59 |

- DMA 通道：0，每次采样 10 个点

## PWM

| 索引 | 功能 | 定时器 | 通道 | 时钟频率 |
|------|------|--------|------|----------|
| 0 | 蜂鸣器 (BUZZER) | TIMA0 | CC3 | 4 MHz |
| 1 | 震动马达 (VIB_MOTOR) | TIMG0 | CC1 | 4 MHz |

## DMA 通道分配

| 通道 | 用途 |
|------|------|
| 0 | ADC0 |
| 1 | SPI1 TX |
| 2 | SPI1 RX |
| 3 | UART0 TX |
| 4 | UART0 RX |

## 引脚总览（按端口分组）

### GPIOA

| 引脚 | 功能 |
|------|------|
| PA2 | SPI1 CS |
| PA10 | UART0 TX |
| PA11 | UART0 RX |
| PA12 | 蜂鸣器 PWM（TIMA0 CC3） |
| PA14 | TFT 背光 (BLK) |
| PA15 | TFT 数据/命令 (DC) |
| PA16 | TFT 复位 (RST) |
| PA17 | Soft SPI SCLK |
| PA25 | 软件按钮 |
| PA26 | 摇杆 Y (ADC CH1) |
| PA27 | 摇杆 X (ADC CH0) |

### GPIOB

| 引脚 | 功能 |
|------|------|
| PB2 | 电源指示灯 |
| PB8 | Soft SPI MOSI |
| PB14 | SPI1 MISO |
| PB15 | SPI1 MOSI |
| PB16 | SPI1 SCLK |
| PB17 | SPI CS (手动控制) |
| PB18 | 按键-上 |
| PB19 | 按键-左 |
| PB20 | 按键-下 |
| PB24 | 按键-右 |
