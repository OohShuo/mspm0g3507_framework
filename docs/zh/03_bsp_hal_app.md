# 03 — BSP / HAL / APP 层

## BSP — 板级支持包

薄封装 DriverLib 外设。`board_config.h` 定义所有引脚映射。

| 模块 | 关键 API |
| --- | --- |
| GPIO | `Bsp_Gpio_Init`、`Write(idx, state)`、`Read(idx)`、`Toggle(idx)` |
| PWM | `Bsp_Pwm_Init`、`Set_Duty(idx, float)`、`Set_Freq(idx, Hz)`、`Start/Stop(idx)` |
| ADC | `Bsp_Adc_Init`、`Read_Voltage(idx, channel) → float`、DMA 回调 |
| 硬件 SPI | `Bsp_Hard_Spi_Init`、`Write/Read(idx, data, len)`、DMA 回调 |
| 软件 SPI | `Bsp_Soft_Spi_Init`、`Write(idx, data, len)`（位操作，仅写） |
| UART | `Bsp_Uart_Init`、`Write/Read`、`Start_Continuous_Rx`（DMA+空闲中断）、回调 |
| 时基 | `Bsp_Get_Tick_Ms() → uint32_t` |

`Bsp_Init()` 中的初始化顺序：GPIO → PWM → ADC → 硬件 SPI → 软件 SPI → UART（可选）。

## HAL — 硬件抽象层

对象式驱动，每个模块：`配置结构体 → Create(config) → Init() → Update_All()`。

### 模块

**ST7789** — LCD 240×320，软件 SPI + GPIO（DC/RST/BKL）。  
`St7789_Create`、`Init`、`Flush`、`Begin_Write/Write_Pixels/End_Write`、`Set_Backlight`

**W25Q32** — SPI NOR Flash 4 MiB，硬件 SPI + GPIO（CS）。  
`W25q32_Create`、`Init`、`Read`、`Page_Program`、`Sector_Erase`、`Block_Erase_32K/64K`、`Chip_Erase`

**Joystick（摇杆）** — 双通道 ADC（X/Y），归一化到 [-1.0, 1.0]。  
`Joystick_Create(config)`、`Calibrate_Center()`。配置：电压范围、死区、反向标志。

**Button（按键）** — GPIO 输入，软件消抖。  
`Button_Create(config)`、`Get_State() → up/down`。配置：`gpio_idx`、`gpio_state_when_pressed`。

**Buzzer（蜂鸣器）** — PWM，音符序列器，35 个预定义音效。  
`Buzzer_Create`、`Play_Sfx(idx)`、`Play(music)`、`Stop`、`Set_Volume(%)`。

**LED Simple** — GPIO 开关/闪烁。`Led_Simple_Init`、`On/Off/Toggle`、`Start_Blink/Stop_Blink`。

**LED Breath（呼吸灯）** — PWM 渐亮渐灭。`Led_Breath_Init`、`Start/Stop`。

**COM UART** — 帧协议 UART。由 `FRAMEWORK_USE_UART` 控制。供 Flash Manager 使用。

### HAL 任务

| 任务 | 周期 | 职责 |
| --- | --- | --- |
| Gpio_Task | 10ms | `Led_Simple_Update_All`、`Led_Breath_Update_All`、`Button_Update_All` |
| Buzzer_Task | 5ms | `Buzzer_Update_All` |

ST7789 和 W25Q32 不通过 `Hal_Init()` 初始化——由 APP 层按需 `Create`。

## APP — 应用层

### 游戏控制台

Game descriptor 模式，三状态状态机（菜单 → 游戏 → 结束）。通过统一接口注册游戏：

```c
typedef struct { name, icon, id, init, update, get_score, is_finished } Game_descriptor;
```

详见 [06_game_console.md](06_game_console.md)。

### 存储

W25Q32 双区：低 2 MiB 裸 Flash，高 2 MiB LittleFS。FreeRTOS 互斥锁保护。

```c
uint8_t Storage_Init(void);
uint8_t Storage_Raw_Read/Write/Erase(addr, data, size);
lfs_t* Storage_Get_Lfs(void);
void Storage_Lock/Unlock(void);
```

详见 [05_storage.md](05_storage.md)。

### Flash Manager

UART 二进制协议远程管理文件。7 个命令（READ/WRITE/DELETE/LIST/INFO/FORMAT/RESET）。任务优先级 2，队列驱动。

详见 [05_storage.md](05_storage.md)。

### LFS Port

LittleFS 块设备适配 W25Q32。静态缓冲区（256B 读 + 256B 写 + 16B 预读）。

### Image Asset

游戏素材编码/解码，支持灰度图和调色板压缩。

## 跨层依赖

| 源 | → | 目标 | 原因 |
| --- | --- | --- | --- |
| 游戏控制台 | → | 摇杆、按键、ST7789、蜂鸣器（HAL） | 输入/显示/音频 |
| 存储 | → | W25Q32（HAL） | Flash 芯片 |
| Flash Mgr | → | COM UART（HAL）、存储（APP） | UART + 文件系统 |
| ST7789 | → | 软件 SPI、GPIO（BSP） | LCD 接口 |
| W25Q32 | → | 硬件 SPI、GPIO（BSP） | Flash 接口 |
| 按键 | → | GPIO（BSP） | 数字输入 |
| 摇杆 | → | ADC（BSP） | 模拟输入 |
| 蜂鸣器 | → | PWM（BSP） | 音频输出 |
