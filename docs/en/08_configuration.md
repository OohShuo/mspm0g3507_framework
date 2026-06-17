# 08 — Configuration

## Config Files

| File | Scope | Format |
| --- | --- | --- |
| `config.yaml` | Feature switches | YAML |
| `board_config.h` | Pin/peripheral mapping | C macros |
| `app_config.h` | Application toggles | C macros |
| `test_config.h` | Test toggles | C macros |
| `FreeRTOSConfig.h` | Kernel parameters | C macros |
| `lvgl_config.h` | LVGL settings | C macros |
| `lfs_config.h` | LittleFS backend | C macros |
| `framework.syscfg` | TI SysConfig project | SysConfig GUI |

## Feature Switches (config.yaml)

```yaml
FRAMEWORK_USE_FREERTOS: ON   # RTOS kernel
FRAMEWORK_USE_LVGL: OFF      # Graphics library
FRAMEWORK_USE_LFS: ON        # Filesystem
FRAMEWORK_USE_RTT: OFF       # Debug transport
FRAMEWORK_USE_UART: OFF      # UART subsystem
```

Propagated as `#define FRAMEWORK_USE_xxx 1/0` to all sources. Disabled modules compile to stubs or are excluded by `#if`.

## App Toggles (app_config.h)

```c
#define FLASH_MGR_ENABLE 0            // Flash remote manager
#define GAME_CONSOLE_ENABLE 1         // Game console
#define GAME_RUNTIME_MONITOR_ENABLE 1 // Heap/stack logging
```

## Board Config (board_config.h)

All pin/peripheral mappings:

```c
// GPIO: 10 pins with named indices
#define GPIO_NUM 10
#define GPIO_SW_BTN_IDX  0    // Confirm button
#define GPIO_TFT_DC_IDX  2    // LCD data/command
// ... etc

// PWM: 2 channels
#define PWM_BUZZER_IDX 1

// ADC: joystick X/Y
#define ADC_JOYSTICK_IDX 0
#define JOYSTICK_DIRECTION_THRESHOLD ...
#define JOYSTICK_X_DEAD_ZONE ...
// ... (voltage range, offset, reverse)

// SPI: 3 instances
#define SPI_LCD_IDX      0     // Hard SPI → W25Q32
#define SOFT_SPI_LCD_IDX 1     // Soft SPI → ST7789
```

## SysConfig

TI 图形工具生成 `ti_msp_dl_config.c/h`（时钟树、引脚复用、DMA）。`SYSCFG_DL_init()` 在 `main()` 中首先调用。`board_config.h` 手动维护，提供外设索引的符号名。

### Flow

```
framework.syscfg → SysConfig CLI → ti_msp_dl_config.c/h → ARM 编译
```

VM 构建跳过 SysConfig。
