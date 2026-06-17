# 03 — BSP / HAL / APP Layers

## BSP — Board Support Package

薄封装 DriverLib 外设。`board_config.h` 定义所有引脚映射。

| Module | Key API |
| --- | --- |
| GPIO | `Bsp_Gpio_Init`, `Write(idx, state)`, `Read(idx)`, `Toggle(idx)` |
| PWM | `Bsp_Pwm_Init`, `Set_Duty(idx, float)`, `Set_Freq(idx, Hz)`, `Start/Stop(idx)` |
| ADC | `Bsp_Adc_Init`, `Read_Voltage(idx, channel) → float`, DMA callback |
| Hard SPI | `Bsp_Hard_Spi_Init`, `Write/Read(idx, data, len)`, DMA callbacks |
| Soft SPI | `Bsp_Soft_Spi_Init`, `Write(idx, data, len)` (bit-banging, write-only) |
| UART | `Bsp_Uart_Init`, `Write/Read`, `Start_Continuous_Rx` (DMA+idle), callbacks |
| Time | `Bsp_Get_Tick_Ms() → uint32_t` |

Init order in `Bsp_Init()`: GPIO → PWM → ADC → Hard SPI → Soft SPI → UART (optional).

## HAL — Hardware Abstraction Layer

对象式驱动，每个模块：`Config struct → Create(config) → Init() → Update_All()`。

### Modules

**ST7789** — LCD 240×320, Soft SPI + GPIO (DC/RST/BKL).  
`St7789_Create`, `Init`, `Flush`, `Begin_Write/Write_Pixels/End_Write`, `Set_Backlight`

**W25Q32** — SPI NOR Flash 4 MiB, Hard SPI + GPIO (CS).  
`W25q32_Create`, `Init`, `Read`, `Page_Program`, `Sector_Erase`, `Block_Erase_32K/64K`, `Chip_Erase`

**Joystick** — Dual ADC (X/Y), normalized [-1.0, 1.0].  
`Joystick_Create(config)`, `Calibrate_Center()`. Config: voltage range, dead zone, reverse flags.

**Button** — GPIO input, software debounce.  
`Button_Create(config)`, `Get_State() → up/down`. Config: `gpio_idx`, `gpio_state_when_pressed`.

**Buzzer** — PWM, note sequencer, 35 pre-defined SFX.  
`Buzzer_Create`, `Play_Sfx(idx)`, `Play(music)`, `Stop`, `Set_Volume(%)`.

**LED Simple** — GPIO on/off/blink. `Led_Simple_Init`, `On/Off/Toggle`, `Start_Blink/Stop_Blink`.

**LED Breath** — PWM fading. `Led_Breath_Init`, `Start/Stop`.

**COM UART** — Framed UART protocol. Gated by `FRAMEWORK_USE_UART`. Used by Flash Manager.

### HAL Tasks

| Task | Period | Responsibility |
| --- | --- | --- |
| Gpio_Task | 10ms | `Led_Simple_Update_All`, `Led_Breath_Update_All`, `Button_Update_All` |
| Buzzer_Task | 5ms | `Buzzer_Update_All` |

ST7789 和 W25Q32 不通过 `Hal_Init()` 初始化——由 APP 层按需 `Create`。

## APP — Application Layer

### Game Console

Game descriptor 模式，3 状态状态机（Menu → Game → GameOver）。19 款游戏通过统一接口注册：

```c
typedef struct { name, icon, id, init, update, get_score, is_finished } Game_descriptor;
```

详见 [06_game_console.md](06_game_console.md)。

### Storage

W25Q32 双区：低 2 MiB Raw Flash，高 2 MiB LittleFS。FreeRTOS mutex 保护。

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

LittleFS block device 适配 W25Q32。静态 buffer（256B read + 256B prog + 16B lookahead）。

### Image Asset

游戏素材编码/解码，支持灰度图和调色板压缩。

## Cross-Layer Dependencies

| Source | → | Target | Reason |
| --- | --- | --- | --- |
| Game Console | → | Joystick, Button, ST7789, Buzzer (HAL) | Input/display/audio |
| Storage | → | W25Q32 (HAL) | Flash chip |
| Flash Mgr | → | COM UART (HAL), Storage (APP) | UART + filesystem |
| ST7789 | → | Soft SPI, GPIO (BSP) | LCD interface |
| W25Q32 | → | Hard SPI, GPIO (BSP) | Flash interface |
| Button | → | GPIO (BSP) | Digital input |
| Joystick | → | ADC (BSP) | Analog input |
| Buzzer | → | PWM (BSP) | Audio output |
