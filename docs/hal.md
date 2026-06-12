# HAL — 硬件抽象层

Hardware Abstraction Layer，在 BSP 之上封装具体的硬件驱动逻辑，向应用层提供 Create/Init/Update 模式的高级接口。

## 架构约定

每个 HAL 模块遵循统一模式：

1. `Xxx_Init()` — 子系统初始化（创建 FreeRTOS 资源、注册 BSP 回调等）
2. `Xxx_Create(config)` — 创建模块实例，返回句柄
3. `Xxx_Update_All()` — 批量轮询更新（由 FreeRTOS 任务周期调用）

## 模块列表

### LED Simple — 简易 LED

`src/hal/led_simple/led_simple.h`

单色 LED 驱动，支持常亮/闪烁两种模式。

| API | 说明 |
| --- | --- |
| `Led_Simple_Create(config)` | 创建 LED 实例 |
| `Led_Simple_Set_State(obj, state)` | 设置开/关 |
| `Led_Simple_Toggle(obj)` | 翻转状态 |
| `Led_Simple_Set_Blink_Freq(obj, freq_hz)` | 设置闪烁频率（0 = 常亮） |
| `Led_Simple_Update_All()` | 批量更新（处理闪烁计时） |

配置字段：`gpio_idx`、`use_as_indicator`、`blink_freq_hz`、`gpio_state_when_on`。

### LED Breath — 呼吸灯

`src/hal/led_breath/led_breath.h`

PWM 驱动的呼吸灯，支持频率和亮度调节。

| API | 说明 |
| --- | --- |
| `Led_Breath_Create(config)` | 创建呼吸灯实例 |
| `Led_Breath_Set_Freq(obj, freq_hz)` | 设置呼吸频率 |
| `Led_Breath_Update_All()` | 批量更新（PWM 占空比计算） |

配置字段：`pwm_idx`、`max_brightness`（0-100）、`breath_freq_hz`。

### Button — 按键

`src/hal/button/button.h`

GPIO 按键驱动，内置软件去皮抖。

| API | 说明 |
| --- | --- |
| `Button_Create(config)` | 创建按键实例 |
| `Button_Get_State(obj)` | 获取当前状态（up/down） |
| `Button_Update_All()` | 批量更新（轮询 GPIO + 去皮抖） |

### Joystick — 摇杆

`src/hal/joystick/joystick.h`

双通道 ADC 摇杆驱动，输出 -1 到 1 的归一化坐标。

| API | 说明 |
| --- | --- |
| `Joystick_Create(config)` | 创建摇杆实例，可直接读 `x_value` / `y_value` |

配置字段：`adc_idx`、`adc_channel_x`、`adc_channel_y`、`x_offset`、`y_offset`、`x_reverse`、`y_reverse`。

### Buzzer — 蜂鸣器

`src/hal/buzzer/buzzer.h`

PWM 驱动的无源蜂鸣器，内置 Music Library 支持多首曲目。

| API | 说明 |
| --- | --- |
| `Buzzer_Create(config)` | 创建蜂鸣器实例 |
| `Buzzer_Play(obj, music, is_loop)` | 播放指定曲目 |
| `Buzzer_Stop(obj)` | 停止播放 |
| `Buzzer_Update_All()` | 批量更新（音符切换、滑音处理） |

内置曲目库 `music_library[]` 通过 `Music_idx` 枚举索引（`main_theme`、`victory`、`death`、`mario`、`totoro` 等）。

### ST7789 — LCD 显示驱动

`src/hal/st7789/st7789.h`

ST7789V TFT-LCD 控制器驱动（240×240），SPI 接口，支持 LVGL flush 回调。

| API | 说明 |
| --- | --- |
| `St7789_Create(config)` | 创建并初始化 LCD 实例 |
| `St7789_Flush(obj, x1, y1, x2, y2, px_map, px_size)` | 刷新像素区域 |
| `St7789_Set_Backlight(obj, on)` | 背光开关 |
| `St7789_Register_Flush_Done_Cb(obj, cb, arg)` | 注册刷新完成回调 |
| `St7789_Send_Color(obj, cmd, cmd_len, pixels, pixels_len)` | 发送像素数据 |

### W25Q32 — SPI Flash 驱动

`src/hal/w25q32/w25q32.h`

Winbond W25Q32JV（4 MiB）SPI NOR Flash 驱动，用于 LittleFS 文件系统存储。

| API | 说明 |
| --- | --- |
| `W25q32_Create(config)` | 创建 flash 实例 |
| `W25q32_Init(obj)` | 初始化（读取 JEDEC ID） |
| `W25q32_Read(obj, addr, data, len)` | 读取数据 |
| `W25q32_Page_Program(obj, addr, data, len)` | 页写入（≤256B） |
| `W25q32_Sector_Erase(obj, addr)` | 擦除 4 KB 扇区 |
| `W25q32_Block_Erase_64K(obj, addr)` | 擦除 64 KB 块 |
| `W25q32_Chip_Erase(obj)` | 全片擦除 |
| `W25q32_Wait_Busy(obj)` | 等待芯片就绪 |

### Com UART — 协议化串口通信

`src/hal/com_uart/com_uart.h`

UART 通信框架，支持多种协议层（无协议 / 7D7E 转义 / Binary Frame CRC）。

| API | 说明 |
| --- | --- |
| `Com_Uart_Create(config)` | 创建通信实例 |
| `Com_Uart_Send(obj, data, len)` | 发送数据（协议层自动组帧） |

配置字段：`uart_idx`、`idle_timeout_ms`、`rx_max_len`、`tx_max_len`、`protocol_type`、`protocol_max_payload`、`on_rx`（RX 回调）、`on_rx_arg`。

支持的协议类型（`local_lib/protocol/protocol.h`）：
- `protocol_none` — 无协议，直通收发
- `protocol_7d7e` — 7D7E 转义帧（类 SLIP）
- `protocol_binary_frame` — SYNC+LEN+CRC16 二进制帧

### Com UDP — 以太网 UDP 通信

`src/hal/com_udp/com_udp.h`

基于 W5500 的 UDP 通信框架，API 镜像 Com UART 设计。

| API | 说明 |
| --- | --- |
| `Com_Udp_Create(cfg)` | 创建 UDP 通信实例 |
| `Com_Udp_Send(obj, data, len, dest_ip, dest_port)` | 发送 UDP 数据报 |
| `Com_Udp_Poll()` | 轮询接收 |
| `Com_Udp_Get_Src(obj, out_ip, out_port)` | 获取最近收到的数据报来源地址 |

### W5500 — 以太网芯片 HAL

`src/hal/w5500/w5500_hal.h`

Wiznet W5500 以太网控制器 HAL，通过 SPI 接口驱动，提供 SPI 互斥保护。

| API | 说明 |
| --- | --- |
| `W5500_Create(cfg)` | 创建 W5500 实例（注册 SPI/CS 回调） |
| `W5500_Reset(obj)` | 硬件复位 |
| `W5500_Get_Mutex(obj)` | 获取 SPI 互斥锁 |

## 系统入口

`src/hal/hal.h`:

| API | 说明 |
| --- | --- |
| `Hal_Init()` | 初始化所有 HAL 子系统 |
| `Hal_Task_Def()` | 创建 HAL FreeRTOS 任务（LED 刷新等） |
