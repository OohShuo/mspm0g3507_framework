# 08 — 配置

## 配置文件

| 文件 | 范围 | 格式 |
| --- | --- | --- |
| `config.yaml` | 特性开关 | YAML |
| `board_config.h` | 引脚/外设映射 | C 宏 |
| `app_config.h` | 应用开关 | C 宏 |
| `test_config.h` | 测试开关 | C 宏 |
| `FreeRTOSConfig.h` | 内核参数 | C 宏 |
| `lvgl_config.h` | LVGL 设置 | C 宏 |
| `lfs_config.h` | LittleFS 后端 | C 宏 |
| `framework.syscfg` | TI SysConfig 工程 | SysConfig GUI |

## 特性开关（config.yaml）

```yaml
FRAMEWORK_USE_FREERTOS: ON   # RTOS 内核
FRAMEWORK_USE_LVGL: OFF      # 图形库
FRAMEWORK_USE_LFS: ON        # 文件系统
FRAMEWORK_USE_RTT: OFF       # 调试传输
FRAMEWORK_USE_UART: OFF      # UART 子系统
```

以 `#define FRAMEWORK_USE_xxx 1/0` 传递给所有源文件。禁用模块编译为空桩或由 `#if` 排除。

## 应用开关（app_config.h）

```c
#define FLASH_MGR_ENABLE 0            // Flash 远程管理
#define GAME_CONSOLE_ENABLE 1         // 游戏控制台
#define GAME_RUNTIME_MONITOR_ENABLE 1 // 堆/栈日志
```

## 板级配置（board_config.h）

所有引脚/外设映射：

```c
// GPIO：10 个引脚，带命名索引
#define GPIO_NUM 10
#define GPIO_SW_BTN_IDX  0    // 确认按键
#define GPIO_TFT_DC_IDX  2    // LCD 数据/命令
// ... 等等

// PWM：2 个通道
#define PWM_BUZZER_IDX 1

// ADC：摇杆 X/Y
#define ADC_JOYSTICK_IDX 0
#define JOYSTICK_DIRECTION_THRESHOLD ...
#define JOYSTICK_X_DEAD_ZONE ...
// ... （电压范围、偏移、反向）

// SPI：3 个实例
#define SPI_LCD_IDX      0     // 硬件 SPI → W25Q32
#define SOFT_SPI_LCD_IDX 1     // 软件 SPI → ST7789
```

## SysConfig

TI 图形工具生成 `ti_msp_dl_config.c/h`（时钟树、引脚复用、DMA）。`SYSCFG_DL_init()` 在 `main()` 中首先调用。`board_config.h` 手动维护，提供外设索引的符号名。

### 流程

```
framework.syscfg → SysConfig CLI → ti_msp_dl_config.c/h → ARM 编译
```

VM 构建跳过 SysConfig。
